#!/usr/bin/env python3
"""Strengthened QA for stack-backed New/1.0.75 scheduler. No device I/O."""
from pathlib import Path
import hashlib
import tools.sfn_scheduler_payload_emulate as qa
from tools.sfn_build_neutral_lab_v2_1_0_66 import decode
ARTIFACT=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.75-offline-scheduler-new-lab.fwr')
raw=ARTIFACT.read_bytes(); qa.RAW=bytearray(raw); qa.APP=bytes(decode(raw)[16:-4])
if __name__=='__main__':
    uc,state=qa.setup(malloc_fail=True); ptr,_=qa.run_boot(uc,state)
    assert state['malloc_calls']==0,state['malloc_calls']
    assert int.from_bytes(uc.mem_read(0x1001EE98,4),'little')==ptr
    print(f'stack_backed_state=PASS ptr={ptr:#x} malloc_calls=0')
    print('parser_tail_roundtrip=PASS' if qa.parser_tail_probe() else 'parser_tail_roundtrip=FAIL')
    print('artifact_sha256='+hashlib.sha256(raw).hexdigest())
    qa.main()
