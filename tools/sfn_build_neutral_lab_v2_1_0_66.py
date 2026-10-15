"""Build a LAB-ONLY MTTL-W01 neutral candidate from pinned stock 1.0.66.

V2 does not patch task-creation instructions. Instead it keeps the stock control,
OTA, relay, meter and Wi-Fi code intact and replaces only the four known legacy
LG U+ service destinations with loopback, so those background clients cannot
leave the strip. The normal bridge/controller address remains configurable via
`up:ip`.
"""
from __future__ import annotations

import hashlib
import struct
from pathlib import Path

EXPECTED_SIZE = 327944
EXPECTED_SHA256 = "d780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04"
XOR_KEY = bytes.fromhex("3f5a27e8d8fb85f9bd4d51196c4f5159")
LAB_VERSION_OFFSET = 0x18E64
LAB_VERSION_SOURCE = b"1.0.66"
LAB_VERSION = b"1.0.70"
LOOPBACK = b"127.0.0.1"
HOST_PATCHES = (
    (0x20594, b"hdslog.lguplus.co.kr", "QMS TLS host"),
    (0x20660, b"hdslog.lguplus.co.kr", "QMS HTTP host"),
    (0x4DCFC, b"mef.onem2m.uplus.co.kr", "MEF/certificate host"),
    (0x4DD30, b"brk2.onem2m.uplus.co.kr", "MQTT host"),
)
PRESERVED_MARKERS = (
    b"LGU+-TAP-HW002", b"MTTL-W01", b"0.1.54",
    b"up:ip:ip_ok", b"up:connect:connect_ok", b"up:ota:ota_ok",
    b"up:reboot_ok", b"wifirssi", b"up:query:%d",
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
        raise ValueError(f"source size {len(raw)} != {EXPECTED_SIZE}")
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError(f"source SHA-256 mismatch: {digest}")
    decoded = decode(raw)
    stored = struct.unpack_from("<I", decoded, len(decoded) - 4)[0]
    if stored != checksum(decoded):
        raise ValueError("source checksum mismatch")
    if bytes(decoded[LAB_VERSION_OFFSET:LAB_VERSION_OFFSET + 6]) != LAB_VERSION_SOURCE:
        raise ValueError("unexpected stock version bytes")
    for offset, expected, label in HOST_PATCHES:
        actual = bytes(decoded[offset:offset + len(expected)])
        if actual != expected:
            raise ValueError(f"unexpected {label} bytes at 0x{offset:x}")
    for marker in PRESERVED_MARKERS:
        if marker not in decoded:
            raise ValueError(f"missing marker {marker!r}")
    return decoded


def build(source: Path, output: Path) -> dict[str, object]:
    original = validate_source(source.read_bytes())
    patched = bytearray(original)
    for offset, expected, _label in HOST_PATCHES:
        patched[offset:offset + len(expected)] = LOOPBACK.ljust(len(expected), b"\0")
    patched[LAB_VERSION_OFFSET:LAB_VERSION_OFFSET + 6] = LAB_VERSION
    new_checksum = checksum(patched)
    struct.pack_into("<I", patched, len(patched) - 4, new_checksum)

    encoded = encode(patched)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(encoded)

    verify = decode(output.read_bytes())
    if struct.unpack_from("<I", verify, len(verify) - 4)[0] != checksum(verify):
        output.unlink(missing_ok=True)
        raise ValueError("output checksum verification failed")
    for offset, expected, _label in HOST_PATCHES:
        area = bytes(verify[offset:offset + len(expected)])
        if area != LOOPBACK.ljust(len(expected), b"\0"):
            output.unlink(missing_ok=True)
            raise ValueError("output host verification failed")
    for marker in PRESERVED_MARKERS:
        if marker not in verify:
            output.unlink(missing_ok=True)
            raise ValueError(f"output lost marker {marker!r}")

    changed = [i for i, (a, b) in enumerate(zip(original, verify)) if a != b]
    return {
        "version": LAB_VERSION.decode(),
        "size": len(encoded),
        "sha256": hashlib.sha256(encoded).hexdigest(),
        "checksum": f"{new_checksum:08x}",
        "changed_decoded_bytes": len(changed),
        "first_change": f"0x{min(changed):x}",
        "last_change": f"0x{max(changed):x}",
    }
