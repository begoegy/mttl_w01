#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_LITTLE_ENDIAN, CS_MODE_MCLASS

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT))
from tools.sfn_build_neutral_lab_v2_1_0_66 import decode, checksum, HOST_PATCHES, LAB_VERSION_OFFSET

SOURCE = ROOT / 'work/firmware/1.0.66/comMTTL-W01_1.0.66.fwr'
NEUTRAL71 = Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.71-neutral-lab-v2.fwr')
TARGET81 = Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.81-local-queue-r8fix-new-lab.fwr')
SOURCE_SHA = 'd780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04'
NEUTRAL71_SHA = 'c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081'
TARGET81_SHA = '5b96489849e8808512a0e3f918013102ced8cb5610f939ae49da60cc2220ff0a'
EXPECTED_SIZE = 327944
LOAD = 0x10006000
TEST_CMD = 0x100166D4
MEMORY_CMD = 0x1001669C
MEMORY_FMT = 0x100166A4
TEST_START, TEST_END = 0x1001606A, 0x10016130
MAIN_START, MAIN_END = 0x1001EB6C, 0x1001EE54
FMT, NAME, DATA_PTR = 0x1001EE5C, 0x1001EE80, 0x1001EE98


def encoded_bytes(path: Path, sha: str) -> bytes:
    raw = path.read_bytes()
    assert len(raw) == EXPECTED_SIZE, (path, len(raw))
    assert hashlib.sha256(raw).hexdigest() == sha, path
    decoded = bytes(decode(raw))
    assert struct.unpack_from('<I', decoded, len(decoded) - 4)[0] == checksum(decoded)
    return decoded


def fo(addr: int) -> int:
    return addr - LOAD + 16


def rd(image: bytes, addr: int, size: int) -> bytes:
    return image[fo(addr):fo(addr) + size]


def diff_ranges(a: bytes, b: bytes) -> list[tuple[int, int]]:
    diffs = [i for i, (x, y) in enumerate(zip(a, b)) if x != y]
    if not diffs:
        return []
    out = []
    start = prev = diffs[0]
    for current in diffs[1:]:
        if current != prev + 1:
            out.append((start, prev + 1))
            start = current
        prev = current
    out.append((start, prev + 1))
    return out


source = encoded_bytes(SOURCE, SOURCE_SHA)
neutral71 = encoded_bytes(NEUTRAL71, NEUTRAL71_SHA)
target81 = encoded_bytes(TARGET81, TARGET81_SHA)

# Source identity / ingress / memory-state pin.
assert source[LAB_VERSION_OFFSET:LAB_VERSION_OFFSET + 6] == b'1.0.66'
assert rd(source, MEMORY_CMD, 7) == b'memory\x00'
assert rd(source, MEMORY_FMT, len(b'up:memory:%d:%d\r\n\x00')) == b'up:memory:%d:%d\r\n\x00'
assert rd(source, TEST_CMD, 5) == b'test\x00'
assert rd(source, DATA_PTR, 4) == b'\x00' * 4
assert rd(source, 0x10013714, 4) == bytes.fromhex('4ff47a71')  # mov.w r1,#1000

# Prove the exact stock 1.0.66 code image and exact 1.0.71 golden image differ only in
# neutral host data, version identity, and checksum. Therefore code addresses are not inherited
# by assumption: the bytes at every reviewed code region are proved equal.
lineage_allowed = set(range(LAB_VERSION_OFFSET, LAB_VERSION_OFFSET + 6))
for offset, old, _label in HOST_PATCHES:
    lineage_allowed.update(range(offset, offset + len(old)))
lineage_allowed.update(range(EXPECTED_SIZE - 4, EXPECTED_SIZE))
lineage_bad = [i for i, (a, b) in enumerate(zip(source, neutral71)) if a != b and i not in lineage_allowed]
assert not lineage_bad, hex(lineage_bad[0]) if lineage_bad else None
for start, end in ((TEST_START, TEST_END), (MAIN_START, MAIN_END), (0x10012248, 0x100123CA),
                   (0x10015A00, 0x10016140), (0x1001AB00, 0x1001AC00)):
    assert rd(source, start, end - start) == rd(neutral71, start, end - start), (hex(start), hex(end))

cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_LITTLE_ENDIAN | CS_MODE_MCLASS)


def target_at(addr: int) -> int:
    ins = next(cs.disasm(rd(source, addr, 4), addr))
    assert ins.mnemonic == 'bl' and ins.op_str.startswith('#0x'), (hex(addr), ins.mnemonic, ins.op_str)
    return int(ins.op_str[1:], 16)


