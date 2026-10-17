#!/usr/bin/env python3
"""Build LAB-ONLY SFN Full Local Engine 1.0.151 from exact proven neutral 1.0.147.

No catalog registration and no device I/O. The builder is SHA-pinned and every decoded-byte
change must fall inside an explicitly reviewed command/code/checksum range.
"""
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path
from tools.sfn_build_neutral_lab_1_0_146 import decode, encode, checksum
from tools.sfn_full_local_engine_segments import build_segments

EXPECTED_SIZE=327944
EXPECTED_SHA256='1a3b66c4a43e63aa54615646b50f2c8507c57c3ce604be3f72211616f1379881'
LOAD=0x10006000
VERSION_OFFSET=0x18E64; SOURCE_VERSION=b'1.0.147'; TARGET_VERSION=b'1.0.151'
MEMORY_COMMAND_OFFSET=0x106AC; MEMORY_COMMAND_SOURCE=b'memory\x00\x00'; MEMORY_COMMAND_TARGET=b'sfn\x00\x00\x00\x00\x00'
MEMORY_FORMAT_OFFSET=0x106B4; MEMORY_FORMAT_SOURCE=b'up:memory:%d:%d\r\n\x00'; MEMORY_FORMAT_TARGET=b'up:sfn:%d:%d:%d\r\n\x00'
TEST_COMMAND_OFFSET=0x106E4; TEST_COMMAND_SOURCE=b'test\x00\x00\x00\x00'; TEST_COMMAND_TARGET=b'vguard\x00\x00'
ANCHORS={
  0x10009914:bytes.fromhex('f0b5f5b042a8c82110f082fb02a84ff4'),
  0x1001600C:bytes.fromhex('00f078fcfcf7c1f80646fcf7bbf80346'),
  0x1001606A:bytes.fromhex('00f0bffd451c0ff26860f4f7c4fc0246'),
  0x1001EB6C:bytes.fromhex('2de9f04fdfb04ff0ff300790002440f2'),
  0x1001EE5C:bytes.fromhex('485454502f312e5820323030204f4b00'),
  0x100233D6:bytes.fromhex('2de9f04e96b002a80021cde90011c0e9'),
  0x10023686:bytes.fromhex('1fb50b4c206828b9032200210120ecf7'),
  0x10012918:bytes.fromhex('02eb05122179e078'),
  0x1001AB42:bytes.fromhex('024608460ff2d411'),
  0x100166DC:bytes.fromhex('737461727400000075703a746573743a737461727465640d0a00000073746f700000000075703a746573743a73746f707065640d0a000000'),
}
PRESERVED=(b'LGU+-TAP-HW002',b'MTTL-W01',b'0.1.55',b'childlock',b'up:cl:%d',b'up:ip:ip_ok',b'up:connect:connect_ok',b'up:ota:ota_ok',b'up:reboot_ok',b'wifirssi',b'up:query:%d',b'ota.bin')

def off(addr:int)->int: return addr-LOAD+16

def source(raw:bytes)->bytearray:
    if len(raw)!=EXPECTED_SIZE: raise SystemExit(f'REFUSE: source size {len(raw)} != {EXPECTED_SIZE}')
    digest=hashlib.sha256(raw).hexdigest()
    if digest!=EXPECTED_SHA256: raise SystemExit(f'REFUSE: source SHA-256 {digest} != pinned {EXPECTED_SHA256}')
    d=decode(raw)
    if struct.unpack_from('<I',d,len(d)-4)[0]!=checksum(d): raise SystemExit('REFUSE: source checksum mismatch')
    if bytes(d[VERSION_OFFSET:VERSION_OFFSET+len(SOURCE_VERSION)])!=SOURCE_VERSION: raise SystemExit('REFUSE: source version mismatch')
    if bytes(d[MEMORY_COMMAND_OFFSET:MEMORY_COMMAND_OFFSET+len(MEMORY_COMMAND_SOURCE)])!=MEMORY_COMMAND_SOURCE: raise SystemExit('REFUSE: memory command mismatch')
    if bytes(d[MEMORY_FORMAT_OFFSET:MEMORY_FORMAT_OFFSET+len(MEMORY_FORMAT_SOURCE)])!=MEMORY_FORMAT_SOURCE: raise SystemExit('REFUSE: memory format mismatch')
    if bytes(d[TEST_COMMAND_OFFSET:TEST_COMMAND_OFFSET+len(TEST_COMMAND_SOURCE)])!=TEST_COMMAND_SOURCE: raise SystemExit('REFUSE: test command mismatch')
    for a,v in ANCHORS.items():
        o=off(a)
        if bytes(d[o:o+len(v)])!=v: raise SystemExit(f'REFUSE: source anchor mismatch at {a:#x}')
    if d.count(b'127.0.0.1')!=4: raise SystemExit('REFUSE: expected exact four-host neutral source')
    for m in PRESERVED:
        if m not in d: raise SystemExit(f'REFUSE: source marker missing {m!r}')
    return d

