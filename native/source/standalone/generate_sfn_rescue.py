#!/usr/bin/env python3
"""Generate a build-only exact rescue descriptor. Never writes a device."""
from pathlib import Path
import argparse, hashlib, sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from standalone.ota import inspect_image
p=argparse.ArgumentParser()
p.add_argument('--fwr',required=True)
p.add_argument('--sha256',required=True)
p.add_argument('--out',required=True)
a=p.parse_args()
raw=Path(a.fwr).read_bytes()
image=inspect_image(raw,a.sha256)
decoded=bytearray(image.decoded)
if len(decoded)<4096: raise SystemExit('rescue image too small')
decoded[8:16]=b'\xff'*8
sector=bytes(decoded[:4096])
digest=hashlib.sha256(decoded).digest()
tail_digest=hashlib.sha256(decoded[4096:]).digest()
def arr(name,data):
    return 'static const unsigned char %s[%d]={%s};\n'%(name,len(data),','.join(str(x) for x in data))
out=Path(a.out)
out.write_text(
    '#ifndef SFN_RESCUE_IMAGE_H\n#define SFN_RESCUE_IMAGE_H\n'
    '#define SFN_RESCUE_FILE_BYTES %du\n'%len(decoded)+
    '#define SFN_RESCUE_FWR_SHA256 "%s"\n'%hashlib.sha256(raw).hexdigest()+
    arr('sfn_rescue_normalized_sha256',digest)+
    arr('sfn_rescue_tail_sha256',tail_digest)+
    arr('sfn_rescue_sector0',sector)+
    '#endif\n',encoding='ascii')
print('RESCUE_HEADER=PASS bytes=%d normalized_sha256=%s sector_sha256=%s'%(len(decoded),digest.hex(),hashlib.sha256(sector).hexdigest()))
