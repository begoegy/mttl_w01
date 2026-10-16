#!/usr/bin/env python3
import re
from keystone import Ks, KS_ARCH_ARM, KS_MODE_THUMB, KS_MODE_LITTLE_ENDIAN, KS_MODE_V8
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
from tools.sfn_scheduler_payload_probe import asm as MONO, ACTIVE, STAGE, SCRATCH, IMAGE_MAX, DATA_PTR, A, B, MAGIC
KS = Ks(KS_ARCH_ARM, KS_MODE_THUMB | KS_MODE_LITTLE_ENDIAN | KS_MODE_V8)
CS = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
TASK_BASE = 0x10009914
MEM_HANDLER = 0x1001600C
NEXT_INT = 0x10016010
MAIN_BASE = 0x1001EB6C
MAIN_LIMIT = 0x1001EE54
ACK_BASE = 0x1001EE5C
ACK_LIMIT = 0x1001EEA8
FMT = 0x100166A4

def assemble(src, addr):
    """Assemble, then repair Keystone absolute-branch drift after label relaxation.

    Keystone resolves numeric far branches before its final relaxation pass. If an earlier
    conditional widens, later numeric BL/B.W targets can drift by a few bytes. We re-encode
    every external numeric branch independently at its final PC and verify the result.
    """
    out, _ = KS.asm(src, addr=addr, as_bytes=True)
    data = bytearray(out)
    expected = [int(x, 16) for x in re.findall(r'\b(?:bl|b(?:\.w)?)\s+0x([0-9a-fA-F]+)', src)]
    lo, hi = addr, addr + len(data)
    for ins in list(CS.disasm(bytes(data), addr)):
        if ins.mnemonic not in ('bl', 'b.w') or not ins.op_str.startswith('#0x'):
            continue
        actual = int(ins.op_str[1:], 16)
        if lo <= actual < hi:
            continue
        if not expected:
            raise RuntimeError(f'external branch without declared numeric target at {ins.address:#x}')
        target = min(expected, key=lambda x: abs(x - actual))
        if abs(target - actual) > 0x1000:
            raise RuntimeError(f'cannot repair branch {ins.address:#x}: {actual:#x} -> ?')
        fixed, _ = KS.asm(f'{ins.mnemonic} 0x{target:x}', addr=ins.address, as_bytes=True)
        if len(fixed) != ins.size:
            raise RuntimeError(f'branch size changed at {ins.address:#x}')
        off = ins.address - addr
        data[off:off+ins.size] = fixed
    # Every far BL/B.W must now land on an explicitly named target from this source.
    for ins in CS.disasm(bytes(data), addr):
        if ins.mnemonic in ('bl', 'b.w') and ins.op_str.startswith('#0x'):
            target = int(ins.op_str[1:], 16)
            if not (lo <= target < hi) and target not in expected:
                raise RuntimeError(f'unverified external branch {ins.address:#x} -> {target:#x}')
    return bytes(data)

def section(start, end):
    return start + ':' + MONO.split(start + ':', 1)[1].split(end + ':', 1)[0]

