#!/usr/bin/env python3
"""Build lab-only SFN Local Event Queue 1.0.80 from exact proven New 1.0.71.

Host-only builder. It never opens a socket, talks to a device, edits a firmware
catalog, flashes, deploys or restarts production.
"""
from __future__ import annotations
import argparse, hashlib, runpy, struct
from pathlib import Path

from tools.sfn_build_neutral_lab_v2_1_0_66 import decode, encode, checksum

EXPECTED_SIZE = 327944
EXPECTED_SHA256 = 'c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081'
SOURCE_VERSION = b'1.0.71'
TARGET_VERSION = b'1.0.80'
VERSION_OFFSET = 0x18E64
LOAD = 0x10006000
TEST_CMD = 0x100166D4
MEMORY_CMD = 0x1001669C
MEMORY_FMT = 0x100166A4
PRESERVED = (
    b'LGU+-TAP-HW002', b'MTTL-W01', b'0.1.54',
    b'up:ip:ip_ok', b'up:connect:connect_ok', b'up:ota:ota_ok', b'up:reboot_ok',
    b'memory\x00', b'up:memory:%d:%d\r\n\x00', b'wifirssi', b'up:query:%d',
)

def mo(addr: int) -> int:
    return addr - LOAD + 16

def exact_source(raw: bytes) -> bytearray:
    if len(raw) != EXPECTED_SIZE:
        raise SystemExit(f'REFUSE source size={len(raw)} expected={EXPECTED_SIZE}')
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f'REFUSE source sha256={digest}')
    d = decode(raw)
    if struct.unpack_from('<I', d, len(d)-4)[0] != checksum(d):
        raise SystemExit('REFUSE source checksum mismatch')
    if bytes(d[VERSION_OFFSET:VERSION_OFFSET+len(SOURCE_VERSION)]) != SOURCE_VERSION:
        raise SystemExit('REFUSE source version mismatch')
    if bytes(d[mo(TEST_CMD):mo(TEST_CMD)+5]) != b'test\x00':
        raise SystemExit('REFUSE factory-test ingress layout mismatch')
    if bytes(d[mo(MEMORY_CMD):mo(MEMORY_CMD)+7]) != b'memory\x00':
        raise SystemExit('REFUSE memory command mismatch')
    if bytes(d[mo(MEMORY_FMT):mo(MEMORY_FMT)+len(b'up:memory:%d:%d\r\n\x00')]) != b'up:memory:%d:%d\r\n\x00':
        raise SystemExit('REFUSE memory formatter mismatch')
    for marker in PRESERVED:
        if marker not in d:
            raise SystemExit(f'REFUSE missing source marker {marker!r}')
    if d.count(b'127.0.0.1') != 4:
        raise SystemExit('REFUSE source is not proven four-host neutral image')
    return d

