#!/usr/bin/env python3
"""Unicorn QA against exact universal 1.0.151 bytes. No device I/O."""
from pathlib import Path
import struct, sys
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *
from tools.sfn_full_local_engine_segments import build_segments, TASK, MAIN, TEST, HOOK, RELAY_API, RELAY_RETURN, DATA_PTR, ACTIVE, VGSTATUS, VGSINCE, A, B, IMAGE
from tools.sfn_repack_full_local_engine_universal import decode_fwr
from tools.sfn_full_local_engine_model import decode as model_decode, NONE

ART=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.151-full-local-engine-universal-r4-a.fwr')
LOAD=0x10006000; APP=decode_fwr(ART.read_bytes())[16:-4]
SEG=build_segments(); HANDLER=SEG['handler_addr']; VGUARD=TEST; EXIT=0x100165BA
RAM=0x20000000; TASK_SP=0x20010000; SP=0x2001F000; CTX=0x20001000; TEXT=0x20018000
TOKEN=0x10016BEC; ATOI=0x10015498; TICK=0x10010408; SNPRINTF=0x1000A8CC; STRLEN=0x1000AA00; SEND=0x10018C12
FLASH_WRITE=0x100157E6; FLASH_READ=0x1001587A; MEMCPY=0x1000AA08; MEMSET=0x1000A9F0; DELAY=0x100101C6
HOOK_RETURN=0x1001291C

def cstr(uc,a,cap=256):
    out=bytearray()
    for i in range(cap):
        c=uc.mem_read(a+i,1)[0]
        if not c: break
        out.append(c)
    return bytes(out)
def ret(uc): uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
def u32(uc,a): return struct.unpack('<I',uc.mem_read(a,4))[0]
def w32(uc,a,v): uc.mem_write(a,struct.pack('<I',v&0xffffffff))

def setup(flash=None):
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x10000000,0x100000); uc.mem_write(LOAD,APP); uc.mem_map(RAM,0x20000); uc.reg_write(UC_ARM_REG_SP,SP); uc.mem_write(CTX+0x10,struct.pack('<I',77))
    state={'tick':100000,'flash':{A:bytearray(b'\xff'*IMAGE),B:bytearray(b'\xff'*IMAGE)},'send':[],'dispatch':[],'writes':[],'delay':[],'stop_delay':False,'stop_hook_return':False}
    if flash:
        for k,v in flash.items(): state['flash'][k][:]=v
    def codehook(uc,address,size,_):
        even=address&~1
        sp=uc.reg_read(UC_ARM_REG_SP)
        if even in (TOKEN,ATOI,TICK,SNPRINTF,STRLEN,SEND,FLASH_WRITE,FLASH_READ,MEMCPY,MEMSET,DELAY,RELAY_RETURN):
            if sp&7: raise AssertionError(f'unaligned stack at {even:#x}: {sp:#x}')
        if even==EXIT: uc.emu_stop(); return
        if even==HOOK_RETURN and state['stop_hook_return']: uc.emu_stop(); return
        if even==TOKEN:
            start=uc.reg_read(UC_ARM_REG_R5); data=cstr(uc,start); pos=data.find(b':'); uc.reg_write(UC_ARM_REG_R0,0 if pos<0 else start+pos); ret(uc)
        elif even==ATOI:
            t=cstr(uc,uc.reg_read(UC_ARM_REG_R0)).decode('ascii','ignore'); sign=-1 if t.startswith('-') else 1; t=t[1:] if t[:1] in '+-' else t; digs=''
            for ch in t:
                if not ch.isdigit(): break
                digs+=ch
            uc.reg_write(UC_ARM_REG_R0,(sign*int(digs or '0'))&0xffffffff); ret(uc)
        elif even==TICK: uc.reg_write(UC_ARM_REG_R0,state['tick']); ret(uc)
        elif even==SNPRINTF:
            dest=uc.reg_read(UC_ARM_REG_R0); code=uc.reg_read(UC_ARM_REG_R3); v,m=struct.unpack('<II',uc.mem_read(sp,8)); rendered=f'up:sfn:{code}:{v}:{m}\r\n'.encode(); uc.mem_write(dest,rendered+b'\0'); uc.reg_write(UC_ARM_REG_R0,len(rendered)); ret(uc)
        elif even==STRLEN: uc.reg_write(UC_ARM_REG_R0,len(cstr(uc,uc.reg_read(UC_ARM_REG_R0)))); ret(uc)
        elif even==SEND:
            a=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2); state['send'].append(bytes(uc.mem_read(a,n))); uc.reg_write(UC_ARM_REG_R0,n); ret(uc)
        elif even==MEMSET:
            a=uc.reg_read(UC_ARM_REG_R0); v=uc.reg_read(UC_ARM_REG_R1)&255; n=uc.reg_read(UC_ARM_REG_R2); uc.mem_write(a,bytes([v])*n); uc.reg_write(UC_ARM_REG_R0,a); ret(uc)
        elif even==MEMCPY:
            d=uc.reg_read(UC_ARM_REG_R0); s=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2); uc.mem_write(d,bytes(uc.mem_read(s,n))); uc.reg_write(UC_ARM_REG_R0,d); ret(uc)
        elif even==FLASH_READ:
            sector=uc.reg_read(UC_ARM_REG_R0); d=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2); uc.mem_write(d,bytes(state['flash'][sector][:n])); uc.reg_write(UC_ARM_REG_R0,0); ret(uc)
        elif even==FLASH_WRITE:
            sector=uc.reg_read(UC_ARM_REG_R0); src=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2); state['flash'][sector][:]=b'\xff'*IMAGE; state['flash'][sector][:n]=uc.mem_read(src,n); state['writes'].append((sector,n)); uc.reg_write(UC_ARM_REG_R0,0); ret(uc)
        elif even==DELAY:
            ms=uc.reg_read(UC_ARM_REG_R0); state['delay'].append(ms); state['tick']=(state['tick']+ms)&0xffffffff
            if state['stop_delay']: uc.emu_stop()
            else: ret(uc)
        elif even==RELAY_RETURN:
            cmd=uc.reg_read(UC_ARM_REG_R2); outlet=uc.reg_read(UC_ARM_REG_R0); state['dispatch'].append((cmd,outlet)); uc.reg_write(UC_ARM_REG_R0,1); ret(uc)
    uc.hook_add(UC_HOOK_CODE,codehook); return uc,state