def put(buf:bytearray,addr:int,data:bytes):
    o=off(addr); buf[o:o+len(data)]=data

def build(src:Path,out:Path)->dict:
    original=source(src.read_bytes()); patched=bytearray(original); seg=build_segments()
    patched[VERSION_OFFSET:VERSION_OFFSET+len(TARGET_VERSION)]=TARGET_VERSION
    patched[MEMORY_COMMAND_OFFSET:MEMORY_COMMAND_OFFSET+8]=MEMORY_COMMAND_TARGET
    patched[MEMORY_FORMAT_OFFSET:MEMORY_FORMAT_OFFSET+len(MEMORY_FORMAT_TARGET)]=MEMORY_FORMAT_TARGET
    patched[TEST_COMMAND_OFFSET:TEST_COMMAND_OFFSET+8]=TEST_COMMAND_TARGET
    allowed=set(range(VERSION_OFFSET,VERSION_OFFSET+len(TARGET_VERSION)))
    allowed.update(range(MEMORY_COMMAND_OFFSET,MEMORY_COMMAND_OFFSET+8))
    allowed.update(range(MEMORY_FORMAT_OFFSET,MEMORY_FORMAT_OFFSET+len(MEMORY_FORMAT_TARGET)))
    allowed.update(range(TEST_COMMAND_OFFSET,TEST_COMMAND_OFFSET+8))
    for name in ('task','memory','main','ack','vguard','legacy','relay','hook','relay_patch','wrapper_patch'):
        a,data=seg[name]; put(patched,a,data); o=off(a); allowed.update(range(o,o+len(data)))
    allowed.update(range(len(patched)-4,len(patched)))
    struct.pack_into('<I',patched,len(patched)-4,checksum(patched))
    changed=[i for i,(a,b) in enumerate(zip(original,patched)) if a!=b]
    bad=[i for i in changed if i not in allowed]
    if bad: raise SystemExit(f'REFUSE: unexpected decoded change at 0x{bad[0]:x}')
    if bytes(patched[VERSION_OFFSET:VERSION_OFFSET+7])!=TARGET_VERSION: raise SystemExit('REFUSE: target version mismatch')
    if bytes(patched[MEMORY_COMMAND_OFFSET:MEMORY_COMMAND_OFFSET+8])!=MEMORY_COMMAND_TARGET: raise SystemExit('REFUSE: sfn command mismatch')
    if bytes(patched[TEST_COMMAND_OFFSET:TEST_COMMAND_OFFSET+8])!=TEST_COMMAND_TARGET: raise SystemExit('REFUSE: vguard command mismatch')
    if b'up:sfn:%d:%d:%d\r\n\x00' not in patched: raise SystemExit('REFUSE: SFN ack format missing')
    if patched.count(b'127.0.0.1')!=4: raise SystemExit('REFUSE: neutral host isolation changed')
    for m in PRESERVED:
        if m not in patched: raise SystemExit(f'REFUSE: output marker missing {m!r}')
    encoded=encode(patched); out.parent.mkdir(parents=True,exist_ok=True)
    tmp=out.with_suffix(out.suffix+'.tmp'); tmp.write_bytes(encoded); tmp.replace(out)
    verify=decode(out.read_bytes())
    if struct.unpack_from('<I',verify,len(verify)-4)[0]!=checksum(verify): out.unlink(missing_ok=True); raise SystemExit('REFUSE: output checksum invalid')
    return {
      'source_sha256':EXPECTED_SHA256,'output_sha256':hashlib.sha256(encoded).hexdigest(),'size':len(encoded),
      'decoded_changed_bytes':len(changed),'target_version':'1.0.151','schedule_max_events':38,
      'childlock_preserved':True,'unified_flash_sectors':'0xFC000/0xFD000','oneM2M_task_disabled':True,
      'relay_on_voltage_gate':True,'no_device_io':True,
    }

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('source',type=Path); ap.add_argument('output',type=Path); ap.add_argument('--ack-lab-only',action='store_true'); a=ap.parse_args()
    if not a.ack_lab_only: raise SystemExit('REFUSE: pass --ack-lab-only; 1.0.151 is not physically approved')
    meta=build(a.source,a.output)
    print('LAB-ONLY FULL LOCAL ENGINE 1.0.151 BUILT')
    for k,v in meta.items(): print(f'{k}={v}')
    print('NO DEVICE IO; NO OTA; NO CATALOG REGISTRATION')
if __name__=='__main__': main()
