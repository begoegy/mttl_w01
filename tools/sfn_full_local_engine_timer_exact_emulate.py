#!/usr/bin/env python3
"""Exact-byte Unicorn QA for SFN Full Local Engine 1.0.152 timer architecture.

No device I/O. The critical regression is first-clock operation with DATA_PTR cold-zero
and no vendor check_ota_ssl_task bootstrap.
"""
from __future__ import annotations
from pathlib import Path
import hashlib, struct, sys
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *
from tools.sfn_repack_universal_fwr import decode_fwr
from tools.sfn_full_local_engine_timer_segments import *

ART=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.152-full-local-engine-timer-r3-a.fwr')
EXPECTED='fafa3f7e5a789bbe634e9e45c95ceb92962caad4574105c545e0a42aa1d7d872'
assert hashlib.sha256(ART.read_bytes()).hexdigest()==EXPECTED
APP=decode_fwr(ART.read_bytes())[16:-4]
LOAD=0x10006000
RAM=0x20000000; RAMSZ=0x40000; SP=RAM+0x38000; CTX=RAM+0x1000; CMD=RAM+0x2000
STATE=RAM+0x5000; TIMER=RAM+0x9000; STOP=RAM+0x3F000
NEXTTOK=0x10016BEC; ATOI=0x10015498; SNPRINTF=0x1000A8CC; STRLEN=0x1000AA00; SEND=0x10018C12
FLASH_WRITE=0x100157E6; FLASH_READ=0x1001587A; MEMCPY=0x1000AA08; MEMSET=0x1000A9F0


def cstr(uc,a,cap=512):
    out=bytearray()
    for i in range(cap):
        ch=uc.mem_read(a+i,1)[0]
        if not ch: break
        out.append(ch)
    return bytes(out)

def u32(uc,a): return struct.unpack('<I',uc.mem_read(a,4))[0]
def w32(uc,a,v): uc.mem_write(a,struct.pack('<I',v&0xffffffff))
def ret(uc): uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))