def run_boot(uc,state):
    state['stop_delay']=True; uc.reg_write(UC_ARM_REG_SP,TASK_SP); uc.emu_start(TASK|1,0,count=20000); state['stop_delay']=False
    ptr=u32(uc,DATA_PTR); assert RAM<=ptr<RAM+0x20000,hex(ptr); return ptr

def command(uc,state,addr,line):
    bs=line.encode()+b'\0'; uc.mem_write(TEXT,bs); uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_R4,CTX); uc.reg_write(UC_ARM_REG_R5,TEXT)
    before_s=len(state['send']); before_w=len(state['writes']); uc.emu_start(addr|1,EXIT|1,count=30000); return state['send'][before_s:],state['writes'][before_w:]

def runtime_once(uc,state,ptr):
    before=len(state['dispatch']); state['stop_delay']=True; uc.reg_write(UC_ARM_REG_SP,ptr); uc.reg_write(UC_ARM_REG_R4,ptr); uc.emu_start(MAIN|1,0,count=30000); state['stop_delay']=False; return state['dispatch'][before:]

def sample_voltage(uc,state,millivolts,tick):
    state['tick']=tick; state['stop_hook_return']=True; before=len(state['dispatch']); uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_R0,millivolts); uc.reg_write(UC_ARM_REG_R2,0); uc.reg_write(UC_ARM_REG_R5,0); uc.emu_start(HOOK|1,0,count=30000); state['stop_hook_return']=False; return state['dispatch'][before:]

