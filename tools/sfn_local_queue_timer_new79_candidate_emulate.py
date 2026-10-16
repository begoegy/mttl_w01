#!/usr/bin/env python3
"""Execute the exact built 1.0.78 candidate payload under Unicorn. No device I/O."""
from pathlib import Path
import runpy, struct, hashlib
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *
from tools.sfn_build_neutral_lab_v2_1_0_66 import decode

L=runpy.run_path(str(Path(__file__).with_name('sfn_local_queue_timer_new71_segments_crlf.py')),run_name='layout')
ART=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.79-local-queue-crlf-new-lab.fwr')
EXPECTED='f7dcc62cdab631ac92d55db31a22dcb56f7b6475f75b5e33f11adba0941a97ca'
assert hashlib.sha256(ART.read_bytes()).hexdigest()==EXPECTED
APP=bytes(decode(ART.read_bytes()))[16:-4]
LOAD=0x10006000; RAM=0x20000000; RAMSZ=0x30000; SP=RAM+0x28000; CTX=RAM+0x1000; TEXT=RAM+0x2000; STATE=RAM+0x4000; TIMER=RAM+0x6000; SENT=RAM+0x2F000
STUBS={
 L['ALLOC']:'alloc',L['FREE']:'free',L['TIMER_CREATE']:'timer_create',L['TIMER_CMD']:'timer_cmd',
 L['TICK']:'tick',L['ENTER']:'enter',L['EXITC']:'exit',L['NEXTTOK']:'next',L['ATOI']:'atoi',
 L['RELAY']:'relay',L['FORMAT']:'format',L['STRLEN']:'strlen',L['SEND']:'send',
}

def cstr(uc,a,cap=256):
 out=bytearray()
 for i in range(cap):
  c=uc.mem_read(a+i,1)[0]
  if not c: break
  out.append(c)
 return bytes(out)
def ret(uc): uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
def u32(uc,a): return struct.unpack('<I',uc.mem_read(a,4))[0]

def setup(timer_ok=True,timer_cmd_ok=True,relay_ok=True):
 uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x10000000,0x100000); uc.mem_write(LOAD,APP); uc.mem_map(RAM,RAMSZ)
 # Candidate bytes must already contain the reviewed machine code and data; do not overlay them.
 assert bytes(uc.mem_read(L['TEST'],len(L['test_blob'])))==L['test_blob']
 assert bytes(uc.mem_read(L['MAIN'],len(L['main_blob'])))==L['main_blob']
 assert cstr(uc,L['FMT'])==b'up:sfnq:%d:%d:%d:%d\r\n'
 assert cstr(uc,L['NAME'])==b'sfnqueue'
 assert u32(uc,L['DATA_PTR'])==0
 uc.mem_write(SENT,b'\x00\xbf\x00\xbf'); uc.mem_write(CTX+0x10,struct.pack('<I',77)); uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_R4,CTX)
 state={'tick':0,'timer_ok':timer_ok,'timer_cmd_ok':timer_cmd_ok,'relay_ok':relay_ok,'timer':0,'commands':[],'relay':[],'send':[],'allocs':[],'critical':0}
 def hook(uc,address,size,_):
  even=address&~1
  if even==SENT: uc.emu_stop(); return
  name=STUBS.get(even)
  if not name:return
  if name=='alloc':
   n=uc.reg_read(UC_ARM_REG_R0); state['allocs'].append(n); uc.reg_write(UC_ARM_REG_R0,STATE if n==256 else RAM+0x8000)
  elif name=='free': pass
  elif name=='timer_create':
   if not state['timer_ok']: uc.reg_write(UC_ARM_REG_R0,0)
   else:
    cb=u32(uc,uc.reg_read(UC_ARM_REG_SP)); ident=uc.reg_read(UC_ARM_REG_R3)
    uc.mem_write(TIMER,b'\0'*0x28); uc.mem_write(TIMER+0x20,struct.pack('<I',ident)); uc.mem_write(TIMER+0x24,struct.pack('<I',cb)); state['timer']=TIMER; state['timer_callback']=cb; uc.reg_write(UC_ARM_REG_R0,TIMER)
  elif name=='timer_cmd':
   state['commands'].append((uc.reg_read(UC_ARM_REG_R1),uc.reg_read(UC_ARM_REG_R2)))
   uc.reg_write(UC_ARM_REG_R0,1 if state['timer_cmd_ok'] else 0)
  elif name=='tick': uc.reg_write(UC_ARM_REG_R0,state['tick']&0xffffffff)
  elif name=='enter': state['critical']+=1; uc.reg_write(UC_ARM_REG_R0,0)
  elif name=='exit': state['critical']-=1
  elif name=='next':
   start=uc.reg_read(UC_ARM_REG_R5); data=cstr(uc,start); pos=data.find(b':'); uc.reg_write(UC_ARM_REG_R0,0 if pos<0 else start+pos)
  elif name=='atoi':
   s=cstr(uc,uc.reg_read(UC_ARM_REG_R0)).decode('ascii','ignore'); sign=-1 if s.startswith('-') else 1
   if s[:1] in '+-':s=s[1:]
   ds=''
   for ch in s:
    if not ch.isdigit():break
    ds+=ch
   uc.reg_write(UC_ARM_REG_R0,(sign*int(ds or '0'))&0xffffffff)
  elif name=='relay':
   state['relay'].append((uc.reg_read(UC_ARM_REG_R0),uc.reg_read(UC_ARM_REG_R1))); uc.reg_write(UC_ARM_REG_R0,1 if state['relay_ok'] else 0)
  elif name=='format':
   dest=uc.reg_read(UC_ARM_REG_R0); fmt=cstr(uc,uc.reg_read(UC_ARM_REG_R2)).decode(); op=uc.reg_read(UC_ARM_REG_R3); sp=uc.reg_read(UC_ARM_REG_SP); vals=[u32(uc,sp+4*i) for i in range(3)]
   out=(fmt%(op,*vals)).encode(); uc.mem_write(dest,out+b'\0'); uc.reg_write(UC_ARM_REG_R0,len(out))
  elif name=='strlen': uc.reg_write(UC_ARM_REG_R0,len(cstr(uc,uc.reg_read(UC_ARM_REG_R0))))
  elif name=='send':
   p,n=uc.reg_read(UC_ARM_REG_R1),uc.reg_read(UC_ARM_REG_R2); state['send'].append(bytes(uc.mem_read(p,n))); uc.reg_write(UC_ARM_REG_R0,n)
  ret(uc)
 uc.hook_add(UC_HOOK_CODE,hook)
 return uc,state

