#!/usr/bin/env python3
"""Build SFN Neutral 1.0.72 offline-scheduler lab image from the proven New 1.0.71 canary.

No flashing. Exact-source gated. Ports the already-emulated scheduler only because the exact 1.0.71 source has the same approved
injection regions/call ABI as the 1.0.148 family. No device I/O is performed by this builder.
"""
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path
from tools.sfn_build_neutral_lab_v2_1_0_66 import decode, encode, checksum
LOAD_ADDRESS = 0x10006000
from tools.sfn_scheduler_segments import build_segments, MAIN_LIMIT, ACK_LIMIT

EXPECTED_SIZE = 327944
EXPECTED_SHA256 = 'c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081'
SOURCE_VERSION=b'1.0.71'; TARGET_VERSION=b'1.0.72'
VERSION_OFFSET=0x18E64
MEMORY_COMMAND_OFFSET=0x106AC
MEMORY_COMMAND_SOURCE=b'memory\x00'
MEMORY_COMMAND_TARGET=b'sfn\x00\x00\x00\x00'
MEMORY_FORMAT_OFFSET=0x106B4
MEMORY_FORMAT_SOURCE=b'up:memory:%d:%d\r\n\x00'
MEMORY_FORMAT_TARGET=b'up:sfn:%d:%d:%d\r\n\x00'
PRESERVED_MARKERS=(b'LGU+-TAP-HW002',b'MTTL-W01',b'0.1.54',b'up:ip:ip_ok',b'up:connect:connect_ok',b'up:ota:ota_ok',b'up:reboot_ok',b'up:test:started',b'up:test:stopped',b'wifirssi',b'up:query:%d')

def mem_off(addr:int)->int:
    return addr-LOAD_ADDRESS+16

def source(raw:bytes)->bytearray:
    if len(raw)!=EXPECTED_SIZE: raise SystemExit(f'REFUSE: source size {len(raw)} != {EXPECTED_SIZE}')
    digest=hashlib.sha256(raw).hexdigest()
    if digest!=EXPECTED_SHA256: raise SystemExit(f'REFUSE: source SHA-256 {digest} != pinned {EXPECTED_SHA256}')
    d=decode(raw)
    stored=struct.unpack_from('<I',d,len(d)-4)[0]
    if stored!=checksum(d): raise SystemExit('REFUSE: source checksum mismatch')
    if bytes(d[VERSION_OFFSET:VERSION_OFFSET+len(SOURCE_VERSION)])!=SOURCE_VERSION: raise SystemExit('REFUSE: source version mismatch')
    if bytes(d[MEMORY_COMMAND_OFFSET:MEMORY_COMMAND_OFFSET+len(MEMORY_COMMAND_SOURCE)])!=MEMORY_COMMAND_SOURCE: raise SystemExit('REFUSE: memory command layout mismatch')
    if bytes(d[MEMORY_FORMAT_OFFSET:MEMORY_FORMAT_OFFSET+len(MEMORY_FORMAT_SOURCE)])!=MEMORY_FORMAT_SOURCE: raise SystemExit('REFUSE: memory response layout mismatch')
    if d.count(b'127.0.0.1')!=4: raise SystemExit('REFUSE: source is not four-host neutral image')
    for m in PRESERVED_MARKERS:
        if m not in d: raise SystemExit(f'REFUSE: source marker missing {m!r}')
    return d

def put_mem(buf:bytearray, addr:int, data:bytes):
    o=mem_off(addr); buf[o:o+len(data)]=data

def build(src:Path,out:Path)->dict:
    original=source(src.read_bytes()); patched=bytearray(original)
    seg=build_segments()
    patched[VERSION_OFFSET:VERSION_OFFSET+len(TARGET_VERSION)]=TARGET_VERSION
    patched[MEMORY_COMMAND_OFFSET:MEMORY_COMMAND_OFFSET+len(MEMORY_COMMAND_TARGET)]=MEMORY_COMMAND_TARGET
    patched[MEMORY_FORMAT_OFFSET:MEMORY_FORMAT_OFFSET+len(MEMORY_FORMAT_TARGET)]=MEMORY_FORMAT_TARGET
    # Fill only previously reviewed/reclaimed regions. Remaining bytes keep stock alignment/data intact.
    for name in ('task','memory','main','ack'):
        addr,data=seg[name]; put_mem(patched,addr,data)
    # Neutral cloud downloader is gone; task body now owns scheduler. Main code may never cross version marker.
    assert seg['main'][0]+len(seg['main'][1])<=MAIN_LIMIT
    assert seg['ack'][0]+len(seg['ack'][1])<=ACK_LIMIT
    # Only explicitly approved decoded ranges may differ.
    allowed=set(range(VERSION_OFFSET,VERSION_OFFSET+len(TARGET_VERSION)))
    allowed.update(range(MEMORY_COMMAND_OFFSET,MEMORY_COMMAND_OFFSET+len(MEMORY_COMMAND_TARGET)))
    allowed.update(range(MEMORY_FORMAT_OFFSET,MEMORY_FORMAT_OFFSET+len(MEMORY_FORMAT_TARGET)))
    for name in ('task','memory','main','ack'):
        addr,data=seg[name]; o=mem_off(addr); allowed.update(range(o,o+len(data)))
    allowed.update(range(len(patched)-4,len(patched)))
    struct.pack_into('<I',patched,len(patched)-4,checksum(patched))
    changed=[i for i,(a,b) in enumerate(zip(original,patched)) if a!=b]
    bad=[i for i in changed if i not in allowed]
    if bad: raise SystemExit(f'REFUSE: unexpected decoded change at 0x{bad[0]:x}')
    if patched.count(b'127.0.0.1')!=4: raise SystemExit('REFUSE: neutral host isolation changed')
    for m in PRESERVED_MARKERS:
        if m not in patched: raise SystemExit(f'REFUSE: output marker missing {m!r}')
    if b'up:sfn:%d:%d:%d\r\n' not in patched: raise SystemExit('REFUSE: scheduler ACK format missing')
    if bytes(patched[VERSION_OFFSET:VERSION_OFFSET+len(TARGET_VERSION)])!=TARGET_VERSION: raise SystemExit('REFUSE: target version verification failed')
    encoded=encode(patched)
    out.parent.mkdir(parents=True,exist_ok=True); tmp=out.with_suffix(out.suffix+'.tmp'); tmp.write_bytes(encoded); tmp.replace(out)
    check=decode(out.read_bytes())
    if struct.unpack_from('<I',check,len(check)-4)[0]!=checksum(check): out.unlink(missing_ok=True); raise SystemExit('REFUSE: output checksum verification failed')
    return {'source_sha256':EXPECTED_SHA256,'output_sha256':hashlib.sha256(encoded).hexdigest(),'size':len(encoded),'decoded_changed_bytes':len(changed),'target_version':'1.0.72','task_bytes':len(seg['task'][1]),'main_bytes':len(seg['main'][1]),'ack_bytes':len(seg['ack'][1]),'handler':hex(seg['handler_addr']),'relay':hex(seg['relay_addr'])}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('source',type=Path); ap.add_argument('output',type=Path); ap.add_argument('--ack-lab-only',action='store_true'); a=ap.parse_args()
    if not a.ack_lab_only: raise SystemExit('REFUSE: pass --ack-lab-only')
    meta=build(a.source,a.output); print('LAB-ONLY OFFLINE-SCHEDULER CANDIDATE BUILT')
    for k,v in meta.items(): print(f'{k}={v}')
    print('NO DEVICE WAS FLASHED')
if __name__=='__main__': main()
