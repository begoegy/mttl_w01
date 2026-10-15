#!/usr/bin/env python3
"""Build the exact-image-gated SFN local-cycle lab candidate from proven neutral 1.0.147.

Research/lab only. This builder never flashes or registers the result in production.
It deliberately replaces the stock factory `test` command handler with `cycle`.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

EXPECTED_SIZE = 327944
EXPECTED_SHA256 = "1a3b66c4a43e63aa54615646b50f2c8507c57c3ce604be3f72211616f1379881"
XOR_KEY = bytes.fromhex("3f5a27e8d8fb85f97c8397d3ad879f97")
LOAD_ADDRESS = 0x10006000
SOURCE_VERSION = b"1.0.147"
TARGET_VERSION = b"1.0.148"
VERSION_OFFSET = 0x18E64
COMMAND_OFFSET = 0x106E4
COMMAND_SOURCE = b"test\x00\x00\x00\x00"
COMMAND_TARGET = b"cycle\x00\x00\x00"
TEXT_OFFSET = 0x106F4
TEXT_END = 0x10724
HANDLER_OFFSET = 0x1007A     # memory 0x1001606a
TASK_OFFSET = 0x10110        # memory 0x10016100
NEXT_HANDLER_OFFSET = 0x10140  # memory 0x10016130

# Generated once from reviewed Thumb assembly in sfn_cycle_payload_probe.py.
# The handler parses cycle:<1-4>:<5-900>, creates a low-priority local task,
# sends the "armed" ack, and returns. The task waits 100 ticks before OFF,
# waits the requested duration locally, then turns the outlet ON and self-deletes.
HANDLER = bytes.fromhex(
    "00f0bffd002842d0451c2846fff70ffa0646012e3bdb042e39dc00f0b2fd002835d0"
    "0130fff703fa0746052f2fdb40f2843087422bdc3b02334382b0012000900020019046"
    "f20110c1f2010046f20071c1f2010140f20012f9f7d1ff02b0012815d129a840f200"
    "1146f2e462c1f20102334682b00097f4f7f4fb02b029a8f4f78afc0246002329a920"
    "6902f08dfd5fe2"
)
TASK = bytes.fromhex(
    "70b504466420faf75ef8e5b2240a0c20294604f016fd40f2e8306043faf753f80b"
    "20294604f00dfd0020faf72af8fee7"
)
ACK_FORMAT = b"up:cycle:%d:armed:%d\r\n\x00"
TASK_NAME = b"sfncycle\x00"
PRESERVED_MARKERS = (
    b"LGU+-TAP-HW002", b"MTTL-W01", b"0.1.55", b"up:ip:ip_ok",
    b"up:connect:connect_ok", b"up:ota:ota_ok", b"up:reboot_ok",
)


def decode(raw: bytes) -> bytearray:
    out = bytearray(raw)
    for i in range(16, len(out)):
        out[i] ^= XOR_KEY[i % 16]
    return out


def encode(decoded: bytes | bytearray) -> bytes:
    out = bytearray(decoded)
    for i in range(16, len(out)):
        out[i] ^= XOR_KEY[i % 16]
    return bytes(out)


def checksum(decoded: bytes | bytearray) -> int:
    return sum(decoded[:-4]) & 0xFFFFFFFF


def validate_source(raw: bytes) -> bytearray:
    if len(raw) != EXPECTED_SIZE:
        raise SystemExit(f"REFUSE: source size {len(raw)} != {EXPECTED_SIZE}")
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"REFUSE: source SHA-256 {digest} != pinned {EXPECTED_SHA256}")
    decoded = decode(raw)
    stored = struct.unpack_from("<I", decoded, len(decoded) - 4)[0]
    calculated = checksum(decoded)
    if stored != calculated:
        raise SystemExit(f"REFUSE: source checksum {stored:08x} != {calculated:08x}")
    if bytes(decoded[VERSION_OFFSET:VERSION_OFFSET + len(SOURCE_VERSION)]) != SOURCE_VERSION:
        raise SystemExit("REFUSE: source version marker mismatch")
    if bytes(decoded[COMMAND_OFFSET:COMMAND_OFFSET + len(COMMAND_SOURCE)]) != COMMAND_SOURCE:
        raise SystemExit("REFUSE: stock factory test command layout mismatch")
    if decoded.count(b"127.0.0.1") != 4:
        raise SystemExit("REFUSE: source is not the proven four-host neutral image")
    for marker in PRESERVED_MARKERS:
        if marker not in decoded:
            raise SystemExit(f"REFUSE: source marker missing: {marker!r}")
    if len(HANDLER) > TASK_OFFSET - HANDLER_OFFSET or len(TASK) != NEXT_HANDLER_OFFSET - TASK_OFFSET:
        raise SystemExit("REFUSE: internal payload layout mismatch")
    return decoded


def build(source: Path, output: Path) -> dict[str, object]:
    original = validate_source(source.read_bytes())
    patched = bytearray(original)
    patched[VERSION_OFFSET:VERSION_OFFSET + len(TARGET_VERSION)] = TARGET_VERSION
    patched[COMMAND_OFFSET:COMMAND_OFFSET + len(COMMAND_TARGET)] = COMMAND_TARGET

    text = bytearray(TEXT_END - TEXT_OFFSET)
    text[:len(ACK_FORMAT)] = ACK_FORMAT
    name_at = 0x10710 - TEXT_OFFSET
    text[name_at:name_at + len(TASK_NAME)] = TASK_NAME
    patched[TEXT_OFFSET:TEXT_END] = text

    patched[HANDLER_OFFSET:TASK_OFFSET] = b"\x00\xbf" * ((TASK_OFFSET - HANDLER_OFFSET) // 2)
    patched[HANDLER_OFFSET:HANDLER_OFFSET + len(HANDLER)] = HANDLER
    patched[TASK_OFFSET:NEXT_HANDLER_OFFSET] = TASK

    new_checksum = checksum(patched)
    struct.pack_into("<I", patched, len(patched) - 4, new_checksum)

    allowed = set(range(VERSION_OFFSET, VERSION_OFFSET + len(TARGET_VERSION)))
    allowed.update(range(COMMAND_OFFSET, COMMAND_OFFSET + len(COMMAND_TARGET)))
    allowed.update(range(TEXT_OFFSET, TEXT_END))
    allowed.update(range(HANDLER_OFFSET, NEXT_HANDLER_OFFSET))
    allowed.update(range(len(patched) - 4, len(patched)))
    changed = [i for i, (a, b) in enumerate(zip(original, patched)) if a != b]
    unexpected = [i for i in changed if i not in allowed]
    if unexpected:
        raise SystemExit(f"REFUSE: unexpected decoded change at 0x{unexpected[0]:x}")

    if bytes(patched[COMMAND_OFFSET:COMMAND_OFFSET + 6]) != b"cycle\x00":
        raise SystemExit("REFUSE: cycle command verification failed")
    if ACK_FORMAT not in patched or TASK_NAME not in patched:
        raise SystemExit("REFUSE: local-cycle response/task markers missing")
    if bytes(patched[VERSION_OFFSET:VERSION_OFFSET + len(TARGET_VERSION)]) != TARGET_VERSION:
        raise SystemExit("REFUSE: target version verification failed")
    if patched.count(b"127.0.0.1") != 4:
        raise SystemExit("REFUSE: neutral host isolation changed")
    for marker in PRESERVED_MARKERS:
        if marker not in patched:
            raise SystemExit(f"REFUSE: output marker missing: {marker!r}")

    encoded = encode(patched)
    output.parent.mkdir(parents=True, exist_ok=True)
    tmp = output.with_suffix(output.suffix + ".tmp")
    tmp.write_bytes(encoded)
    tmp.replace(output)

    verify = decode(output.read_bytes())
    stored = struct.unpack_from("<I", verify, len(verify) - 4)[0]
    if stored != checksum(verify) or stored != new_checksum:
        output.unlink(missing_ok=True)
        raise SystemExit("REFUSE: output checksum verification failed")
    return {
        "source_sha256": EXPECTED_SHA256,
        "output_sha256": hashlib.sha256(encoded).hexdigest(),
        "size": len(encoded),
        "internal_checksum": f"{new_checksum:08x}",
        "decoded_changed_bytes": len(changed),
        "target_version": TARGET_VERSION.decode(),
        "wire_command": "up:cycle:<1-4>:<5-900>",
        "ack": "up:cycle:<channel>:armed:<seconds>",
        "factory_test_command_replaced": True,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--ack-lab-only", action="store_true")
    args = parser.parse_args()
    if not args.ack_lab_only:
        raise SystemExit("REFUSE: pass --ack-lab-only; this candidate is not production-approved")
    meta = build(args.source, args.output)
    print("LAB-ONLY LOCAL-CYCLE CANDIDATE BUILT")
    for key, value in meta.items():
        print(f"{key}={value}")
    print(f"output={args.output}")
    print("NO DEVICE WAS FLASHED; NOT APPROVED FOR CUSTOMER DEPLOYMENT")


if __name__ == "__main__":
    main()
