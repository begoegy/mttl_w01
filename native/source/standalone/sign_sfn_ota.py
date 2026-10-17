#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib, struct, sys
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ed25519
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from standalone.ota import inspect_image

DOMAIN=b'SFN-OTA-ED25519\x00'

def message(sequence:int,size:int,digest:bytes)->bytes:
    if not 1<=sequence<=0xffffffff: raise ValueError('invalid OTA sequence')
    if not 44<=size<=0x6A014 or len(digest)!=32: raise ValueError('invalid OTA identity')
    return DOMAIN+struct.pack('>II',sequence,size)+digest

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--private-key',required=True);ap.add_argument('--fwr',required=True);ap.add_argument('--sequence',required=True,type=int);ap.add_argument('--out',required=True);a=ap.parse_args()
    data=Path(a.fwr).read_bytes();decoded=inspect_image(data).decoded
    marker=("SFN_NATIVE_REMOTE_OTA_SAFE;SEQ="+str(a.sequence)).encode("ascii")
    if marker not in decoded: raise SystemExit("FWR embedded OTA sequence/profile does not match signing sequence")
    digest=hashlib.sha256(data).digest()
    key=serialization.load_pem_private_key(Path(a.private_key).read_bytes(),password=None)
    if not isinstance(key,ed25519.Ed25519PrivateKey): raise SystemExit('Ed25519 private key required')
    sig=key.sign(message(a.sequence,len(data),digest))
    if len(sig)!=64: raise SystemExit('unexpected Ed25519 signature length')
    Path(a.out).write_bytes(sig)
    print('OTA_SEQUENCE='+str(a.sequence));print('FWR_SHA256='+digest.hex());print('OTA_SIGNATURE_SHA256='+hashlib.sha256(sig).hexdigest())
if __name__=='__main__':main()