def main():
    uc,state=setup(); ptr=run_boot(uc,state)
    assert u32(uc,ptr+40)==0
    # Persistent voltage policy first. flags=enabled|auto_restore|(all outlets<<8); 180.0/250.0V, hysteresis5V, hold2s, recovery30s.
    flags=1|2|(15<<8); thresholds=1800|(2500<<16); timing=50|(20<<16)
    sent,writes=command(uc,state,VGUARD,f'vguard:4:{flags}:{thresholds}:{timing}:30')
    assert writes==[(A,40)],writes; assert sent==[b'up:sfn:95:4:0\r\n'],sent
    first=model_decode(bytes(state['flash'][A])); assert first and first.voltage.enabled and first.voltage.revision==4 and not first.schedules
    # Clock + one daily ON schedule, preserving voltage policy in same A/B image.
    epoch=1_800_000_000; minute=(epoch//60)%1440
    sent,_=command(uc,state,HANDLER,f'sfn:90:{epoch}:0:0:0:0:0'); assert sent==[f'up:sfn:90:{epoch}:1\r\n'.encode()],sent
    assert command(uc,state,HANDLER,'sfn:91:7:1')==([],[])
    assert command(uc,state,HANDLER,f'sfn:92:0:1:1:0:{NONE}:{minute}:1')==([],[])
    sent,writes=command(uc,state,HANDLER,'sfn:93:7'); assert writes==[(B,48)],writes; assert sent==[b'up:sfn:93:7:769\r\n'],sent
    second=model_decode(bytes(state['flash'][B])); assert second and second.schedule_generation==7 and len(second.schedules)==1 and second.voltage==first.voltage
    # Due schedule turns outlet 1 ON through the real patched common relay path.
    calls=runtime_once(uc,state,ptr); assert (11,1) in calls,calls
    # Two sustained high-voltage samples: first arms hold, second trips and turns all protected live outlets OFF.
    assert sample_voltage(uc,state,260000,101000)==[]
    offcalls=sample_voltage(uc,state,260000,104000); assert all((12,n) in offcalls for n in (1,2,3,4)),offcalls
    status=u32(uc,ptr+VGSTATUS); assert status&1,status
    # Same due ON is now vetoed inside patched common relay dispatcher: no stock dispatch reaches RELAY_RETURN.
    w32(uc,ptr+32,0xffffffff); calls=runtime_once(uc,state,ptr); assert (11,1) not in calls,calls
    # OFF remains allowed while tripped.
    gate=SEG['gate_addr']; before=len(state['dispatch']); uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_LR,EXIT|1); uc.reg_write(UC_ARM_REG_R0,12); uc.reg_write(UC_ARM_REG_R1,1); uc.emu_start(RELAY_API|1,0,count=1000); assert (12,1) in state['dispatch'][before:]
    # Safe voltage must remain stable for recovery period; then original ON mask is restored and trip clears.
    sample_voltage(uc,state,220000,105000); restores=sample_voltage(uc,state,220000,136000); assert all((11,n) in restores for n in (1,2,3,4)),restores; assert not (u32(uc,ptr+VGSTATUS)&1)
    # Cold boot from exact persisted bytes recovers both voltage policy and schedules; wall clock is intentionally resynced after device reboot.
    durable={k:bytes(v) for k,v in state['flash'].items()}; uc2,state2=setup(durable); ptr2=run_boot(uc2,state2); active=bytes(uc2.mem_read(ptr2+ACTIVE,IMAGE)); recovered=model_decode(active); assert recovered and recovered.schedule_generation==7 and recovered.voltage.revision==4 and len(recovered.schedules)==1
    # Vendor child-lock implementation and parser remain byte-identical to the physically-proven 1.0.147 base.
    from tools.sfn_build_neutral_lab_1_0_146 import decode as decode_vendor
    base=bytes(decode_vendor(Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.147-neutral-lab.fwr').read_bytes())); final=decode_fwr(ART.read_bytes()); BASE=LOAD-16
    for lo,hi in ((0x1001602E,0x1001606A),(0x10026114,0x10026230)):
        assert base[lo-BASE:hi-BASE]==final[lo-BASE:hi-BASE],(hex(lo),hex(hi))
    assert b'childlock' in final and b'up:cl:%d' in final
    print('exact_boot_flash_recovery=PASS')
    print('exact_vguard_persist_and_ack=PASS')
    print('exact_schedule_persist_and_due_execution=PASS')
    print('exact_voltage_trip_force_off=PASS')
    print('exact_common_relay_on_veto=PASS')
    print('exact_safe_recovery_restore=PASS')
    print('vendor_childlock_bytes_preserved=PASS')
    print('NO_DEVICE_IO NO_OTA NO_FLASH')
if __name__=='__main__': main()
