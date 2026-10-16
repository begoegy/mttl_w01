#!/usr/bin/env python3
"""Build LAB-ONLY SFN Local Event Queue 1.0.150 from exact public Voltra 1.0.146.

This is a host-only, exact-image-gated family port. It composes the already-proven neutral
host isolation with the physically-proven 1.0.81 Local Queue machine-code layout. It never
opens a device socket, edits the production firmware catalog, flashes, deploys, or restarts.
"""
from __future__ import annotations

import argparse
import hashlib
import runpy
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.sfn_build_neutral_lab_1_0_146 import (
    decode, encode, checksum, HOST_PATCHES, LOOPBACK,
)

EXPECTED_SIZE = 327944
EXPECTED_SHA256 = "ae997d79f8e54376a4eb94e447cf94582311544aa128ff4a487d2190f67208ad"
SOURCE_VERSION = b"1.0.146"
TARGET_VERSION = b"1.0.150"
VERSION_OFFSET = 0x18E64
LOAD = 0x10006000
TEST_CMD = 0x100166D4
MEMORY_CMD = 0x1001669C
MEMORY_FMT = 0x100166A4
PRESERVED = (
    b"LGU+-TAP-HW002", b"MTTL-W01", b"0.1.55",
    b"up:ip:ip_ok", b"up:connect:connect_ok", b"up:ota:ota_ok", b"up:reboot_ok",
    b"memory\x00", b"up:memory:%d:%d\r\n\x00", b"wifirssi", b"up:query:%d",
)


def mo(addr: int) -> int:
    return addr - LOAD + 16


def exact_source(raw: bytes) -> bytearray:
    if len(raw) != EXPECTED_SIZE:
        raise SystemExit(f"REFUSE source size={len(raw)} expected={EXPECTED_SIZE}")
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"REFUSE source sha256={digest}")
    d = decode(raw)
    if struct.unpack_from("<I", d, len(d)-4)[0] != checksum(d):
        raise SystemExit("REFUSE source checksum mismatch")
    if bytes(d[VERSION_OFFSET:VERSION_OFFSET+len(SOURCE_VERSION)]) != SOURCE_VERSION:
        raise SystemExit("REFUSE source version mismatch")
    if bytes(d[mo(TEST_CMD):mo(TEST_CMD)+5]) != b"test\x00":
        raise SystemExit("REFUSE factory-test ingress layout mismatch")
    if bytes(d[mo(MEMORY_CMD):mo(MEMORY_CMD)+7]) != b"memory\x00":
        raise SystemExit("REFUSE memory command mismatch")
    if bytes(d[mo(MEMORY_FMT):mo(MEMORY_FMT)+len(b"up:memory:%d:%d\r\n\x00")]) != b"up:memory:%d:%d\r\n\x00":
        raise SystemExit("REFUSE memory formatter mismatch")
    for offset, expected, label in HOST_PATCHES:
        if bytes(d[offset:offset+len(expected)]) != expected:
            raise SystemExit(f"REFUSE source {label} mismatch at {offset:#x}")
    for marker in PRESERVED:
        if marker not in d:
            raise SystemExit(f"REFUSE missing source marker {marker!r}")
    return d