def run_handler(uc,state,line):
 uc.mem_write(TEXT,line.encode()+b'\0'); uc.reg_write(UC_ARM_REG_R5,TEXT); uc.reg_write(UC_ARM_REG_R4,CTX); uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_LR,SENT|1)
 before=len(state['send']); uc.emu_start(L['HANDLER']|1,SENT|1,count=5000); return state['send'][before:]
def fire(uc,state,tick=None):
 if tick is not None:state['tick']=tick
 uc.reg_write(UC_ARM_REG_R0,TIMER); uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_LR,SENT|1); before=len(state['relay']); uc.emu_start(L['CALLBACK']|1,SENT|1,count=5000); return state['relay'][before:]

def last_ack(state): return state['send'][-1].decode().strip() if state['send'] else ''

def run_wire(uc,state,line):
 return run_handler(uc,state,line+'\r\n')

def main():
 uc,s=setup();
 run_wire(uc,s,'sfnq:4'); assert last_ack(s)=='up:sfnq:4:0:0:0'; assert s['allocs']==[256] and s['timer']
 run_wire(uc,s,'sfnq:1:7:2'); run_wire(uc,s,'sfnq:2:0:5:1:0'); run_wire(uc,s,'sfnq:2:1:10:2:1'); s['tick']=1000; run_wire(uc,s,'sfnq:3:7')
 assert last_ack(s)=='up:sfnq:3:7:2:0',last_ack(s); assert s['commands'][-1]==(4,5000),s['commands']
 # Incomplete replacement leaves active generation 7.
 run_wire(uc,s,'sfnq:1:8:2'); run_wire(uc,s,'sfnq:2:0:30:3:1'); run_wire(uc,s,'sfnq:4'); assert last_ack(s)=='up:sfnq:4:7:2:0'
 # Old active event still fires while replacement is incomplete.
 assert fire(uc,s,6000)==[(12,1)]; assert s['commands'][-1]==(4,5000)
 # Complete replacement at tick 7000; stale callback from old timer must not relay early.
 run_wire(uc,s,'sfnq:1:8:1'); run_wire(uc,s,'sfnq:2:0:30:3:1'); s['tick']=7000; run_wire(uc,s,'sfnq:3:8'); assert last_ack(s)=='up:sfnq:3:8:1:0'; before=len(s['relay']); fire(uc,s,7001); assert len(s['relay'])==before; assert s['commands'][-1][0]==4 and s['commands'][-1][1]>=29999
 # New event fires offline/local at due tick.
 assert fire(uc,s,37000)==[(11,3)]; run_wire(uc,s,'sfnq:4'); assert last_ack(s)=='up:sfnq:4:8:1:1'
 # Duplicate commit is idempotent and acknowledges current generation.
 run_wire(uc,s,'sfnq:3:8'); assert last_ack(s)=='up:sfnq:3:8:1:1'
 # Malformed/out-of-order staging fails closed without replacing active.
 run_wire(uc,s,'sfnq:1:9:2'); run_wire(uc,s,'sfnq:2:1:1:1:1'); assert last_ack(s).startswith('up:sfnq:0:8:1:')
 run_wire(uc,s,'sfnq:4'); assert last_ack(s)=='up:sfnq:4:8:1:1'
 # Strict numeric parsing rejects prefixes, empty fields and >9 digits.
 for bad in ('sfnq:1:10x:1','sfnq:1::1','sfnq:1:1000000000:1','sfnq:2:0:5z:1:1'):
  run_wire(uc,s,bad); assert last_ack(s).startswith('up:sfnq:0:8:1:'),(bad,last_ack(s))
 # Cap is eight, not customer lifetime limit.
 run_wire(uc,s,'sfnq:1:10:9'); assert last_ack(s).startswith('up:sfnq:0:8:1:')
 # Timer creation failure never publishes state and returns an explicit error ACK.
 ucf,sf=setup(timer_ok=False); run_wire(ucf,sf,'sfnq:4'); assert last_ack(sf)=='up:sfnq:0:0:0:0'; assert u32(ucf,L['DATA_PTR'])==0
 # Timer-command failure is reported in status meta high byte, no relay side effect.
 uc2,s2=setup(timer_cmd_ok=False); run_wire(uc2,s2,'sfnq:1:1:1'); run_wire(uc2,s2,'sfnq:2:0:5:1:1'); run_wire(uc2,s2,'sfnq:3:1'); assert last_ack(s2)=='up:sfnq:3:1:1:256',last_ack(s2); assert not s2['relay']
 # Relay rejection is recorded as error=2, index still advances exactly once.
 uc3,s3=setup(relay_ok=False); run_wire(uc3,s3,'sfnq:1:1:1'); run_wire(uc3,s3,'sfnq:2:0:1:4:1'); run_wire(uc3,s3,'sfnq:3:1'); fire(uc3,s3,1000); run_wire(uc3,s3,'sfnq:4'); assert last_ack(s3)=='up:sfnq:4:1:1:513',last_ack(s3)
 # Cold power loss resets RAM queue: fresh boot reports generation zero.
 uc4,s4=setup(); run_wire(uc4,s4,'sfnq:4'); assert last_ack(s4)=='up:sfnq:4:0:0:0'
 assert s['allocs']==[256],s['allocs']
 assert all(x['critical']==0 for x in (s,sf,s2,s3,s4))
 # The new architecture must never call/create a queue worker task.
 from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB,CS_MODE_LITTLE_ENDIAN
 cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB|CS_MODE_LITTLE_ENDIAN)
 targets=[]
 for base,blob in ((L['TEST'],L['test_blob']),(L['MAIN'],L['main_blob'])):
  for ins in cs.disasm(blob,base):
   if ins.mnemonic=='bl' and ins.op_str.startswith('#0x'): targets.append(int(ins.op_str[1:],16))
 assert 0x10010068 not in targets,targets
 print('real_wire_crlf_parser=PASS')
 print('timer_queue_machine_code=PASS')
 print('atomic_incomplete_replace=PASS')
 print('stale_timer_after_replace=PASS')
 print('offline_local_execution=PASS')
 print('duplicate_commit=PASS')
 print('malformed_fail_closed=PASS')
 print('strict_numeric_parser=PASS')
 print('single_state_single_timer=PASS')
 print('no_new_worker_task=PASS')
 print('timer_create_failure=PASS')
 print('timer_failure_status=PASS')
 print('relay_failure_status=PASS')
 print('cold_boot_queue_loss=PASS')
 print('acks=',len(s['send']),'relay=',s['relay'])
if __name__=='__main__':main()