# Exact 1.0.66 timer/relay proof.
assert target_at(0x10012272) == 0x10010068  # timer service task creator
assert target_at(0x10015AFC) == 0x1001AB42  # stock timer callback -> relay dispatcher
assert target_at(0x100160B2) == 0x100122B8  # native timer control path
first = list(cs.disasm(rd(source, 0x10012290, 8), 0x10012290))
assert first[0].mnemonic == 'movs' and '#0x28' in first[0].op_str
assert target_at(0x10012292) == 0x100120A0  # 40-byte timer object allocation
callback = list(cs.disasm(rd(source, 0x100123C2, 8), 0x100123C2))
assert callback[0].mnemonic.startswith('ldr') and '#0x24' in callback[0].op_str
assert callback[1].mnemonic == 'mov' and callback[1].op_str == 'r0, r7'
assert callback[2].mnemonic == 'blx' and callback[2].op_str == 'r1'
window = list(cs.disasm(rd(source, 0x10012248, 0x30), 0x10012248))
window_text = '\n'.join(f'{i.address:08x} {i.mnemonic} {i.op_str}' for i in window)
assert '1001225c str r4, [sp, #0xc]' in window_text
assert '10012262 str r4, [sp, #8]' in window_text
assert '10012266 mov.w r2, #0x200' in window_text

# Candidate payload-space identity against the physically-canary source lineage.
region_hashes = {}
for label, start, end in (
    ('factory_test_region', TEST_START, TEST_END),
    ('main_reclaimed_region', MAIN_START, MAIN_END),
    ('data_region', FMT, DATA_PTR + 4),
):
    blob = rd(source, start, end - start)
    assert blob == rd(neutral71, start, end - start)
    region_hashes[label] = hashlib.sha256(blob).hexdigest()

# Prove the exact already-canary-tested 1.0.81 artifact is a bounded direct transition from
# stock 1.0.66: neutral host slots + reviewed queue regions + command/version/data + checksum only.
direct_allowed = set(range(LAB_VERSION_OFFSET, LAB_VERSION_OFFSET + 6))
for offset, old, _label in HOST_PATCHES:
    direct_allowed.update(range(offset, offset + len(old)))
direct_allowed.update(range(fo(TEST_CMD), fo(TEST_CMD) + 5))
direct_allowed.update(range(fo(TEST_START), fo(TEST_END)))
direct_allowed.update(range(fo(MAIN_START), fo(MAIN_END)))
direct_allowed.update(range(fo(FMT), fo(DATA_PTR) + 4))
direct_allowed.update(range(EXPECTED_SIZE - 4, EXPECTED_SIZE))
direct_bad = [i for i, (a, b) in enumerate(zip(source, target81)) if a != b and i not in direct_allowed]
assert not direct_bad, hex(direct_bad[0]) if direct_bad else None

# Vendor safety/control markers remain present in the exact target artifact.
for marker in (b'LGU+-TAP-HW002', b'MTTL-W01', b'0.1.54', b'up:ip:ip_ok', b'up:connect:connect_ok',
               b'up:ota:ota_ok', b'up:reboot_ok', b'memory\x00', b'up:memory:%d:%d\r\n\x00',
               b'wifirssi', b'up:query:%d'):
    assert marker in target81, marker
assert target81.count(b'127.0.0.1') == 4

print('SOURCE_PIN=PASS')
print('source_version=0.1.54-1.0.66')
print('source_size=327944')
print('source_sha256=' + SOURCE_SHA)
print('rollback_sha256=' + SOURCE_SHA)
print('binary_family=mttl-w01-hw002-r1')
print('source_to_1.0.71_unexpected_diff=0')
for start, end in diff_ranges(source, neutral71):
    print(f'lineage_diff_range=0x{start:x}..0x{end:x}')
print('timer_service_xref=PASS')
print('timer_create_control=PASS')
print('relay_dispatcher_xref=PASS')
print('parser_ingress=PASS')
print('memory_state_pointer=PASS')
for label, digest in region_hashes.items():
    print(f'{label}_sha256={digest}')
print('direct_1.0.66_to_exact_1.0.81_unexpected_diff=0')
print('target_1.0.81_sha256=' + TARGET81_SHA)
print('STATIC_PROOF=PASS')
print('NO_DEVICE_IO NO_FLASH NO_OTA NO_DEPLOY NO_CATALOG_WRITE')
