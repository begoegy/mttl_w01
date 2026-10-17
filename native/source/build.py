"""Portable source entry point. Invoked by MTTL-Builder with local tool paths."""
from pathlib import Path
import os,subprocess,sys
root=Path(__file__).resolve().parent
for name in ('MTTL_GCC_DIR','MTTL_TERSER_ROOT'):
    if not os.environ.get(name):raise SystemExit('Use MTTL-Builder, or configure '+name)
subprocess.run([sys.executable,str(root/'standalone/build_network_test.py'),'--ota-test','--sensor-test','--relay-test'],cwd=root,check=True)
