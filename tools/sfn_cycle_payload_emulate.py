#!/usr/bin/env python3
"""Unicorn QA for the injected 1.0.148 local-cycle handler/task.

This executes only the new Thumb payload and stubs the reviewed stock APIs it calls.
It does not emulate the full device or flash anything.
"""
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *

ARTIFACT = Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.148-neutral-cycle-lab.fwr')
KEY = bytes.fromhex('3f5a27e8d8fb85f97c8397d3ad879f97')
LOAD = 0x10006000
HANDLER = 0x1001606A
TASK = 0x10016100
EXIT = 0x100165BA
RAM = 0x20000000
SP = RAM + 0x18000
CTX = RAM + 0x1000
TEXT = RAM + 0x2000
STUBS = {
    0x10016BEC: 'strchr_colon',
    0x10015498: 'atoi',
    0x10010068: 'xTaskCreate',
    0x1000A8CC: 'snprintf',
    0x1000AA00: 'strlen',
    0x10018C12: 'send',
    0x100101C6: 'delay',
    0x1001AB42: 'relay',
    0x10010182: 'task_delete',
}

raw = bytearray(ARTIFACT.read_bytes())
for i in range(16, len(raw)):
    raw[i] ^= KEY[i % 16]
APP = bytes(raw[16:-4])


def cstr(uc, address, cap=256):
    data = bytearray()
    for i in range(cap):
        c = uc.mem_read(address + i, 1)[0]
        if not c:
            break
        data.append(c)
    return bytes(data)


def ret(uc):
    uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))


def setup():
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    uc.mem_map(0x10000000, 0x100000)
    uc.mem_write(LOAD, APP)
    uc.mem_map(RAM, 0x20000)
    uc.reg_write(UC_ARM_REG_SP, SP)
    uc.mem_write(CTX + 0x10, struct.pack('<I', 77))
    uc.reg_write(UC_ARM_REG_R4, CTX)
    return uc


def run_handler(line, task_create_ok=True):
    uc = setup()
    uc.mem_write(TEXT, line.encode() + b'\0')
    uc.reg_write(UC_ARM_REG_R5, TEXT)
    calls = {'task': [], 'send': [], 'delay': [], 'relay': []}

    def hook(uc, address, size, _):
        even = address & ~1
        if even == EXIT:
            uc.emu_stop(); return
        name = STUBS.get(even)
        if not name:
            return
        if name == 'strchr_colon':
            start = uc.reg_read(UC_ARM_REG_R5)
            data = cstr(uc, start)
            pos = data.find(b':')
            uc.reg_write(UC_ARM_REG_R0, 0 if pos < 0 else start + pos)
        elif name == 'atoi':
            data = cstr(uc, uc.reg_read(UC_ARM_REG_R0)).decode('ascii', 'ignore')
            sign = -1 if data.startswith('-') else 1
            if data[:1] in '+-': data = data[1:]
            digits = ''
            for ch in data:
                if not ch.isdigit(): break
                digits += ch
            uc.reg_write(UC_ARM_REG_R0, (sign * int(digits or '0')) & 0xffffffff)
        elif name == 'xTaskCreate':
            calls['task'].append((uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R3)))
            uc.reg_write(UC_ARM_REG_R0, 1 if task_create_ok else 0)
        elif name == 'snprintf':
            dest, fmt, channel = (uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R2), uc.reg_read(UC_ARM_REG_R3))
            seconds = struct.unpack('<I', uc.mem_read(uc.reg_read(UC_ARM_REG_SP), 4))[0]
            rendered = cstr(uc, fmt).decode() % (channel, seconds)
            uc.mem_write(dest, rendered.encode() + b'\0')
            uc.reg_write(UC_ARM_REG_R0, len(rendered))
        elif name == 'strlen':
            uc.reg_write(UC_ARM_REG_R0, len(cstr(uc, uc.reg_read(UC_ARM_REG_R0))))
        elif name == 'send':
            buf, size = uc.reg_read(UC_ARM_REG_R1), uc.reg_read(UC_ARM_REG_R2)
            calls['send'].append(bytes(uc.mem_read(buf, size)))
            uc.reg_write(UC_ARM_REG_R0, size)
        ret(uc)

    uc.hook_add(UC_HOOK_CODE, hook)
    uc.emu_start(HANDLER | 1, EXIT | 1, count=500)
    return calls


def run_task(param):
    uc = setup(); uc.reg_write(UC_ARM_REG_R0, param)
    calls = {'delay': [], 'relay': [], 'deleted': False}
    def hook(uc, address, size, _):
        even = address & ~1
        name = STUBS.get(even)
        if not name: return
        if name == 'delay':
            calls['delay'].append(uc.reg_read(UC_ARM_REG_R0)); ret(uc)
        elif name == 'relay':
            calls['relay'].append((uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R1)))
            uc.reg_write(UC_ARM_REG_R0, 1); ret(uc)
        elif name == 'task_delete':
            calls['deleted'] = True; uc.emu_stop()
    uc.hook_add(UC_HOOK_CODE, hook)
    uc.emu_start(TASK | 1, 0, count=500)
    return calls


def main():
    good = run_handler('cycle:2:30')
    assert good['task'] == [(TASK | 1, (30 << 8) | 2)], good
    assert good['send'] == [b'up:cycle:2:armed:30\r\n'], good
    for bad in ('cycle:0:30', 'cycle:5:30', 'cycle:2:4', 'cycle:2:901', 'cycle:2'):
        calls = run_handler(bad)
        assert not calls['task'] and not calls['send'], (bad, calls)
    failed = run_handler('cycle:2:30', task_create_ok=False)
    assert failed['task'] and not failed['send'], failed
    task = run_task((30 << 8) | 2)
    assert task['delay'] == [100, 30000], task
    assert task['relay'] == [(12, 2), (11, 2)], task
    assert task['deleted'], task
    print('handler_valid=PASS')
    print('handler_bounds=PASS')
    print('no_ack_on_task_create_failure=PASS')
    print('task_sequence=PASS (100ms guard -> OFF -> local 30s delay -> ON -> self-delete)')

if __name__ == '__main__':
    main()
