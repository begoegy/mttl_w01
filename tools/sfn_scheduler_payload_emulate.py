#!/usr/bin/env python3
"""Unicorn QA for SFN 1.0.149 offline scheduler payload. No device I/O."""
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *
from tools.sfn_build_neutral_cycle_lab_1_0_147 import decode
from tools.sfn_scheduler_flash_model import encode as flash_encode, Item, NONE, MAX_IMAGE_SIZE, SECTOR_A, SECTOR_B
from tools.sfn_scheduler_segments import build_segments

ARTIFACT=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.149-offline-scheduler-lab.fwr')
SEG=build_segments(); LOAD=0x10006000; HANDLER=SEG['handler_addr']; TASK=SEG['task'][0]; RUNTIME=SEG['main'][0]; EXIT=0x100165BA
DATA_PTR=0x10052DA4; HEAP=0x20002000; HEAP_SIZE=0x2000; RAM=0x20000000; SP=0x2001E000; CTX=0x20001000; TEXT=0x20001800
STUBS={0x10016BEC:'colon',0x10015498:'atoi',0x10010408:'tick',0x1000A8CC:'snprintf',0x1000AA00:'strlen',0x10018C12:'send',0x100157E6:'flash_write',0x1001587A:'flash_read',0x1000AA08:'memcpy',0x1000A9F0:'memset',0x100120A0:'malloc',0x100101C6:'delay',0x1001AB42:'relay'}
RAW=bytearray(ARTIFACT.read_bytes()); APP=bytes(decode(RAW)[16:-4])

def cstr(uc,a,cap=256):
    out=bytearray()
    for i in range(cap):
        c=uc.mem_read(a+i,1)[0]
        if not c: break
        out.append(c)
    return bytes(out)
def ret(uc): uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))

def setup(flash=None):
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x10000000,0x100000); uc.mem_write(LOAD,APP)
    uc.mem_map(RAM,0x20000); uc.reg_write(UC_ARM_REG_SP,SP); uc.mem_write(CTX+0x10,struct.pack('<I',77)); uc.reg_write(UC_ARM_REG_R4,CTX)
    state={'tick':100000,'flash':{SECTOR_A:bytearray(b'\xff'*MAX_IMAGE_SIZE),SECTOR_B:bytearray(b'\xff'*MAX_IMAGE_SIZE)},'send':[],'relay':[],'delay':[],'writes':[],'stop_on_delay':False}
    if flash:
        for k,v in flash.items(): state['flash'][k][:len(v)]=v
    def hook(uc,address,size,_):
        even=address&~1
        if even==EXIT: uc.emu_stop(); return
        name=STUBS.get(even)
        if not name:return
        if name=='colon':
            start=uc.reg_read(UC_ARM_REG_R5); data=cstr(uc,start); pos=data.find(b':'); uc.reg_write(UC_ARM_REG_R0,0 if pos<0 else start+pos); ret(uc)
        elif name=='atoi':
            t=cstr(uc,uc.reg_read(UC_ARM_REG_R0)).decode('ascii','ignore'); sign=-1 if t.startswith('-') else 1; t=t[1:] if t[:1] in '+-' else t; digs=''
            for ch in t:
                if not ch.isdigit(): break
                digs+=ch
            uc.reg_write(UC_ARM_REG_R0,(sign*int(digs or '0'))&0xffffffff); ret(uc)
        elif name=='tick': uc.reg_write(UC_ARM_REG_R0,state['tick']); ret(uc)
        elif name=='snprintf':
            dest=uc.reg_read(UC_ARM_REG_R0); code=uc.reg_read(UC_ARM_REG_R3); sp=uc.reg_read(UC_ARM_REG_SP); v,m=struct.unpack('<II',uc.mem_read(sp,8)); rendered=f'up:sfn:{code}:{v}:{m}\r\n'.encode(); uc.mem_write(dest,rendered+b'\0'); uc.reg_write(UC_ARM_REG_R0,len(rendered)); ret(uc)
        elif name=='strlen': uc.reg_write(UC_ARM_REG_R0,len(cstr(uc,uc.reg_read(UC_ARM_REG_R0)))); ret(uc)
        elif name=='send':
            a=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2); state['send'].append(bytes(uc.mem_read(a,n))); uc.reg_write(UC_ARM_REG_R0,n); ret(uc)
        elif name=='memset':
            a=uc.reg_read(UC_ARM_REG_R0); v=uc.reg_read(UC_ARM_REG_R1)&255; n=uc.reg_read(UC_ARM_REG_R2); uc.mem_write(a,bytes([v])*n); uc.reg_write(UC_ARM_REG_R0,a); ret(uc)
        elif name=='memcpy':
            d=uc.reg_read(UC_ARM_REG_R0); s=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2); uc.mem_write(d,bytes(uc.mem_read(s,n))); uc.reg_write(UC_ARM_REG_R0,d); ret(uc)
        elif name=='malloc': uc.reg_write(UC_ARM_REG_R0,HEAP); ret(uc)
        elif name=='flash_read':
            sector=uc.reg_read(UC_ARM_REG_R0); d=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2); uc.mem_write(d,bytes(state['flash'][sector][:n])); uc.reg_write(UC_ARM_REG_R0,0); ret(uc)
        elif name=='flash_write':
            sector=uc.reg_read(UC_ARM_REG_R0); s=uc.reg_read(UC_ARM_REG_R1); n=uc.reg_read(UC_ARM_REG_R2); state['flash'][sector][:]=b'\xff'*MAX_IMAGE_SIZE; state['flash'][sector][:n]=uc.mem_read(s,n); state['writes'].append((sector,n)); uc.reg_write(UC_ARM_REG_R0,0); ret(uc)
        elif name=='delay':
            ms=uc.reg_read(UC_ARM_REG_R0); state['delay'].append(ms); state['tick']=(state['tick']+ms)&0xffffffff
            if state['stop_on_delay']: uc.emu_stop()
            else: ret(uc)
        elif name=='relay': state['relay'].append((uc.reg_read(UC_ARM_REG_R0),uc.reg_read(UC_ARM_REG_R1))); uc.reg_write(UC_ARM_REG_R0,1); ret(uc)
    uc.hook_add(UC_HOOK_CODE,hook); return uc,state

