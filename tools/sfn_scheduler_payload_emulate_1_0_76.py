#!/usr/bin/env python3
"""QA for New/1.0.76 lazy scheduler initialization. No device I/O."""
from pathlib import Path
import hashlib
import tools.sfn_scheduler_payload_emulate as qa
from tools.sfn_build_neutral_lab_v2_1_0_66 import decode

A75=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.75-offline-scheduler-new-lab.fwr')
A76=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.76-offline-scheduler-new-lab.fwr')
EPOCH=1_800_000_000
CMD=f'sfn:90:{EPOCH}:0:0:0:0:0'
ACK=f'up:sfn:90:{EPOCH}:1\r\n'.encode()

def load(path):
    raw=path.read_bytes(); qa.RAW=bytearray(raw); qa.APP=bytes(decode(raw)[16:-4]); return raw

if __name__=='__main__':
    # Regression: exact rejected 1.0.75 cannot ACK when the vendor boot path never created the scheduler task.
    load(A75)
    uc75,state75=qa.setup(malloc_fail=True)
    assert int.from_bytes(uc75.mem_read(qa.DATA_PTR,4),'little')==0
    sent,_=qa.command(uc75,state75,CMD)
    assert sent==[],sent
    assert state75['task_create_calls']==[],state75['task_create_calls']
    print('rejected_1_0_75_without_task=PASS (no ACK reproduced)')

    # 1.0.76 must recover from the same initial condition by invoking the untouched stock task creator.
    raw=load(A76)
    uc,state=qa.setup(malloc_fail=True)
    assert int.from_bytes(uc.mem_read(qa.DATA_PTR,4),'little')==0
    sent,_=qa.command(uc,state,CMD)
    assert sent==[ACK],sent
    assert len(state['task_create_calls'])==1,state['task_create_calls']
    entry,depth,param,priority=state['task_create_calls'][0]
    assert (entry,depth,param,priority)==(qa.TASK|1,0x400,0,2),state['task_create_calls'][0]
    ptr=int.from_bytes(uc.mem_read(qa.DATA_PTR,4),'little')
    assert ptr==qa.LAZY_STATE,hex(ptr)
    assert 1000 in state['delay'],state['delay']
    print(f'lazy_first_command_ack=PASS ptr={ptr:#x} task_create=1 stack_words={depth} priority={priority}')
    # A second command must reuse the same task, then a complete durable schedule transaction must work.
    sent2,_=qa.command(uc,state,CMD)
    assert sent2==[ACK],sent2
    assert len(state['task_create_calls'])==1,state['task_create_calls']
    minute=(EPOCH//60)%1440
    assert qa.command(uc,state,'sfn:91:76:1')==( [], [] )
    assert qa.command(uc,state,f'sfn:92:0:1:1:0:65535:{minute}:1')==( [], [] )
    sent3,writes3=qa.command(uc,state,'sfn:93:76')
    assert writes3 and sent3==[b'up:sfn:93:76:769\r\n'],(sent3,writes3)
    sent4,_=qa.command(uc,state,'sfn:94')
    assert sent4==[b'up:sfn:94:76:769\r\n'],sent4
    assert len(state['task_create_calls'])==1,state['task_create_calls']
    print('lazy_full_transaction=PASS task_create_stays_1')

    # Normal task-start path remains valid and publishes only after initialization reaches runtime.
    uc2,state2=qa.setup(malloc_fail=True); ptr2,_=qa.run_boot(uc2,state2)
    assert state2['malloc_calls']==0,state2['malloc_calls']
    print(f'normal_boot_path=PASS ptr={ptr2:#x} malloc_calls=0')
    print('parser_tail_roundtrip=PASS' if qa.parser_tail_probe() else 'parser_tail_roundtrip=FAIL')
    print('artifact_sha256='+hashlib.sha256(raw).hexdigest())
    qa.main()