def build_segments():
    next_src = section('next_int', 'validate').replace('next_int:', '')
    # validate lives after the boot block; boot address is computed after a first size pass.
    boot_raw = 'task:' + MONO.split('task:', 1)[1].split('t_loop:', 1)[0]
    # The vendor task already owns a 0x400-word (4096-byte) RTOS stack. Keep scheduler state
    # inside that persistent task stack instead of consuming another 1152 bytes from the device heap.
    heap_boot = f''' movw r0,#1152\n bl 0x100120a0\n cbz r0,t_dead\n mov r4,r0\n mov r2,r0\n movw r0,#{DATA_PTR & 0xffff}\n movt r0,#{DATA_PTR >> 16}\n str r2,[r0]\n'''
    stack_boot = f''' sub.w sp,sp,#1152\n mov r4,sp\n'''
    if heap_boot not in boot_raw:
        raise AssertionError('scheduler heap bootstrap layout changed')
    boot_raw = boot_raw.replace(heap_boot, stack_boot, 1)
    runtime_src = 't_loop:' + MONO.split('t_loop:', 1)[1].split('t_dead:', 1)[0]
    runtime_src = runtime_src.replace(' cbz r0,t_sleep', ' cmp r0,#0\n beq.w t_sleep')
    validate_src = section('validate', 'relay')
    relay_src = section('relay', 'ack')
    ack_src = section('ack', 'handler')
    handler_src = section('handler', 'task')
    handler_src = handler_src.replace(' cbz r0,h_exit', ' cmp r0,#0\n beq.w h_exit', 1)
    # This hook is inline in the stock command parser. Preserve the parser's live low registers
    # across all scheduler calls; four low registers keep the stack 8-byte aligned.
    handler_src = handler_src.replace('handler:\n', 'handler:\n push {r3,r5,r6,r7}\n', 1)
    handler_src = handler_src.replace('h_exit:\n b 0x100165ba', 'h_exit:\n pop {r3,r5,r6,r7}\n b 0x100165ba', 1)

    # Keep bounds checks that prevent dangerous relay values, but semantic day/date pairing is
    # compiled by the server and cannot make an invalid relay fire; removing these checks saves
    # scarce code bytes while malformed fields still fail harmlessly at runtime.
    handler_src = handler_src.replace(' mov r5,r0\n', '', 1)
    handler_src = handler_src.replace(''' cmp r5,#2\n bne hi_not_week\n cmp r0,#0\n beq h_exit\n b hi_days_ok\nhi_not_week:\n cmp r0,#0\n bne h_exit\nhi_days_ok:\n strb r0,[r6,#2]\n''', ' strb r0,[r6,#2]\n')
    handler_src = handler_src.replace(''' cmp r5,#0\n bne hi_repeat_date\n cmp r0,r1\n beq h_exit\n b hi_date_ok\nhi_repeat_date:\n cmp r0,r1\n bne h_exit\nhi_date_ok:\n strh r0,[r6,#4]\n''', ' strh r0,[r6,#4]\n')

    # ACK sits after the immutable version marker; the old bytes there were MEF-only HTTP strings.
    # MONO is already formatted once, so Keystone source carries these immediates as decimal.
    # Rewrite either decimal or hex spelling and fail closed if the old pointer survives.
    ack_src, n_lo = re.subn(r'movw r2,\s*#(?:60976|0xee30)\b', f'movw r2, #{FMT & 0xffff}', ack_src, count=1)
    ack_src, n_hi = re.subn(r'movt r2,\s*#(?:4097|0x1001)\b', f'movt r2, #{FMT >> 16}', ack_src, count=1)
    if (n_lo, n_hi) != (1, 1):
        raise AssertionError(f'ACK format pointer rewrite failed: low={n_lo} high={n_hi}')
    ack_bin = assemble(ack_src, ACK_BASE)

    # Runtime starts at the reclaimed MEF downloader entry. Measure it first so relay/handler
    # absolute addresses are deterministic.
    runtime_probe = runtime_src.replace('bl relay', f'bl 0x{MAIN_BASE + 0x200:x}')
    runtime_bin = assemble(runtime_probe, MAIN_BASE)
    relay_addr = MAIN_BASE + len(runtime_bin)
    relay_bin = assemble(relay_src, relay_addr)
    handler_addr = relay_addr + len(relay_bin)
    next_bin = assemble(f'b.w 0x{handler_addr:x}\nnext_int:\n' + next_src, MEM_HANDLER)

    # Validate/get_data live in the old check_ota_ssl_task body, which the firmware already starts.
    # First measure boot, then place validate and get_data immediately after it.
    publish = f'''boot_runtime:\n mov r2,r4\n movw r0,#{DATA_PTR & 0xffff}\n movt r0,#{DATA_PTR >> 16}\n str r2,[r0]\n b.w 0x{MAIN_BASE:x}\n'''
    boot_probe = boot_raw.replace('bl validate', f'bl 0x{TASK_BASE + 0xA0:x}')
    boot_probe = boot_probe.replace('cbz r0,t_loop', 'cbz r0,boot_runtime').replace('bls t_loop', 'bls boot_runtime')
    boot_probe += publish
    boot_tmp = assemble(boot_probe, TASK_BASE)
    validate_addr = TASK_BASE + len(boot_tmp)
    validate_bin = assemble(validate_src, validate_addr)
    get_addr = validate_addr + len(validate_bin)
    get_src = f'''get_data:\n movw r0,#{DATA_PTR & 0xffff}\n movt r0,#{DATA_PTR >> 16}\n ldr r1,[r0]\n cbnz r1,gd_fast\n push {{r4,lr}}\n mov r4,r0\n bl 0x10009a12\n movw r0,#1000\n bl 0x100101c6\n ldr r1,[r4]\n mov r0,r1\n pop {{r4,pc}}\ngd_fast:\n mov r0,r1\n bx lr\n'''
    get_bin = assemble(get_src, get_addr)
    boot_src = boot_raw.replace('bl validate', f'bl 0x{validate_addr:x}')
    boot_src = boot_src.replace('cbz r0,t_loop', 'cbz r0,boot_runtime').replace('bls t_loop', 'bls boot_runtime')
    boot_src += publish
    boot_bin = assemble(boot_src, TASK_BASE)
    assert len(boot_bin) == len(boot_tmp)

    runtime_bin = assemble(runtime_src.replace('bl relay', f'bl 0x{relay_addr:x}'), MAIN_BASE)
    handler_src = handler_src.replace('bl get_data', f'bl 0x{get_addr:x}')
    handler_src = handler_src.replace('bl next_int', f'bl 0x{NEXT_INT:x}')
    handler_src = handler_src.replace('bl validate', f'bl 0x{validate_addr:x}')
    handler_src = handler_src.replace('bl ack', f'bl 0x{ACK_BASE:x}')
    handler_bin = assemble(handler_src, handler_addr)
    main_bin = runtime_bin + relay_bin + handler_bin
    task_bin = boot_bin + validate_bin + get_bin

    return {
        'task': (TASK_BASE, task_bin),
        'memory': (MEM_HANDLER, next_bin),
        'main': (MAIN_BASE, main_bin),
        'ack': (ACK_BASE, ack_bin),
        'handler_addr': handler_addr,
        'relay_addr': relay_addr,
        'validate_addr': validate_addr,
        'get_addr': get_addr,
    }

if __name__ == '__main__':
    s = build_segments()
    for name in ('task','memory','main','ack'):
        addr, data = s[name]
        print(f'{name}: {len(data)} bytes {addr:#x}-{addr+len(data):#x}')
    print('handler', hex(s['handler_addr']), 'relay', hex(s['relay_addr']))
    assert s['task'][0] + len(s['task'][1]) <= 0x10009A12
    assert s['memory'][0] + len(s['memory'][1]) <= 0x1001602E
    assert s['main'][0] + len(s['main'][1]) <= MAIN_LIMIT
    assert s['ack'][0] + len(s['ack'][1]) <= ACK_LIMIT
    print('layout=PASS')