def run_boot(uc,state):
    state['stop_on_delay']=True; before=len(state['delay'])
    uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_R0,0)
    uc.emu_start(TASK|1,0,count=10000)
    state['stop_on_delay']=False
    ptr=struct.unpack('<I',uc.mem_read(DATA_PTR,4))[0]
    assert ptr==HEAP,(hex(ptr),hex(HEAP))
    return ptr,state['delay'][before:]

def command(uc,state,line):
    before_send=len(state['send']); before_writes=len(state['writes'])
    uc.mem_write(TEXT,line.encode()+b'\0')
    uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_R4,CTX); uc.reg_write(UC_ARM_REG_R5,TEXT)
    uc.emu_start(HANDLER|1,EXIT|1,count=10000)
    return state['send'][before_send:],state['writes'][before_writes:]

def runtime_minute(uc,state):
    before=len(state['relay']); state['stop_on_delay']=True
    uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_R4,HEAP)
    uc.emu_start(RUNTIME|1,0,count=10000)
    state['stop_on_delay']=False
    return state['relay'][before:]

def flash_copy(state):
    return {k:bytes(v) for k,v in state['flash'].items()}

def main():
    # Fresh boot: no table is active, and task reaches its local scheduler sleep safely.
    uc,state=setup(); ptr,delays=run_boot(uc,state)
    assert delays and delays[-1]==1000

    epoch=1_800_000_000
    minute=(epoch//60)%1440
    sent,writes=command(uc,state,f'sfn:90:{epoch}:0:0:0:0:0')
    assert sent==[f'up:sfn:90:{epoch}:1\r\n'.encode()],sent

    assert command(uc,state,'sfn:91:7:2')==( [], [] )
    assert command(uc,state,f'sfn:92:0:1:1:0:{NONE}:{minute}:1')==( [], [] )
    # Incomplete transaction cannot commit or touch flash.
    sent,writes=command(uc,state,'sfn:93:7'); assert not sent and not writes,(sent,writes)
    # Restart staging and send the complete generation.
    command(uc,state,'sfn:91:7:2')
    command(uc,state,f'sfn:92:0:1:1:0:{NONE}:{minute}:1')
    command(uc,state,f'sfn:92:1:1:1:0:{NONE}:{(minute+1)%1440}:0')
    sent,writes=command(uc,state,'sfn:93:7')
    assert writes==[(SECTOR_A,40)],writes
    assert sent==[b'up:sfn:93:7:770\r\n'],sent
    sent,_=command(uc,state,'sfn:94'); assert sent==[b'up:sfn:94:7:770\r\n'],sent
    durable=flash_copy(state)

    # Cold boot from flash only: table survives, clock deliberately does not.
    uc2,state2=setup(durable); run_boot(uc2,state2)
    sent,_=command(uc2,state2,'sfn:94'); assert sent==[b'up:sfn:94:7:514\r\n'],sent
    command(uc2,state2,f'sfn:90:{epoch}:0:0:0:0:0')
    sent,_=command(uc2,state2,'sfn:94'); assert sent==[b'up:sfn:94:7:770\r\n'],sent
    relays=runtime_minute(uc2,state2)
    assert relays==[(11,1)],relays

    # A valid newer B generation is used; if it is corrupted, cold boot falls back to valid A.
    command(uc2,state2,'sfn:91:8:1')
    command(uc2,state2,f'sfn:92:0:2:1:0:{NONE}:{minute}:1')
    sent,writes=command(uc2,state2,'sfn:93:8')
    assert writes==[(SECTOR_B,32)] and sent==[b'up:sfn:93:8:769\r\n'],(writes,sent)
    broken=flash_copy(state2); broken[SECTOR_B]=bytearray(broken[SECTOR_B]); broken[SECTOR_B][31]^=0x55
    uc3,state3=setup(broken); run_boot(uc3,state3)
    sent,_=command(uc3,state3,'sfn:94'); assert sent==[b'up:sfn:94:7:514\r\n'],sent

    print('clock_sync_ack=PASS')
    print('atomic_incomplete_generation=PASS')
    print('flash_commit_readback_status=PASS')
    print('cold_boot_table_recovery=PASS')
    print('cold_boot_clock_requires_resync=PASS')
    print('offline_due_relay=PASS (no server on/off command)')
    print('ab_corrupt_newest_fallback=PASS')

if __name__=='__main__': main()