def put_region(buf: bytearray, start: int, end: int, payload: bytes) -> None:
    if len(payload) > end-start:
        raise SystemExit(f"REFUSE payload overflow {start:#x}..{end:#x}")
    o = mo(start)
    buf[o:o+(end-start)] = b"\x00\xbf" * ((end-start)//2)
    buf[o:o+len(payload)] = payload


def build(source: Path, output: Path) -> dict[str, object]:
    original = exact_source(source.read_bytes())
    patched = bytearray(original)
    layout = runpy.run_path(
        str(Path(__file__).with_name("sfn_local_queue_timer_new71_segments_r8fix.py")),
        run_name="sfnq_layout_146",
    )
    test_start, test_end = layout["TEST"], layout["TEST_END"]
    main_start, main_end = layout["MAIN"], layout["MAIN_END"]
    fmt_addr, name_addr, ptr_addr = layout["FMT"], layout["NAME"], layout["DATA_PTR"]
    test_blob, main_blob = layout["test_blob"], layout["main_blob"]
    fmt = b"up:sfnq:%d:%d:%d:%d\r\n\x00"
    name = b"sfnqueue\x00"

    if (test_start, test_end) != (0x1001606A, 0x10016130):
        raise SystemExit("REFUSE reviewed test region drift")
    if (main_start, main_end) != (0x1001EB6C, 0x1001EE54):
        raise SystemExit("REFUSE reviewed main region drift")
    if (fmt_addr, name_addr, ptr_addr) != (0x1001EE5C, 0x1001EE80, 0x1001EE98):
        raise SystemExit("REFUSE reviewed data region drift")
    if len(test_blob) != 198 or len(main_blob) != 736:
        raise SystemExit(f"REFUSE unexpected code size test={len(test_blob)} main={len(main_blob)}")

    # Compose the already-proven neutral host isolation directly from the exact vendor source.
    for offset, expected, _label in HOST_PATCHES:
        patched[offset:offset+len(expected)] = LOOPBACK.ljust(len(expected), b"\0")

    patched[VERSION_OFFSET:VERSION_OFFSET+len(TARGET_VERSION)] = TARGET_VERSION
    patched[mo(TEST_CMD):mo(TEST_CMD)+5] = b"sfnq\x00"
    put_region(patched, test_start, test_end, test_blob)
    put_region(patched, main_start, main_end, main_blob)
    patched[mo(fmt_addr):mo(ptr_addr)+4] = b"\x00" * ((ptr_addr+4)-fmt_addr)
    patched[mo(fmt_addr):mo(fmt_addr)+len(fmt)] = fmt
    patched[mo(name_addr):mo(name_addr)+len(name)] = name
    patched[mo(ptr_addr):mo(ptr_addr)+4] = b"\x00\x00\x00\x00"

    allowed = set(range(VERSION_OFFSET, VERSION_OFFSET+len(TARGET_VERSION)))
    for offset, expected, _label in HOST_PATCHES:
        allowed.update(range(offset, offset+len(expected)))
    allowed.update(range(mo(TEST_CMD), mo(TEST_CMD)+5))
    allowed.update(range(mo(test_start), mo(test_end)))
    allowed.update(range(mo(main_start), mo(main_end)))
    allowed.update(range(mo(fmt_addr), mo(ptr_addr)+4))
    allowed.update(range(len(patched)-4, len(patched)))

    struct.pack_into("<I", patched, len(patched)-4, checksum(patched))
    changed = [i for i, (a, b) in enumerate(zip(original, patched)) if a != b]
    bad = [i for i in changed if i not in allowed]
    if bad:
        raise SystemExit(f"REFUSE unexpected decoded change at {bad[0]:#x}")

    if bytes(patched[mo(MEMORY_CMD):mo(MEMORY_CMD)+7]) != b"memory\x00":
        raise SystemExit("REFUSE memory command changed")
    if bytes(patched[mo(MEMORY_FMT):mo(MEMORY_FMT)+len(b"up:memory:%d:%d\r\n\x00")]) != b"up:memory:%d:%d\r\n\x00":
        raise SystemExit("REFUSE memory formatter changed")
    if patched.count(b"127.0.0.1") != 4:
        raise SystemExit("REFUSE neutral host isolation failed")
    for marker in PRESERVED:
        if marker not in patched:
            raise SystemExit(f"REFUSE output lost marker {marker!r}")

    encoded = encode(patched)
    output.parent.mkdir(parents=True, exist_ok=True)
    tmp = output.with_suffix(output.suffix + ".tmp")
    tmp.write_bytes(encoded)
    tmp.replace(output)
    verify = decode(output.read_bytes())
    if struct.unpack_from("<I", verify, len(verify)-4)[0] != checksum(verify):
        output.unlink(missing_ok=True)
        raise SystemExit("REFUSE output checksum failed")
    if verify != patched:
        output.unlink(missing_ok=True)
        raise SystemExit("REFUSE encode/decode roundtrip mismatch")

    return {
        "source_sha256": EXPECTED_SHA256,
        "output_sha256": hashlib.sha256(encoded).hexdigest(),
        "size": len(encoded),
        "decoded_changed_bytes": len(changed),
        "target_version": TARGET_VERSION.decode(),
        "test_code_bytes": len(test_blob),
        "main_code_bytes": len(main_blob),
        "resident_events": 8,
        "state_bytes": 256,
        "new_worker_task": False,
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("source", type=Path)
    ap.add_argument("output", type=Path)
    ap.add_argument("--lab-build-only", action="store_true")
    args = ap.parse_args()
    if not args.lab_build_only:
        raise SystemExit("REFUSE: pass --lab-build-only")
    meta = build(args.source, args.output)
    print("LAB_ONLY_LOCAL_QUEUE_1_0_150_BUILT")
    for key, value in meta.items():
        print(f"{key}={value}")
    print("NO_DEVICE_IO NO_FLASH NO_OTA NO_DEPLOY NO_CATALOG_CHANGE")


if __name__ == "__main__":
    main()
