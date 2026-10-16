#!/usr/bin/env python3
from pathlib import Path
import struct
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_OP_IMM, CS_OP_MEM, CS_OP_REG
from tools.sfn_build_neutral_lab_v2_1_0_66 import decode
SOURCE=Path('/var/lib/sfn-power/firmware/comMTTL-W01_1.0.71-neutral-lab-v2.fwr')
BASE=0x10006000-16; SLOT=0x1001EE98; SLOT_END=SLOT+4
D=bytes(decode(SOURCE.read_bytes()))
o=SLOT-BASE
assert D[o:o+4]==b'\x00'*4, D[o:o+4].hex()
md=Cs(CS_ARCH_ARM,CS_MODE_THUMB); md.detail=True
refs=[]
for off in range(16,len(D)-4,2):
    a=BASE+off; insns=list(md.disasm(D[off:off+4],a,count=1))
    if not insns: continue
    ins=insns[0]; target=None
    try:
        if ins.mnemonic=='adr' and len(ins.operands)>=2 and ins.operands[1].type==CS_OP_IMM:
            target=ins.operands[1].imm
        elif ins.mnemonic in ('addw','subw') and len(ins.operands)>=3 and ins.operands[1].type==CS_OP_REG and ins.reg_name(ins.operands[1].reg)=='pc' and ins.operands[2].type==CS_OP_IMM:
            pc=(a+4)&~3; disp=ins.operands[2].imm; target=pc+disp if ins.mnemonic=='addw' else pc-disp
        elif ins.mnemonic.startswith('ldr') and len(ins.operands)>=2 and ins.operands[1].type==CS_OP_MEM and ins.reg_name(ins.operands[1].mem.base)=='pc':
            lit=((a+4)&~3)+ins.operands[1].mem.disp; lo=lit-BASE
            if 0<=lo<=len(D)-4: target=struct.unpack_from('<I',D,lo)[0]
    except Exception: pass
    if target is not None and SLOT<=target<SLOT_END: refs.append((a,ins.mnemonic,ins.op_str,target))
raw=[]
for t in range(SLOT,SLOT_END):
    pat=struct.pack('<I',t); pos=0
    while True:
        x=D.find(pat,pos)
        if x<0: break
        raw.append((t,BASE+x)); pos=x+1
assert not refs, refs
assert not raw, raw
print(f'owned_state_slot=PASS address={SLOT:#x} bytes=00000000 refs=0 raw_ptrs=0')
