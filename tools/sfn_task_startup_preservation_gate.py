#!/usr/bin/env python3
"""Fail closed if a candidate changes the vendor task/startup window proven necessary on hardware."""
from __future__ import annotations
import argparse, hashlib
from pathlib import Path
from tools.sfn_repack_universal_fwr import decode_fwr
BASE_SHA='b75e394e77c7ec2e8598bb98d157980bd889cc06989cf8fc9702d6bfc20fba8f'
LOAD=0x10006000
START=0x10009914
END=0x10009A36

def body(path:Path)->bytes:
 raw=path.read_bytes()
 return bytes(decode_fwr(raw))

def main():
 ap=argparse.ArgumentParser(); ap.add_argument('baseline',type=Path); ap.add_argument('candidate',type=Path); a=ap.parse_args()
 if hashlib.sha256(a.baseline.read_bytes()).hexdigest()!=BASE_SHA:
  raise SystemExit('REFUSE: baseline is not exact physically-proven universal 1.0.150')
 b=body(a.baseline); c=body(a.candidate)
 off=lambda va: va-LOAD+16
 x=b[off(START):off(END)]; y=c[off(START):off(END)]
 if x!=y:
  changed=[START+i for i,(p,q) in enumerate(zip(x,y)) if p!=q]
  print(f'VENDOR_TASK_STARTUP_PRESERVATION=FAIL changed_bytes={len(changed)} first={changed[0]:#x}')
  raise SystemExit(2)
 print(f'VENDOR_TASK_STARTUP_PRESERVATION=PASS bytes={END-START} range={START:#x}..{END:#x}')
if __name__=='__main__': main()
