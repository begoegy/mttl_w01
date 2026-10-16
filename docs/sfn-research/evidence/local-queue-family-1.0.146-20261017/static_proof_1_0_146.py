#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_LITTLE_ENDIAN, CS_MODE_MCLASS

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT))
from tools.sfn_build_neutral_lab_1_0_146 import decode as decode146, checksum, HOST_PATCHES, VERSION_OFFSET
from tools.sfn_build_neutral_lab_v2_1_0_66 import decode as decode66

SOURCE146 = ROOT / 'work/firmware/1.0.146/comMTTL-W01_1.0.146.fwr'
SOURCE66 = ROOT / 'work/firmware/1.0.66/comMTTL-W01_1.0.66.fwr'
NEUTRAL147 = Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.147-neutral-lab.fwr')
TARGET150 = Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.150-local-queue-hw002-r1b-lab.fwr')
TARGET81 = Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.81-local-queue-r8fix-new-lab.fwr')
SOURCE146_SHA = 'ae997d79f8e54376a4eb94e447cf94582311544aa128ff4a487d2190f67208ad'
SOURCE66_SHA = 'd780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04'
NEUTRAL147_SHA = '1a3b66c4a43e63aa54615646b50f2c8507c57c3ce604be3f72211616f1379881'
TARGET150_SHA = '14a89fdf7cff3961d6bc116c10601ff2b319656c557ea7fc7460d85f64aaa9b4'
TARGET81_SHA = '5b96489849e8808512a0e3f918013102ced8cb5610f939ae49da60cc2220ff0a'
EXPECTED_SIZE = 327944
LOAD = 0x10006000
TEST_CMD = 0x100166D4
MEMORY_CMD = 0x1001669C
MEMORY_FMT = 0x100166A4
TEST_START, TEST_END = 0x1001606A, 0x10016130
MAIN_START, MAIN_END = 0x1001EB6C, 0x1001EE54
FMT, NAME, DATA_PTR = 0x1001EE5C, 0x1001EE80, 0x1001EE98


def encoded_bytes(path: Path, sha: str, decoder) -> bytes:
    raw = path.read_bytes()
    assert len(raw) == EXPECTED_SIZE, (path, len(raw))
    assert hashlib.sha256(raw).hexdigest() == sha, path
    decoded = bytes(decoder(raw))
    assert struct.unpack_from('<I', decoded, len(decoded) - 4)[0] == checksum(decoded)
    return decoded


def fo(addr: int) -> int:
    return addr - LOAD + 16


def rd(image: bytes, addr: int, size: int) -> bytes:
    return image[fo(addr):fo(addr) + size]


source146 = encoded_bytes(SOURCE146, SOURCE146_SHA, decode146)
source66 = encoded_bytes(SOURCE66, SOURCE66_SHA, decode66)
neutral147 = encoded_bytes(NEUTRAL147, NEUTRAL147_SHA, decode146)
target150 = encoded_bytes(TARGET150, TARGET150_SHA, decode146)
target81 = encoded_bytes(TARGET81, TARGET81_SHA, decode66)

# Exact source identity and parser/state pins.
assert source146[VERSION_OFFSET:VERSION_OFFSET + 7] == b'1.0.146'
assert rd(source146, TEST_CMD, 5) == b'test\x00'
assert rd(source146, MEMORY_CMD, 7) == b'memory\x00'
assert rd(source146, MEMORY_FMT, len(b'up:memory:%d:%d\r\n\x00')) == b'up:memory:%d:%d\r\n\x00'
assert rd(source146, DATA_PTR, 4) == b'\x00' * 4

# Re-prove the already-physical-canary neutral 1.0.147 lineage: host slots + version + checksum only.
neutral_allowed = set(range(VERSION_OFFSET, VERSION_OFFSET + 7))
for offset, old, _label in HOST_PATCHES:
    neutral_allowed.update(range(offset, offset + len(old)))
neutral_allowed.update(range(EXPECTED_SIZE - 4, EXPECTED_SIZE))
neutral_bad = [i for i, (a, b) in enumerate(zip(source146, neutral147)) if a != b and i not in neutral_allowed]
assert not neutral_bad, hex(neutral_bad[0]) if neutral_bad else None

# Exact queue-owned regions are byte-identical to the proven 1.0.66 lineage before patching.
for label, start, end in (
    ('timer_core', 0x10012248, 0x100123CA),
    ('factory_exact', TEST_START, TEST_END),
    ('main_exact', MAIN_START, MAIN_END),
    ('data_exact', FMT, DATA_PTR + 4),
    ('relay_xref_window', 0x10015AE0, 0x10015B20),
):
    a = rd(source146, start, end - start)
    b = rd(source66, start, end - start)
    assert a == b, label

