#!/usr/bin/env python3
from pathlib import Path
import hashlib
import struct
import sys

EXPECTED_SIZE = 327944
EXPECTED_SHA256 = "d780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04"
XOR_KEY = bytes.fromhex("3f5a27e8d8fb85f9bd4d51196c4f5159")
REQUIRED_MARKERS = (
    b"LGU+-TAP-HW002",
    b"MTTL-W01",
    b"0.1.54",
    b"1.0.66",
    b"wifirssi",
    b"up:query:%d",
    b"up:ota:ota_ok",
)


def validate(path: Path) -> None:
    raw = path.read_bytes()
    if len(raw) != EXPECTED_SIZE:
        raise SystemExit(f"REFUSE: size {len(raw)} != {EXPECTED_SIZE}")
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"REFUSE: sha256 {digest} != {EXPECTED_SHA256}")

    decoded = bytearray(raw)
    for index in range(16, len(decoded)):
        decoded[index] ^= XOR_KEY[index % 16]

    stored = struct.unpack_from("<I", decoded, len(decoded) - 4)[0]
    calculated = sum(decoded[:-4]) & 0xFFFFFFFF
    if stored != calculated:
        raise SystemExit(f"REFUSE: internal checksum {stored:08x} != {calculated:08x}")

    missing = [marker.decode("ascii") for marker in REQUIRED_MARKERS if marker not in decoded]
    if missing:
        raise SystemExit("REFUSE: missing markers: " + ", ".join(missing))

    print("VALID STOCK MTTL-W01 1.0.66")
    print(f"size={len(raw)}")
    print(f"sha256={digest}")
    print(f"internal_checksum={stored:08x}")
    print("hardware_marker=LGU+-TAP-HW002")
    print("boot_components=0.1.54 + 1.0.66")
    print("rssi_protocol=wifirssi -> up:query:%d")
    print("local_ota_ack=up:ota:ota_ok")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} FIRMWARE.fwr")
    validate(Path(sys.argv[1]))
