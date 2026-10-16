import hashlib
import tempfile
import unittest
from pathlib import Path

from tools.sfn_build_local_queue_1_0_150_from_1_0_146 import build, decode, checksum

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'work/firmware/1.0.146/comMTTL-W01_1.0.146.fwr'
EXPECTED = '14a89fdf7cff3961d6bc116c10601ff2b319656c557ea7fc7460d85f64aaa9b4'


class LocalQueue150BuilderTest(unittest.TestCase):
    def test_exact_source_build_is_reproducible_and_pinned(self):
        with tempfile.TemporaryDirectory() as td:
            a = Path(td) / 'a.fwr'
            b = Path(td) / 'b.fwr'
            ma = build(SOURCE, a)
            mb = build(SOURCE, b)
            self.assertEqual(a.read_bytes(), b.read_bytes())
            self.assertEqual(hashlib.sha256(a.read_bytes()).hexdigest(), EXPECTED)
            self.assertEqual(ma['output_sha256'], EXPECTED)
            self.assertEqual(mb['output_sha256'], EXPECTED)
            decoded = decode(a.read_bytes())
            self.assertEqual(decoded[0x18E64:0x18E64 + 7], b'1.0.150')
            self.assertEqual(decoded.count(b'127.0.0.1'), 4)
            self.assertEqual(int.from_bytes(decoded[-4:], 'little'), checksum(decoded))

    def test_wrong_source_refuses(self):
        with tempfile.TemporaryDirectory() as td:
            bad = Path(td) / 'bad.fwr'
            raw = bytearray(SOURCE.read_bytes())
            raw[100] ^= 1
            bad.write_bytes(raw)
            with self.assertRaises(SystemExit):
                build(bad, Path(td) / 'out.fwr')


if __name__ == '__main__':
    unittest.main()
