#!/usr/bin/env python3
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import *
from tools.sfn_build_neutral_lab_1_0_146 import decode
FW=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.147-neutral-lab.fwr')
LOAD=0x10006000; FN=0x10026114; TABLE=0x100541E0
raw=decode(FW.read_bytes()); app=bytes(raw[16:-4])
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB); uc.mem_map(0x10000000,0x100000); uc.mem_write(LOAD,app)
SP=0x20001000; uc.mem_map(0x20000000,0x2000)

def run(mask):
    # restore pristine five entries from vendor image
    off=TABLE-LOAD
    uc.mem_write(TABLE,app[off:off+5*0x40])
    uc.reg_write(UC_ARM_REG_SP,SP); uc.reg_write(UC_ARM_REG_LR,0x10070001); uc.reg_write(UC_ARM_REG_R0,mask)
    # return address points inside mapped code; stop before executing it
    uc.emu_start(FN|1,0x10070000,count=200)
    return [struct.unpack('<I',uc.mem_read(TABLE+i*0x40,4))[0] for i in range(5)]
expected_unlocked=[0x1001A1C7,0x1001A1FF,0x1001A1FF,0x1001A1FF,0x1001A1FF]
for mask in (0,1,2,4,8,16,31):
    got=run(mask)
    exp=[0 if mask&(1<<i) else expected_unlocked[i] for i in range(5)]
    assert got==exp,(mask,[hex(x) for x in got],[hex(x) for x in exp])
    print(f'mask={mask:02d} callbacks='+','.join(hex(x) for x in got))
print('bit0=master_button')
print('bit1=outlet1_button')
print('bit2=outlet2_button')
print('bit3=outlet3_button')
print('bit4=outlet4_button')
print('zero=all_buttons_enabled')
print('31=all_buttons_disabled')
print('vendor_childlock_callback_semantics=PASS')
print('NO_DEVICE_IO NO_OTA NO_FLASH')
