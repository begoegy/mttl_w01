#!/usr/bin/env python3
"""Exact branch-destination gate for SFN Full Local Engine 1.0.152 R3.

Keystone can silently wrap out-of-range CBZ/CBNZ immediates. This gate pins every
remaining compact conditional branch in reviewed custom regions to the exact
reviewed target, in addition to enforcing the architectural 0..126-byte range.
"""
from pathlib import Path
import hashlib
from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
from capstone.arm import ARM_OP_IMM
from tools.sfn_repack_universal_fwr import decode_fwr
from tools.sfn_full_local_engine_timer_segments import build_segments

ART=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.152-full-local-engine-timer-r3-a.fwr')
EXPECTED_SHA='fafa3f7e5a789bbe634e9e45c95ceb92962caad4574105c545e0a42aa1d7d872'
LOAD=0x10006000
EXPECTED={
  0x1000997A:0x10009986,0x1000999E:0x100099C2,0x100099A2:0x100099AC,
  0x10016016:0x10016022,
  0x1001EB9C:0x1001EBA4,0x1001EBA6:0x1001EBAE,0x1001EBE6:0x1001EC34,
  0x1001EBF2:0x1001EC2E,0x1001EC00:0x1001EC1A,0x1001EC36:0x1001EC42,
  0x1001ECA4:0x1001ECB0,0x1001ED74:0x1001EDA0,0x1001EDBE:0x1001EDFA,
  0x1001EDDE:0x1001EDEE,0x1001EE24:0x1001EE2C,0x1001EE32:0x1001EE4E,
  0x1001607C:0x1001608E,0x100160F8:0x10016120,0x10016102:0x10016120,
  0x100233FE:0x1002342A,0x10023412:0x1002342A,0x10023550:0x10023558,0x100235D6:0x10023628,
  0x100166F6:0x1001670A,0x100236AA:0x100236AE,0x1001EE9E:0x1001EEA6,
}

def main():
 raw=ART.read_bytes(); assert hashlib.sha256(raw).hexdigest()==EXPECTED_SHA
 app=decode_fwr(raw)[16:-4]; seg=build_segments(); md=Cs(CS_ARCH_ARM,CS_MODE_THUMB); md.detail=True
 actual={}
 for name in ('task','creator_helper','memory','main','ack','vguard','legacy','gate','timer_create','timer_get'):
  lo,blob=seg[name]
  for ins in md.disasm(app[lo-LOAD:lo-LOAD+len(blob)],lo):
   if ins.mnemonic not in ('cbz','cbnz'): continue
   target=next(op.imm for op in ins.operands if op.type==ARM_OP_IMM)
   delta=target-(ins.address+4)
   assert 0<=delta<=126,(hex(ins.address),ins.mnemonic,hex(target),delta)
   actual[ins.address]=target
 assert actual==EXPECTED,({'missing':{hex(k):hex(v) for k,v in EXPECTED.items() if actual.get(k)!=v}},
                           {'extra':{hex(k):hex(v) for k,v in actual.items() if EXPECTED.get(k)!=v}})
 print(f'EXACT_CBZ_CBNZ_TARGETS=PASS count={len(actual)} max_delta={max(v-(k+4) for k,v in actual.items())}')
 print('KEY_LONG_BRANCHES=PASS get_state_existing/fail/unlock use B<cond>, not CBZ/CBNZ')
if __name__=='__main__': main()
