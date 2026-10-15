"""Build a LAB-ONLY 1.0.71 maintenance image from the exact proven 1.0.70 canary image.

No functional code or host patch is changed. Only the reported firmware version and
container checksum are updated. This exists solely to prove neutral->neutral remote OTA.
"""
from __future__ import annotations
import hashlib
import struct
from pathlib import Path

EXPECTED_SIZE = 327944
EXPECTED_SHA256 = "0cbf05da978ef3985439e9a1d40e71a8bcd5dea7988a800217615873783472a2"
XOR_KEY = bytes.fromhex("3f5a27e8d8fb85f9bd4d51196c4f5159")
VERSION_OFFSET = 0x18E64
SOURCE_VERSION = b"1.0.70"
TARGET_VERSION = b"1.0.71"
LOOPBACK = b"127.0.0.1"
HOST_AREAS = (
    (0x20594, len(b"hdslog.lguplus.co.kr")),
    (0x20660, len(b"hdslog.lguplus.co.kr")),
    (0x4DCFC, len(b"mef.onem2m.uplus.co.kr")),
    (0x4DD30, len(b"brk2.onem2m.uplus.co.kr")),
)
PRESERVED = (
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

def build(source: Path, output: Path) -> dict[str, object]:
    raw = source.read_bytes()
    if len(raw) != EXPECTED_SIZE:
        raise ValueError("unexpected source size")
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError(f"unexpected source SHA-256: {digest}")
    decoded = decode(raw)
    stored = struct.unpack_from("<I", decoded, len(decoded)-4)[0]
    if stored != checksum(decoded):
        raise ValueError("source checksum mismatch")
    if bytes(decoded[VERSION_OFFSET:VERSION_OFFSET+6]) != SOURCE_VERSION:
        raise ValueError("source version mismatch")
    for offset, length in HOST_AREAS:
        expected = LOOPBACK.ljust(length, b"\0")
        if bytes(decoded[offset:offset+length]) != expected:
            raise ValueError(f"neutral host patch missing at 0x{offset:x}")
    for marker in PRESERVED:
        if marker not in decoded:
            raise ValueError(f"missing preserved marker {marker!r}")

    patched = bytearray(decoded)
    patched[VERSION_OFFSET:VERSION_OFFSET+6] = TARGET_VERSION
    new_sum = checksum(patched)
    struct.pack_into("<I", patched, len(patched)-4, new_sum)
    encoded = encode(patched)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(encoded)

    verify = decode(output.read_bytes())
    changed = [i for i,(a,b) in enumerate(zip(decoded, verify)) if a != b]
    allowed = set(range(VERSION_OFFSET, VERSION_OFFSET+6)) | set(range(len(verify)-4, len(verify)))
    if not changed or any(i not in allowed for i in changed):
        output.unlink(missing_ok=True)
        raise ValueError("unexpected decoded differences")
    if struct.unpack_from("<I", verify, len(verify)-4)[0] != checksum(verify):
        output.unlink(missing_ok=True)
        raise ValueError("output checksum mismatch")
    for offset, length in HOST_AREAS:
        if bytes(verify[offset:offset+length]) != LOOPBACK.ljust(length, b"\0"):
            output.unlink(missing_ok=True)
            raise ValueError("neutral host patch changed")
    return {
        "version": TARGET_VERSION.decode(),
        "size": len(encoded),
        "sha256": hashlib.sha256(encoded).hexdigest(),
        "checksum": f"{new_sum:08x}",
        "changed_decoded_offsets": [hex(i) for i in changed],
    }
