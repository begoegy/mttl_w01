#!/usr/bin/env python3
"""Build a research-only MTTL-W01 neutral 1.0.66 lab candidate.

This tool is intentionally exact-image gated. It does not flash a device and it does
not install the output into any SFN Power production firmware catalog.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

EXPECTED_SIZE = 327944
EXPECTED_SHA256 = "d780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04"
XOR_KEY = bytes.fromhex("3f5a27e8d8fb85f9bd4d51196c4f5159")
NOP32 = bytes.fromhex("00bf00bf")
LAB_VERSION_OFFSET = 0x18E64
LAB_VERSION_SOURCE = b"1.0.66"
LAB_VERSION = b"1.0.69"
PATCHES = (
    (0x3A3E, bytes.fromhex("06f01bfb"), "disable check_ota_ssl_task creation"),
    (0x197A4, bytes.fromhex("f0f768fc"), "disable ota_task / ota_ssl_task creation"),
    (0x1D6C2, bytes.fromhex("ecf7d9fc"), "disable LGtonlyoneM2MTask creation"),
)
PRESERVED_MARKERS = (
    b"LGU+-TAP-HW002",
    b"MTTL-W01",
    b"0.1.54",
    b"1.0.66",
    b"up:ip:ip_ok",
    b"up:connect:connect_ok",
    b"up:ota:ota_ok",
    b"up:reboot_ok",
    b"wifirssi",
    b"up:query:%d",
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
        raise SystemExit(f"REFUSE: source SHA-256 {digest} != {EXPECTED_SHA256}")
    decoded = decode(raw)
    stored = struct.unpack_from("<I", decoded, len(decoded) - 4)[0]
    calculated = checksum(decoded)
    if stored != calculated:
        raise SystemExit(f"REFUSE: source checksum {stored:08x} != {calculated:08x}")
    missing = [m.decode("ascii") for m in PRESERVED_MARKERS if m not in decoded]
    if missing:
        raise SystemExit("REFUSE: source markers missing: " + ", ".join(missing))
    for offset, expected, label in PATCHES:
        actual = bytes(decoded[offset:offset + 4])
        if actual != expected:
            raise SystemExit(
                f"REFUSE: {label}: decoded bytes at 0x{offset:x} are {actual.hex()}, expected {expected.hex()}"
            )
    actual_version = bytes(decoded[LAB_VERSION_OFFSET:LAB_VERSION_OFFSET + len(LAB_VERSION_SOURCE)])
    if actual_version != LAB_VERSION_SOURCE:
        raise SystemExit(
            f"REFUSE: version bytes at 0x{LAB_VERSION_OFFSET:x} are {actual_version!r}, expected {LAB_VERSION_SOURCE!r}"
        )
    return decoded


def build(source: Path, output: Path) -> dict[str, object]:
    raw = source.read_bytes()
    original = validate_source(raw)
    patched = bytearray(original)

    for offset, _expected, _label in PATCHES:
        patched[offset:offset + 4] = NOP32
    patched[LAB_VERSION_OFFSET:LAB_VERSION_OFFSET + len(LAB_VERSION)] = LAB_VERSION

    new_checksum = checksum(patched)
    struct.pack_into("<I", patched, len(patched) - 4, new_checksum)

    output_markers = tuple(m for m in PRESERVED_MARKERS if m != LAB_VERSION_SOURCE) + (LAB_VERSION,)
    missing = [m.decode("ascii") for m in output_markers if m not in patched]
    if missing:
        raise SystemExit("REFUSE: patched markers missing: " + ", ".join(missing))
    if LAB_VERSION_SOURCE in patched:
        raise SystemExit("REFUSE: stock version marker still present after lab identity patch")

    allowed = set(range(len(patched) - 4, len(patched)))
    for offset, _expected, _label in PATCHES:
        allowed.update(range(offset, offset + 4))
    allowed.update(range(LAB_VERSION_OFFSET, LAB_VERSION_OFFSET + len(LAB_VERSION)))
    changed = [i for i, (a, b) in enumerate(zip(original, patched)) if a != b]
    unexpected = [i for i in changed if i not in allowed]
    if unexpected:
        raise SystemExit(f"REFUSE: unexpected decoded changes begin at 0x{unexpected[0]:x}")

    encoded = encode(patched)
    output.parent.mkdir(parents=True, exist_ok=True)
    temp = output.with_suffix(output.suffix + ".tmp")
    temp.write_bytes(encoded)
    temp.replace(output)

    verify = decode(output.read_bytes())
    stored = struct.unpack_from("<I", verify, len(verify) - 4)[0]
    calculated = checksum(verify)
    if stored != calculated or stored != new_checksum:
        output.unlink(missing_ok=True)
        raise SystemExit("REFUSE: output checksum verification failed")
    for offset, _expected, label in PATCHES:
        if bytes(verify[offset:offset + 4]) != NOP32:
            output.unlink(missing_ok=True)
            raise SystemExit(f"REFUSE: output verification failed for {label}")

    return {
        "source_sha256": EXPECTED_SHA256,
        "output_sha256": hashlib.sha256(encoded).hexdigest(),
        "size": len(encoded),
        "internal_checksum": f"{new_checksum:08x}",
        "decoded_changed_bytes": len(changed),
        "patch_offsets": [f"0x{o:x}" for o, _, _ in PATCHES],
        "reported_lab_version": LAB_VERSION.decode("ascii"),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--ack-lab-only", action="store_true", help="required acknowledgement that output is not production-approved")
    args = parser.parse_args()
    if not args.ack_lab_only:
        raise SystemExit("REFUSE: pass --ack-lab-only; this builder creates a research candidate only")
    meta = build(args.source, args.output)
    print("LAB-ONLY NEUTRAL CANDIDATE BUILT")
    for key, value in meta.items():
        print(f"{key}={value}")
    print(f"output={args.output}")
    print("NOT APPROVED FOR PRODUCTION OR CUSTOMER FLASHING")


if __name__ == "__main__":
    main()
