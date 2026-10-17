#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, importlib.util, struct
from pathlib import Path

EXPECTED_SOURCE_SHA='e70ba96726dc2a0093685fe5f5fb9e0bc8c71017a6676c51bf2abb3301a76098'
XOR_BASE=bytes.fromhex('ca5923e7d79a84e843b3afe794b1afa7')
GENERIC_HEADER_TAIL=b'\xff'*8
LOAD=0x10006000
EXPECTED_HW=b'LGU+-TAP-HW002'; EXPECTED_MODEL=b'MTTL-W01'; EXPECTED_VERSION=b'1.0.151'

def fwr_key(header): return bytes(((seed^value)+1)&0xff for seed,value in zip(XOR_BASE,header))
def transform(data):
    key=fwr_key(data[:16]); return data[:16]+bytes(v^key[i%16] for i,v in enumerate(data[16:],16))
def checksum(d): return sum(d[:-4])&0xffffffff
def decode_fwr(raw):
    d=transform(raw); size,load=struct.unpack_from('<II',d,0)
    if load!=LOAD or len(d)!=size+20: raise ValueError('image2 geometry')
    if struct.unpack_from('<I',d,len(d)-4)[0]!=checksum(d): raise ValueError('checksum')
    for marker in (EXPECTED_HW,EXPECTED_MODEL,EXPECTED_VERSION,b'childlock',b'up:cl:%d'):
        if marker not in d: raise ValueError(f'marker {marker!r}')
    return d

def build(source,output):
    raw=source.read_bytes(); digest=hashlib.sha256(raw).hexdigest()
    if digest!=EXPECTED_SOURCE_SHA: raise ValueError(f'source SHA {digest}')
    d=bytearray(decode_fwr(raw)); before=bytes(d)
    d[8:16]=GENERIC_HEADER_TAIL; struct.pack_into('<I',d,len(d)-4,checksum(d))
    out=transform(bytes(d)); verify=decode_fwr(out)
    if verify[8:16]!=GENERIC_HEADER_TAIL: raise ValueError('header')
    if verify[16:-4]!=before[16:-4]: raise ValueError('application body changed')
    output.parent.mkdir(parents=True,exist_ok=True); output.write_bytes(out)
    return {'source_sha256':digest,'output_sha256':hashlib.sha256(out).hexdigest(),'size':len(out),'application_body_identical':True,'generic_header_tail':verify[8:16].hex()}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('source',type=Path); ap.add_argument('output',type=Path); ap.add_argument('--lab-only',action='store_true'); a=ap.parse_args()
    if not a.lab_only: raise SystemExit('REFUSE: pass --lab-only')
    for k,v in build(a.source,a.output).items(): print(f'{k}={v}')
    print('NO DEVICE IO; NO OTA; NO CATALOG')
if __name__=='__main__': main()
