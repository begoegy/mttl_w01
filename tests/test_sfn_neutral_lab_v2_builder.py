import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

from tools import sfn_build_neutral_lab_v2_1_0_66 as builder

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "work/firmware/1.0.66/comMTTL-W01_1.0.66.fwr"
EXPECTED_SHA256 = "0cbf05da978ef3985439e9a1d40e71a8bcd5dea7988a800217615873783472a2"


class NeutralLabV2BuilderTest(unittest.TestCase):
    def test_exact_pinned_stock_build(self):
        with tempfile.TemporaryDirectory() as td:
            output = Path(td) / "neutral-v2.fwr"
            meta = builder.build(SOURCE, output)
            self.assertEqual(meta["sha256"], EXPECTED_SHA256)
            self.assertEqual(hashlib.sha256(output.read_bytes()).hexdigest(), EXPECTED_SHA256)
            self.assertEqual(output.stat().st_size, builder.EXPECTED_SIZE)

            stock = builder.decode(SOURCE.read_bytes())
            built = builder.decode(output.read_bytes())
            self.assertEqual(struct.unpack_from("<I", built, len(built) - 4)[0], builder.checksum(built))
            self.assertEqual(bytes(built[builder.LAB_VERSION_OFFSET:builder.LAB_VERSION_OFFSET + 6]), builder.LAB_VERSION)

            for offset, expected, _label in builder.HOST_PATCHES:
                self.assertEqual(bytes(stock[offset:offset + len(expected)]), expected)
                self.assertEqual(bytes(built[offset:offset + len(expected)]), builder.LOOPBACK.ljust(len(expected), b"\0"))
                self.assertNotIn(expected, built)

            for marker in builder.PRESERVED_MARKERS:
                self.assertIn(marker, built)

            # V2 deliberately leaves the old task-creation instructions intact.
            for offset, expected in (
                (0x3A3E, bytes.fromhex("06f01bfb")),
                (0x197A4, bytes.fromhex("f0f768fc")),
                (0x1D6C2, bytes.fromhex("ecf7d9fc")),
            ):
                self.assertEqual(bytes(built[offset:offset + 4]), expected)

    def test_refuses_modified_source(self):
        with tempfile.TemporaryDirectory() as td:
            bad = Path(td) / "bad.fwr"
            raw = bytearray(SOURCE.read_bytes())
            raw[1234] ^= 1
            bad.write_bytes(raw)
            with self.assertRaises(ValueError):
                builder.build(bad, Path(td) / "out.fwr")


if __name__ == "__main__":
    unittest.main()
