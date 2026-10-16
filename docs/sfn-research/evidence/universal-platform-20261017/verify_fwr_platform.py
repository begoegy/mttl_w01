#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, re, struct
from pathlib import Path
XOR_BASE=bytes.fromhex('ca5923e7d79a84e843b3afe794b1afa7')
TABLE_TAIL=bytes.fromhex('52544b57696e00ff000101009581010100000000')
EXPECTED_LOAD=0x10006000
EXPECTED_ENTRY=0x10006071
EXPECTED_HW=b'LGU+-TAP-HW002'

def fwr_key(header: bytes) -> bytes:
    if len(header)!=16: raise ValueError('header')
    return bytes(((seed ^ value)+1)&255 for seed,value in zip(XOR_BASE,header))

def decode(raw: bytes) -> bytes:
    key=fwr_key(raw[:16])
    return raw[:16]+bytes(value^key[index%16] for index,value in enumerate(raw[16:],16))

def inspect(path: Path):
    raw=path.read_bytes(); d=decode(raw)
    size,load=struct.unpack_from('<II',d); entry=struct.unpack_from('<I',d,16)[0]
    checksum=struct.unpack_from('<I',d,len(d)-4)[0]
    versions=sorted(set(x.decode() for x in re.findall(rb'(?<![0-9.])[0-9]{1,2}\.[0-9]{1,3}\.[0-9]{1,3}(?![0-9.])',d[16:-4])))
    result={
      'file':path.name,'sha256':hashlib.sha256(raw).hexdigest(),'size':len(raw),
      'body_size':size,'load':f'0x{load:08x}','entry':f'0x{entry:08x}',
      'length_ok':len(d)==size+20,'checksum_ok':checksum==(sum(d[:-4])&0xffffffff),
      'table_ok':d[20:40]==TABLE_TAIL,'hardware_offset':d.find(EXPECTED_HW),
      'model_offset':d.find(b'MTTL-W01'),'versions':versions,
      'platform_ok':load==EXPECTED_LOAD and entry==EXPECTED_ENTRY and d[20:40]==TABLE_TAIL and d.find(EXPECTED_HW)==0x28 and d.find(b'MTTL-W01')>=0,
    }
    return result

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('files',nargs='+',type=Path); args=ap.parse_args()
    rows=[inspect(p) for p in args.files]
    for r in rows:
        print(' | '.join(f'{k}={v}' for k,v in r.items()))
    if not all(r['platform_ok'] and r['length_ok'] and r['checksum_ok'] for r in rows):
        raise SystemExit(2)
    print(f'UNIVERSAL_PLATFORM_SIGNATURE=PASS count={len(rows)}')
if __name__=='__main__': main()
