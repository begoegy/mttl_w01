"""Build a LAB-ONLY neutral candidate from the exact public Voltra 1.0.146 image.

The source is the unauthenticated public OTA image currently served at
http://5.182.18.10/ota.bin with Host mef.onem2m.uplus.co.kr.  This builder is
strictly pinned to its size/SHA/checksum and changes only the four Voltra service
host strings, the displayed firmware version, and the trailing additive checksum.
The normal UP controller destination configured through up:ip is preserved.
"""
from __future__ import annotations

import hashlib
import struct
from pathlib import Path

EXPECTED_SIZE = 327944
EXPECTED_SHA256 = "ae997d79f8e54376a4eb94e447cf94582311544aa128ff4a487d2190f67208ad"
XOR_KEY = bytes.fromhex("3f5a27e8d8fb85f97c8397d3ad879f97")
VERSION_OFFSET = 0x18E64
SOURCE_VERSION = b"1.0.146"
TARGET_VERSION = b"1.0.147"
LOOPBACK = b"127.0.0.1"
HOST_PATCHES = (
    (0x20594, b"log.toi.ommeq.com", "QMS TLS host"),
    (0x20660, b"log.toi.ommeq.com", "QMS HTTP host"),
    (0x4DCFC, b"dev.toi.ommeq.com", "MEF/certificate host"),
    (0x4DD30, b"dev.toi.ommeq.com", "MQTT host"),
)
PRESERVED_MARKERS = (
    b"LGU+-TAP-HW002", b"MTTL-W01", b"0.1.55",
    b"up:ip:ip_ok", b"up:connect:connect_ok", b"up:ota:ota_ok",
    b"up:reboot_ok", b"wifirssi", b"ota.bin",
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
    if bytes(decoded[VERSION_OFFSET:VERSION_OFFSET + len(SOURCE_VERSION)]) != SOURCE_VERSION:
        raise ValueError("unexpected source version bytes")
    for offset, expected, label in HOST_PATCHES:
        if bytes(decoded[offset:offset + len(expected)]) != expected:
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
    patched[VERSION_OFFSET:VERSION_OFFSET + len(SOURCE_VERSION)] = TARGET_VERSION
    new_checksum = checksum(patched)
    struct.pack_into("<I", patched, len(patched) - 4, new_checksum)

    allowed = set(range(VERSION_OFFSET, VERSION_OFFSET + len(SOURCE_VERSION)))
    for offset, expected, _label in HOST_PATCHES:
        allowed.update(range(offset, offset + len(expected)))
    allowed.update(range(len(patched) - 4, len(patched)))
    changed = {i for i, (a, b) in enumerate(zip(original, patched)) if a != b}
    if not changed or not changed.issubset(allowed):
        raise ValueError("unexpected decoded-byte changes")

    encoded = encode(patched)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(encoded)
    verify = decode(output.read_bytes())
    if struct.unpack_from("<I", verify, len(verify) - 4)[0] != checksum(verify):
        output.unlink(missing_ok=True)
        raise ValueError("output checksum verification failed")
    for marker in PRESERVED_MARKERS:
        if marker not in verify:
            output.unlink(missing_ok=True)
            raise ValueError(f"output lost marker {marker!r}")
    for offset, expected, _label in HOST_PATCHES:
        if bytes(verify[offset:offset + len(expected)]) != LOOPBACK.ljust(len(expected), b"\0"):
            output.unlink(missing_ok=True)
            raise ValueError("output host verification failed")

    return {
        "version": TARGET_VERSION.decode(),
        "size": len(encoded),
        "sha256": hashlib.sha256(encoded).hexdigest(),
        "checksum": f"{new_checksum:08x}",
        "changed_decoded_bytes": len(changed),
        "first_change": f"0x{min(changed):x}",
        "last_change": f"0x{max(changed):x}",
    }