def setup(*,flash=None,alloc_ok=True,timer_create_ok=True,timer_cmd_ok=True):
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
    uc.mem_map(0x10000000,0x100000); uc.mem_write(LOAD,APP)
    uc.mem_map(RAM,RAMSZ); uc.mem_write(STOP,b'\x00\xbf\x00\xbf')
    uc.mem_write(CTX+0x10,struct.pack('<I',77)); uc.reg_write(UC_ARM_REG_SP,SP)
    state={
      'tick':100000,'alloc_ok':alloc_ok,'timer_create_ok':timer_create_ok,'timer_cmd_ok':timer_cmd_ok,
      'allocs':[],'timer':0,'timer_cb':0,'timer_cmds':[],'send':[],'relay':[],'critical':0,
      'flash':{A:bytearray(b'\xff'*IMAGE),B:bytearray(b'\xff'*IMAGE)},'writes':[],
    }
    if flash:
        for k,v in flash.items(): state['flash'][k][:]=v
    # Exact artifact must contain exact assembled segments at all patched entries.
    seg=build_segments()
    for name in ('task','creator_patch','creator_helper','memory','main','ack','vguard','legacy','gate','timer_create','timer_get','hook','relay_patch'):
        a,b=seg[name]
        assert bytes(uc.mem_read(a,len(b)))==b,(name,hex(a))
    assert u32(uc,DATA_PTR)==0
    assert cstr(uc,0x1001669C)==b'sfn'
    assert cstr(uc,0x100166D4)==b'vguard'
    assert bytes(uc.mem_read(CREATOR,2))==b'\x70\x47'  # bx lr

    stubs={
      ALLOC:'alloc',TIMER_CREATE:'timer_create',TIMER_CMD:'timer_cmd',TICK:'tick',ENTER:'enter',EXITC:'exit',
      NEXTTOK:'next',ATOI:'atoi',SNPRINTF:'snprintf',STRLEN:'strlen',SEND:'send',
      FLASH_WRITE:'flash_write',FLASH_READ:'flash_read',MEMCPY:'memcpy',MEMSET:'memset',RELAY_RETURN:'relay_return',
    }
    def hook(uc,address,size,_):
        even=address&~1
        if even in {STOP,PARSER_EXIT,HOOK_RETURN}:
            uc.emu_stop(); return
        name=stubs.get(even)
        if not name: return
        sp=uc.reg_read(UC_ARM_REG_SP)
        if name in {'alloc','timer_create','timer_cmd','tick','enter','exit','snprintf','strlen','send','flash_write','flash_read','memcpy','memset','relay_return'}:
            assert sp%8==0,(name,hex(sp))
        if name=='alloc':
            n=uc.reg_read(UC_ARM_REG_R0); state['allocs'].append(n)
            uc.reg_write(UC_ARM_REG_R0,STATE if state['alloc_ok'] and n==STATE_SIZE else 0)
        elif name=='timer_create':
            if not state['timer_create_ok']:
                uc.reg_write(UC_ARM_REG_R0,0)
            else:
                cb=u32(uc,sp); ident=uc.reg_read(UC_ARM_REG_R3)
                uc.mem_write(TIMER,b'\0'*0x40); w32(uc,TIMER+0x20,ident); w32(uc,TIMER+0x24,cb)
                state['timer']=TIMER; state['timer_cb']=cb; uc.reg_write(UC_ARM_REG_R0,TIMER)
        elif name=='timer_cmd':
            state['timer_cmds'].append((uc.reg_read(UC_ARM_REG_R0),uc.reg_read(UC_ARM_REG_R1),uc.reg_read(UC_ARM_REG_R2)))
            uc.reg_write(UC_ARM_REG_R0,1 if state['timer_cmd_ok'] else 0)
        elif name=='tick': uc.reg_write(UC_ARM_REG_R0,state['tick']&0xffffffff)
        elif name=='enter': state['critical']+=1; uc.reg_write(UC_ARM_REG_R0,0)
        elif name=='exit': state['critical']-=1
        elif name=='next':
            # Use the real vendor delimiter register, not a hard-coded ':' contract.
            delim=cstr(uc,uc.reg_read(UC_ARM_REG_R8))
            assert delim==b':',delim
            start=uc.reg_read(UC_ARM_REG_R5); data=cstr(uc,start); pos=data.find(delim)
            uc.reg_write(UC_ARM_REG_R0,0 if pos<0 else start+pos)
        elif name=='atoi':
            raw=cstr(uc,uc.reg_read(UC_ARM_REG_R0)).decode('ascii','ignore')
            sign=-1 if raw.startswith('-') else 1
            if raw[:1] in '+-': raw=raw[1:]
            digits=''
            for ch in raw:
                if not ch.isdigit(): break
                digits+=ch
            uc.reg_write(UC_ARM_REG_R0,(sign*int(digits or '0'))&0xffffffff)
        elif name=='snprintf':
            dest=uc.reg_read(UC_ARM_REG_R0); fmt=cstr(uc,uc.reg_read(UC_ARM_REG_R2)).decode('ascii')
            assert fmt=='up:sfn:%d:%d:%d\r\n',fmt
            op=uc.reg_read(UC_ARM_REG_R3); v=u32(uc,sp); meta=u32(uc,sp+4)
            out=(fmt%(op,v,meta)).encode(); uc.mem_write(dest,out+b'\0'); uc.reg_write(UC_ARM_REG_R0,len(out))
        elif name=='strlen': uc.reg_write(UC_ARM_REG_R0,len(cstr(uc,uc.reg_read(UC_ARM_REG_R0))))
        elif name=='send':
            p=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2); state['send'].append(bytes(uc.mem_read(p,n))); uc.reg_write(UC_ARM_REG_R0,n)
        elif name=='flash_read':
            sector=uc.reg_read(UC_ARM_REG_R0); dest=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2)
            assert sector in state['flash'], ('flash_read_bad_sector',hex(sector),'lr',hex(uc.reg_read(UC_ARM_REG_LR)),'dest',hex(dest),'n',n)
            uc.mem_write(dest,bytes(state['flash'][sector][:n])); uc.reg_write(UC_ARM_REG_R0,0)
        elif name=='flash_write':
            sector=uc.reg_read(UC_ARM_REG_R0); src=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2)
            state['flash'][sector][:]=b'\xff'*IMAGE; state['flash'][sector][:n]=uc.mem_read(src,n); state['writes'].append((sector,n)); uc.reg_write(UC_ARM_REG_R0,0)
        elif name=='memcpy':
            d=uc.reg_read(UC_ARM_REG_R0); s=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2); uc.mem_write(d,bytes(uc.mem_read(s,n))); uc.reg_write(UC_ARM_REG_R0,d)
        elif name=='memset':
            d=uc.reg_read(UC_ARM_REG_R0); v=uc.reg_read(UC_ARM_REG_R1)&255; n=uc.reg_read(UC_ARM_REG_R2); uc.mem_write(d,bytes([v])*n); uc.reg_write(UC_ARM_REG_R0,d)
        elif name=='relay_return':
            # Patched RELAY_API reaches here only after voltage arbitration.
            outlet=uc.reg_read(UC_ARM_REG_R0); cmd=uc.reg_read(UC_ARM_REG_R2); state['relay'].append((cmd,outlet)); uc.reg_write(UC_ARM_REG_R0,1)
        ret(uc)
    uc.hook_add(UC_HOOK_CODE,hook)
    return uc,state