def put_region(buf: bytearray, start: int, end: int, payload: bytes) -> None:
    if len(payload) > end-start:
        raise SystemExit(f'REFUSE payload overflow {start:#x}..{end:#x}')
    o = mo(start)
    buf[o:o+(end-start)] = b'\x00\xbf' * ((end-start)//2)
    buf[o:o+len(payload)] = payload

def build(source: Path, output: Path) -> dict[str, object]:
    original = exact_source(source.read_bytes())
    patched = bytearray(original)
    layout = runpy.run_path(str(Path(__file__).with_name('sfn_local_queue_timer_new71_segments_ack.py')), run_name='sfnq_layout')

    test_start, test_end = layout['TEST'], layout['TEST_END']
    main_start, main_end = layout['MAIN'], layout['MAIN_END']
    fmt_addr, name_addr, ptr_addr = layout['FMT'], layout['NAME'], layout['DATA_PTR']
    test_blob, main_blob = layout['test_blob'], layout['main_blob']
    fmt = b'up:sfnq:%d:%d:%d:%d\r\n\x00'
    name = b'sfnqueue\x00'

    if test_start != 0x1001606A or test_end != 0x10016130:
        raise SystemExit('REFUSE reviewed test region drift')
    if main_start != 0x1001EB6C or main_end != 0x1001EE54:
        raise SystemExit('REFUSE reviewed main region drift')
    if (fmt_addr, name_addr, ptr_addr) != (0x1001EE5C,0x1001EE80,0x1001EE98):
        raise SystemExit('REFUSE reviewed data region drift')
    if len(test_blob) != 198 or len(main_blob) != 736:
        raise SystemExit(f'REFUSE unexpected code size test={len(test_blob)} main={len(main_blob)}')

    patched[VERSION_OFFSET:VERSION_OFFSET+len(TARGET_VERSION)] = TARGET_VERSION
    patched[mo(TEST_CMD):mo(TEST_CMD)+5] = b'sfnq\x00'
    put_region(patched, test_start, test_end, test_blob)
    put_region(patched, main_start, main_end, main_blob)
    # Reclaimed post-version data only. Keep version bytes at 0x1001EE54 untouched except explicit version write above.
    patched[mo(fmt_addr):mo(ptr_addr)+4] = b'\x00' * ((ptr_addr+4)-fmt_addr)
    patched[mo(fmt_addr):mo(fmt_addr)+len(fmt)] = fmt
    patched[mo(name_addr):mo(name_addr)+len(name)] = name
    patched[mo(ptr_addr):mo(ptr_addr)+4] = b'\x00\x00\x00\x00'

    allowed = set(range(VERSION_OFFSET, VERSION_OFFSET+len(TARGET_VERSION)))
    allowed.update(range(mo(TEST_CMD), mo(TEST_CMD)+5))
    allowed.update(range(mo(test_start), mo(test_end)))
    allowed.update(range(mo(main_start), mo(main_end)))
    allowed.update(range(mo(fmt_addr), mo(ptr_addr)+4))
    allowed.update(range(len(patched)-4, len(patched)))

    struct.pack_into('<I', patched, len(patched)-4, checksum(patched))
    changed = [i for i,(a,b) in enumerate(zip(original,patched)) if a != b]
    bad = [i for i in changed if i not in allowed]
    if bad:
        raise SystemExit(f'REFUSE unexpected decoded change at {bad[0]:#x}')

    # Preservation gates after patch.
    if bytes(patched[mo(MEMORY_CMD):mo(MEMORY_CMD)+7]) != b'memory\x00':
        raise SystemExit('REFUSE memory command changed')
    if bytes(patched[mo(MEMORY_FMT):mo(MEMORY_FMT)+len(b'up:memory:%d:%d\r\n\x00')]) != b'up:memory:%d:%d\r\n\x00':
        raise SystemExit('REFUSE memory formatter changed')
    if patched.count(b'127.0.0.1') != 4:
        raise SystemExit('REFUSE neutral host isolation changed')
    for marker in PRESERVED:
        if marker not in patched:
            raise SystemExit(f'REFUSE output lost marker {marker!r}')

    encoded = encode(patched)
    output.parent.mkdir(parents=True, exist_ok=True)
    tmp = output.with_suffix(output.suffix+'.tmp')
    tmp.write_bytes(encoded)
    tmp.replace(output)

    verify = decode(output.read_bytes())
    if struct.unpack_from('<I', verify, len(verify)-4)[0] != checksum(verify):
        output.unlink(missing_ok=True)
        raise SystemExit('REFUSE output checksum failed')
    if verify != patched:
        output.unlink(missing_ok=True)
        raise SystemExit('REFUSE encode/decode roundtrip mismatch')

    return {
        'source_sha256': EXPECTED_SHA256,
        'output_sha256': hashlib.sha256(encoded).hexdigest(),
        'size': len(encoded),
        'decoded_changed_bytes': len(changed),
        'target_version': TARGET_VERSION.decode(),
        'test_code_bytes': len(test_blob),
        'main_code_bytes': len(main_blob),
        'resident_events': 8,
        'state_bytes': 256,
        'new_worker_task': False,
    }

def main() -> None:
    ap=argparse.ArgumentParser()
    ap.add_argument('source', type=Path)
    ap.add_argument('output', type=Path)
    ap.add_argument('--lab-build-only', action='store_true')
    a=ap.parse_args()
    if not a.lab_build_only:
        raise SystemExit('REFUSE: pass --lab-build-only')
    meta=build(a.source,a.output)
    print('LAB_ONLY_LOCAL_QUEUE_1_0_80_BUILT')
    for k,v in meta.items(): print(f'{k}={v}')
    print('NO DEVICE_IO NO_FLASH NO_OTA NO_DEPLOY NO_CATALOG_CHANGE')

if __name__=='__main__': main()
