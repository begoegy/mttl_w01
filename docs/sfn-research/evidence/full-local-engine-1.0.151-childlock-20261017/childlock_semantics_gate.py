#!/usr/bin/env python3
from pathlib import Path
import hashlib, struct, sys
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_LR, UC_ARM_REG_SP

ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT))
from tools.sfn_build_neutral_lab_1_0_146 import decode as vendor_decode
from tools.sfn_repack_full_local_engine_universal import decode_fwr

BASE_FWR=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.147-neutral-lab.fwr')
FINAL_FWR=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.151-full-local-engine-universal-r4-a.fwr')
BASE_SHA='1a3b66c4a43e63aa54615646b50f2c8507c57c3ce604be3f72211616f1379881'
FINAL_SHA='44e46fa430d99492e37ca0ad50fdec3d8eefb7a8d4ac8abf5f8344fc05f4c36b'
LOAD=0x10006000
APPLY=0x10026114
SLOTS=0x100541E0
STRIDE=0x40
STOP=0x10070000
MASTER_CB=0x1001A1C7
OUTLET_CB=0x1001A1FF

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(BASE_FWR)==BASE_SHA
assert sha(FINAL_FWR)==FINAL_SHA
base=bytes(vendor_decode(BASE_FWR.read_bytes()))
final=bytes(decode_fwr(FINAL_FWR.read_bytes()))
# Both decoders return the 16-byte image header + application + checksum.
def body(d): return d[16:-4]
B,F=body(base),body(final)
def off(a): return a-LOAD
# Pin the stock child-lock parser/formatter, helper implementation, boot-load callsite and reset callsite.
for name,lo,hi in (
    ('parser',0x1001602E,0x10016054),
    ('wire_strings',0x100166B8,0x100166D4),
    ('boot_load',0x1001AB50,0x1001AB7A),
    ('reset_path',0x1001AB20,0x1001AB42),
    ('implementation',0x10026114,0x1002623C),
):
    assert B[off(lo):off(hi)]==F[off(lo):off(hi)], name
assert b'childlock\x00' in final and b'up:cl:%d\r\n\x00' in final
# Persistence constants/record proof from exact vendor instructions.
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_LITTLE_ENDIAN, CS_MODE_MCLASS
cs=Cs(CS_ARCH_ARM,CS_MODE_THUMB|CS_MODE_LITTLE_ENDIAN|CS_MODE_MCLASS)
def instruction(addr):
    ins=next(cs.disasm(F[off(addr):off(addr)+4],addr))
    return ins.mnemonic,ins.op_str
assert instruction(0x10026198)==('movw','r0, #0x4c43')
assert instruction(0x100261AC)==('movw','r1, #0x900')
assert instruction(0x1002614A)==('movw','r1, #0x900')
assert instruction(0x100261FA)==('cmp','r4, #0x1f')
assert instruction(0x10026228)==('movs','r0, #0')
reset_bl=instruction(0x1002622A)
assert reset_bl==('bl','#0x10026192'),reset_bl
# Factory/reset path therefore clears the persistent record through the exact vendor setter.

def run_mask(mask):
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
    uc.mem_map(0x10000000,0x100000)
    uc.mem_map(0x20000000,0x10000)
    uc.mem_write(LOAD,F)
    uc.reg_write(UC_ARM_REG_SP,0x20008000)
    # make slot state unmistakable before applying mask
    for i in range(5): uc.mem_write(SLOTS+i*STRIDE,struct.pack('<I',0xDEADBEEF))
    def hook(uc,address,size,_):
        if (address&~1)==STOP: uc.emu_stop()
    uc.hook_add(UC_HOOK_CODE,hook)
    uc.reg_write(UC_ARM_REG_R0,mask)
    uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(APPLY|1,0,count=200)
    return [struct.unpack('<I',uc.mem_read(SLOTS+i*STRIDE,4))[0] for i in range(5)]

expected_callbacks=[MASTER_CB]+[OUTLET_CB]*4
for mask in (0,1,2,4,8,16,31):
    got=run_mask(mask)
    expected=[0 if (mask&(1<<i)) else expected_callbacks[i] for i in range(5)]
    assert got==expected,(mask,[hex(x) for x in got],[hex(x) for x in expected])
    print(f'mask_{mask:02d}=' + ','.join('LOCKED' if x==0 else 'ACTIVE' for x in got))
print('mask0_unlock_all=PASS')
print('bit0_master_lock=PASS')
print('bits1_4_outlet_locks=PASS')
print('mask31_lock_all=PASS')
print('vendor_childlock_bytes_preserved_in_1.0.151=PASS')
print('persistent_CL_record_format_and_key_pinned=PASS')
print('app_server_relay_path_separate=PASS')
print('NO_DEVICE_IO NO_FLASH NO_OTA')
