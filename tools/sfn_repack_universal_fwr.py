#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path

XOR_BASE=bytes.fromhex('ca5923e7d79a84e843b3afe794b1afa7')
GENERIC_HEADER_TAIL=b'\xff'*8
EXPECTED_HW=b'LGU+-TAP-HW002'
EXPECTED_MODEL=b'MTTL-W01'
LOAD=0x10006000

def fwr_key(header: bytes) -> bytes:
    if len(header)!=16: raise ValueError('header length')
    return bytes(((seed ^ value)+1)&0xff for seed,value in zip(XOR_BASE,header))

def transform(data: bytes) -> bytes:
    key=fwr_key(data[:16])
    return data[:16]+bytes(v ^ key[i%16] for i,v in enumerate(data[16:],16))

def checksum(decoded: bytes) -> int:
    return sum(decoded[:-4]) & 0xffffffff

def decode_fwr(raw: bytes) -> bytes:
    d=transform(raw)
    size,load=struct.unpack_from('<II',d,0)
    if load!=LOAD or len(d)!=size+20: raise ValueError('image2 geometry')
    if struct.unpack_from('<I',d,len(d)-4)[0]!=checksum(d): raise ValueError('checksum')
    if EXPECTED_HW not in d or EXPECTED_MODEL not in d: raise ValueError('hardware/model')
    return d

def build(source: Path, output: Path) -> dict[str,object]:
    raw=source.read_bytes(); d=bytearray(decode_fwr(raw)); before=bytes(d)
    d[8:16]=GENERIC_HEADER_TAIL
    struct.pack_into('<I',d,len(d)-4,checksum(d))
    # Encoding transform is symmetric, but the key must derive from the NEW 16-byte header.
    out=transform(bytes(d))
    verify=decode_fwr(out)
    if verify[8:16]!=GENERIC_HEADER_TAIL: raise ValueError('generic header verify')
    # Apart from envelope bytes 8..15 and trailing checksum, application bytes must be identical.
    if verify[16:-4] != before[16:-4]: raise ValueError('application body changed')
    output.parent.mkdir(parents=True,exist_ok=True); output.write_bytes(out)
    return {
      'source_sha256':hashlib.sha256(raw).hexdigest(),
      'output_sha256':hashlib.sha256(out).hexdigest(),
      'size':len(out),
      'application_body_identical':True,
      'generic_header_tail':verify[8:16].hex(),
      'internal_checksum':f'{checksum(verify):08x}',
    }

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('source',type=Path); ap.add_argument('output',type=Path); ap.add_argument('--lab-only',action='store_true'); a=ap.parse_args()
    if not a.lab_only: raise SystemExit('REFUSE: pass --lab-only; no device flashing is authorized')
    meta=build(a.source,a.output)
    for k,v in meta.items(): print(f'{k}={v}')
    print('NO_DEVICE_IO NO_FLASH NO_OTA NO_DEPLOY')
if __name__=='__main__': main()