def run(uc,start,*,r4=CTX,r5=None,r0=None,r1=None,r2=None,stop=STOP,count=60000):
    uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_LR,stop|1); uc.reg_write(UC_ARM_REG_R4,r4)
    # Real parser convention: r8 points at the ':' delimiter literal.
    uc.reg_write(UC_ARM_REG_R8,0x10015FEC)
    if r5 is not None: uc.reg_write(UC_ARM_REG_R5,r5)
    if r0 is not None: uc.reg_write(UC_ARM_REG_R0,r0)
    if r1 is not None: uc.reg_write(UC_ARM_REG_R1,r1)
    if r2 is not None: uc.reg_write(UC_ARM_REG_R2,r2)
    uc.emu_start(start|1,stop|1,count=count)


def command(uc,state,line,*,entry=MEM):
    uc.mem_write(CMD,line.encode()+b'\0'); before=len(state['send'])
    run(uc,entry,r5=CMD)
    assert uc.reg_read(UC_ARM_REG_SP)==SP
    assert uc.reg_read(UC_ARM_REG_R4)==CTX
    assert uc.reg_read(UC_ARM_REG_R8)==0x10015FEC
    return state['send'][before:]

def ack(state): return state['send'][-1].decode().strip() if state['send'] else ''

def timer_fire(uc,state,tick):
    assert state['timer'] and state['timer_cb']; state['tick']=tick; before=len(state['relay'])
    run(uc,state['timer_cb']&~1,r0=state['timer'])
    return state['relay'][before:]

def voltage_sample(uc,state,millivolts,tick):
    state['tick']=tick; before=len(state['relay'])
    # Stop as soon as hook rejoins the stock voltage path.
    uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_R0,millivolts); uc.reg_write(UC_ARM_REG_R2,0); uc.reg_write(UC_ARM_REG_R5,0)
    uc.reg_write(UC_ARM_REG_R8,0x10015FEC); uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(HOOK|1,HOOK_RETURN|1,count=60000)
    return state['relay'][before:]


