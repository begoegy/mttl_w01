import hashlib, importlib.util, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('repack',ROOT/'tools/sfn_repack_universal_fwr.py'); m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
SOURCE=Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.150-local-queue-hw002-r1b-lab.fwr')
class UniversalEnvelopeTest(unittest.TestCase):
 def test_repack_is_reproducible_and_body_identical(self):
  with tempfile.TemporaryDirectory() as td:
   a=Path(td)/'a.fwr'; b=Path(td)/'b.fwr'; ma=m.build(SOURCE,a); mb=m.build(SOURCE,b)
   self.assertEqual(a.read_bytes(),b.read_bytes()); self.assertEqual(ma['output_sha256'],mb['output_sha256'])
   old=m.decode_fwr(SOURCE.read_bytes()); new=m.decode_fwr(a.read_bytes())
   self.assertEqual(new[8:16],b'\xff'*8); self.assertEqual(old[16:-4],new[16:-4])
 def test_source_is_exact_expected_candidate(self):
  self.assertEqual(hashlib.sha256(SOURCE.read_bytes()).hexdigest(),'14a89fdf7cff3961d6bc116c10601ff2b319656c557ea7fc7460d85f64aaa9b4')
if __name__=='__main__': unittest.main()
