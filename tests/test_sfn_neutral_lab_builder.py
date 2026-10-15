import hashlib
import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "work/firmware/1.0.66/comMTTL-W01_1.0.66.fwr"
TOOL = ROOT / "tools/sfn_build_neutral_lab_1_0_66.py"
EXPECTED_OUTPUT_SHA256 = "653ee4dfb9897964e251773dc7e0bafaf14212066472fc9fd551ca553a754484"

spec = importlib.util.spec_from_file_location("neutral_builder", TOOL)
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class NeutralLabBuilderTest(unittest.TestCase):
    def test_exact_stock_build_has_only_approved_decoded_diffs(self):
        with tempfile.TemporaryDirectory() as td:
            output = Path(td) / "neutral-lab.fwr"
            meta = builder.build(SOURCE, output)
            self.assertEqual(hashlib.sha256(output.read_bytes()).hexdigest(), EXPECTED_OUTPUT_SHA256)
            self.assertEqual(meta["size"], builder.EXPECTED_SIZE)

            original = builder.decode(SOURCE.read_bytes())
            patched = builder.decode(output.read_bytes())
            changed = {i for i, pair in enumerate(zip(original, patched)) if pair[0] != pair[1]}
            allowed = set(range(len(patched) - 4, len(patched)))
            for offset, _, _ in builder.PATCHES:
                allowed.update(range(offset, offset + 4))
                self.assertEqual(bytes(patched[offset:offset + 4]), builder.NOP32)
            self.assertTrue(changed <= allowed)

            stored = struct.unpack_from("<I", patched, len(patched) - 4)[0]
            self.assertEqual(stored, builder.checksum(patched))
            for marker in builder.PRESERVED_MARKERS:
                self.assertIn(marker, patched)

    def test_refuses_any_non_pinned_source(self):
        with tempfile.TemporaryDirectory() as td:
            bad = Path(td) / "bad.fwr"
            data = bytearray(SOURCE.read_bytes())
            data[100] ^= 1
            bad.write_bytes(data)
            with self.assertRaises(SystemExit):
                builder.build(bad, Path(td) / "out.fwr")


if __name__ == "__main__":
    unittest.main()