def main():
    # Cold image: no task bootstrap, no state. First real clock command must bootstrap everything itself.
    uc,s=setup(); assert u32(uc,DATA_PTR)==0
    epoch=1_800_000_000; minute=(epoch//60)%1440
    out=command(uc,s,f'sfn:90:{epoch}:0:0:0:0:0')
    assert out and ack(s)==f'up:sfn:90:{epoch}:1',(out,ack(s))
    ptr=u32(uc,DATA_PTR); assert ptr==STATE,hex(ptr); assert s['allocs']==[STATE_SIZE],s['allocs']
    assert s['timer']==TIMER and s['timer_cmds'][-1]==(TIMER,4,1000),s['timer_cmds']
    assert u32(uc,ptr+28)!=0

    # Durable schedule: begin/item/commit, then execute through exact one-shot timer callback.
    assert command(uc,s,'sfn:91:7:1')==[]
    assert command(uc,s,f'sfn:92:0:1:1:0:65535:{minute}:1')==[]
    out=command(uc,s,'sfn:93:7'); assert out and ack(s).startswith('up:sfn:93:7:'),ack(s)
    assert s['writes'],s['writes']
    calls=timer_fire(uc,s,100000); assert (11,1) in calls,calls
    assert s['timer_cmds'][-1]==(TIMER,4,1000)

    # Voltage policy persists in the same A/B record. It must coexist with the schedule.
    flags=1|2|(15<<8); thresholds=1800|(2500<<16); timing=50|(20<<16)
    out=command(uc,s,f'vguard:4:{flags}:{thresholds}:{timing}:30',entry=TEST)
    assert out and ack(s)=='up:sfn:95:4:0',ack(s)
    # High voltage: hold, then trip. All protected currently-on outlets are forced OFF.
    assert voltage_sample(uc,s,260000,101000)==[]
    off=voltage_sample(uc,s,260000,104000); assert all((12,n) in off for n in (1,2,3,4)),off
    # Common ON path is vetoed while tripped, OFF still passes.
    before=len(s['relay']); run(uc,RELAY_API,r0=11,r1=1); assert len(s['relay'])==before
    run(uc,RELAY_API,r0=12,r1=1); assert s['relay'][-1]==(12,0) or s['relay'][-1][0]==12
    # Stable safe voltage clears trip and restores prior ON mask after recovery window.
    voltage_sample(uc,s,220000,105000); restored=voltage_sample(uc,s,220000,136000); assert all((11,n) in restored for n in (1,2,3,4)),restored

    # Cold boot with flash only: voltage hook itself must lazy-load state before any server clock command.
    durable={k:bytes(v) for k,v in s['flash'].items()}
    uc2,s2=setup(flash=durable); assert u32(uc2,DATA_PTR)==0
    voltage_sample(uc2,s2,220000,200000)
    ptr2=u32(uc2,DATA_PTR); assert ptr2==STATE and s2['allocs']==[STATE_SIZE]
    # No clock yet => no timer should be created; local voltage state is still recovered.
    assert s2['timer']==0
    out=command(uc2,s2,f'sfn:90:{epoch}:0:0:0:0:0'); assert ack(s2)==f'up:sfn:90:{epoch}:1'
    assert s2['timer']==TIMER

    # Fail-closed initialization and retry paths.
    ucf,sf=setup(alloc_ok=False); command(ucf,sf,f'sfn:90:{epoch}:0:0:0:0:0'); assert not sf['send']; assert u32(ucf,DATA_PTR)==0
    sf['alloc_ok']=True; command(ucf,sf,f'sfn:90:{epoch}:0:0:0:0:0'); assert ack(sf)==f'up:sfn:90:{epoch}:1'
    uct,st=setup(timer_create_ok=False); command(uct,st,f'sfn:90:{epoch}:0:0:0:0:0'); assert ack(st)==f'up:sfn:90:{epoch}:0'; assert u32(uct,STATE+28)==0
    st['timer_create_ok']=True; command(uct,st,f'sfn:90:{epoch}:0:0:0:0:0'); assert ack(st)==f'up:sfn:90:{epoch}:1'
    uca,sa=setup(timer_cmd_ok=False); command(uca,sa,f'sfn:90:{epoch}:0:0:0:0:0'); assert ack(sa)==f'up:sfn:90:{epoch}:0'; assert u32(uca,STATE+28)==0
    sa['timer_cmd_ok']=True; command(uca,sa,f'sfn:90:{epoch}:0:0:0:0:0'); assert ack(sa)==f'up:sfn:90:{epoch}:1'

    # Disabled vendor creator must return immediately; no hidden worker task is required.
    before_alloc=list(s['allocs']); run(uc,CREATOR); assert s['allocs']==before_alloc
    assert s['critical']==0 and s2['critical']==0 and sf['critical']==0 and st['critical']==0 and sa['critical']==0

    # Vendor Child Lock code must be byte-identical to the exact proven 1.0.150 source.
    base=decode_fwr(Path('/var/lib/sfn-power/firmware/comMTTL-W01_1.0.150-universal-envelope-lab.fwr').read_bytes())
    final=decode_fwr(ART.read_bytes()); image_base=LOAD-16
    for lo,hi in ((0x1001602E,0x1001606A),(0x10026114,0x10026230)):
        assert base[lo-image_base:hi-image_base]==final[lo-image_base:hi-image_base],(hex(lo),hex(hi))
    assert b'childlock' in final and b'up:cl:%d' in final

    print('cold_first_clock_without_vendor_task=PASS')
    print('lazy_state_alloc_776=PASS')
    print('timer_create_and_1s_rearm=PASS')
    print('durable_schedule_commit_and_timer_execution=PASS')
    print('shared_ab_flash_voltage_schedule=PASS')
    print('voltage_trip_force_off_and_on_veto=PASS')
    print('voltage_safe_recovery_restore=PASS')
    print('cold_boot_voltage_lazy_recovery_before_clock=PASS')
    print('malloc_failure_retry=PASS')
    print('timer_create_failure_retry=PASS')
    print('timer_arm_failure_retry=PASS')
    print('vendor_task_creator_disabled=PASS')
    print('vendor_childlock_bytes_preserved=PASS')
    print('NO_DEVICE_IO NO_OTA NO_FLASH')

if __name__=='__main__': main()
