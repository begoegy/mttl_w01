#!/usr/bin/env python3
"""Run strengthened scheduler Unicorn QA against New/1.0.73 candidate. No device I/O."""
from pathlib import Path
import hashlib
import tools.sfn_scheduler_payload_emulate as qa
from tools.sfn_build_neutral_lab_v2_1_0_66 import decode
ARTIFACT=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.73-offline-scheduler-new-lab.fwr')
raw=ARTIFACT.read_bytes()
qa.RAW=bytearray(raw)
qa.APP=bytes(decode(raw)[16:-4])
if __name__=='__main__':
    print('artifact_sha256='+hashlib.sha256(raw).hexdigest())
    qa.main()