# Every external entrypoint called by the queue payload has the same first 64 bytes as the
# physically-proven 1.0.66 lineage. RELAY is a tiny jump-table dispatcher and is checked exactly.
refs = {
    'ALLOC': 0x100120A0,
    'FREE': 0x10012176,
    'TIMER_CREATE': 0x1001227E,
    'TIMER_CMD': 0x100122B8,
    'TICK': 0x100103F8,
    'ENTER': 0x1001094C,
    'EXITC': 0x10010964,
    'NEXTTOK': 0x10016BEC,
    'ATOI': 0x10015498,
    'FORMAT': 0x1000A8CC,
    'STRLEN': 0x1000AA00,
    'SEND': 0x10018C12,
}
for label, addr in refs.items():
    assert rd(source146, addr, 64) == rd(source66, addr, 64), label
assert rd(source146, 0x1001AB42, 14) == rd(source66, 0x1001AB42, 14)

# Independently prove the key stock xrefs in this exact 1.0.146 binary.
cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_LITTLE_ENDIAN | CS_MODE_MCLASS)

def target_at(image: bytes, addr: int) -> int:
    ins = next(cs.disasm(rd(image, addr, 4), addr))
    assert ins.mnemonic == 'bl' and ins.op_str.startswith('#0x'), (hex(addr), ins.mnemonic, ins.op_str)
    return int(ins.op_str[1:], 16)

assert target_at(source146, 0x10012272) == 0x10010068
assert target_at(source146, 0x10015AFC) == 0x1001AB42
assert target_at(source146, 0x100160B2) == 0x100122B8

# The lab 1.0.150 queue bytes are exactly the physically-proven 1.0.81 queue bytes.
for label, start, end in (
    ('command', TEST_CMD, TEST_CMD + 5),
    ('test_payload', TEST_START, TEST_END),
    ('main_payload', MAIN_START, MAIN_END),
    ('queue_data', FMT, DATA_PTR + 4),
):
    assert rd(target150, start, end - start) == rd(target81, start, end - start), label

# Direct source 1.0.146 -> lab target 1.0.150 is bounded to reviewed neutral + queue regions only.
direct_allowed = set(range(VERSION_OFFSET, VERSION_OFFSET + 7))
for offset, old, _label in HOST_PATCHES:
    direct_allowed.update(range(offset, offset + len(old)))
direct_allowed.update(range(fo(TEST_CMD), fo(TEST_CMD) + 5))
direct_allowed.update(range(fo(TEST_START), fo(TEST_END)))
direct_allowed.update(range(fo(MAIN_START), fo(MAIN_END)))
direct_allowed.update(range(fo(FMT), fo(DATA_PTR) + 4))
direct_allowed.update(range(EXPECTED_SIZE - 4, EXPECTED_SIZE))
direct_bad = [i for i, (a, b) in enumerate(zip(source146, target150)) if a != b and i not in direct_allowed]
assert not direct_bad, hex(direct_bad[0]) if direct_bad else None

# Preserve vendor/device control markers while neutralizing only the four legacy service hosts.
for marker in (
    b'LGU+-TAP-HW002', b'MTTL-W01', b'0.1.55', b'up:ip:ip_ok', b'up:connect:connect_ok',
    b'up:ota:ota_ok', b'up:reboot_ok', b'memory\x00', b'up:memory:%d:%d\r\n\x00',
    b'wifirssi', b'up:query:%d',
):
    assert marker in target150, marker
assert target150.count(b'127.0.0.1') == 4
assert target150[VERSION_OFFSET:VERSION_OFFSET + 7] == b'1.0.150'

print('SOURCE_PIN=PASS')
print('source_version=0.1.55-1.0.146')
print('source_size=327944')
print('source_sha256=' + SOURCE146_SHA)
print('rollback_sha256=' + SOURCE146_SHA)
print('binary_family=mttl-w01-hw002-r1')
print('neutral_1.0.147_lineage=PASS')
print('queue_owned_source_regions_match_proven_1.0.66=PASS')
print('queue_external_entrypoints_match_proven_1.0.66=PASS')
print('timer_service_xref=PASS')
print('relay_dispatcher_xref=PASS')
print('native_timer_control_xref=PASS')
print('target_queue_bytes_match_physical_1.0.81=PASS')
print('direct_1.0.146_to_1.0.150_unexpected_diff=0')
print('target_1.0.150_sha256=' + TARGET150_SHA)
print('STATIC_PROOF=PASS')
print('NO_DEVICE_IO NO_FLASH NO_OTA NO_DEPLOY NO_CATALOG_WRITE')
