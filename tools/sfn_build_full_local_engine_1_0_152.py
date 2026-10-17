#!/usr/bin/env python3
"""Build LAB-ONLY SFN Full Local Engine 1.0.152 from exact physical 1.0.150 baseline.

No device I/O, no catalog registration, no deployment. The source is SHA-pinned to
New's physically proven universal 1.0.150 image. Every changed decoded byte must be
inside an explicitly reviewed/reclaimed window.
"""
from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path
from tools.sfn_repack_universal_fwr import decode_fwr, transform, checksum
from tools.sfn_full_local_engine_timer_segments import (
    build_segments, TASK, CREATOR, CREATOR_END, MEM, MEM_END, TEST, TEST_END,
    MAIN, MAIN_END, ACK, ACK_END, LEGACY, LEGACY_END, TEXT, TEXT_END,
    WRAPPER, WRAPPER_END, HOOK, RELAY_API, DATA_PTR,
)

EXPECTED_SIZE=327944
EXPECTED_SHA256='b75e394e77c7ec2e8598bb98d157980bd889cc06989cf8fc9702d6bfc20fba8f'
LOAD=0x10006000
VERSION_OFFSET=0x18E64; SOURCE_VERSION=b'1.0.150'; TARGET_VERSION=b'1.0.152'
MEMORY_COMMAND_OFFSET=0x106AC; SOURCE_MEMORY_COMMAND=b'memory\x00\x00'; TARGET_SFN_COMMAND=b'sfn\x00\x00\x00\x00\x00'
MEMORY_FORMAT_OFFSET=0x106B4; SOURCE_MEMORY_FORMAT=b'up:memory:%d:%d\r\n\x00'; TARGET_ACK_FORMAT=b'up:sfn:%d:%d:%d\r\n\x00'
TEST_COMMAND_OFFSET=0x106E4; SOURCE_TEST_COMMAND=b'sfnq\x00'; TARGET_VGUARD_COMMAND=b'vguard\x00\x00'
PRESERVED=(b'LGU+-TAP-HW002',b'MTTL-W01',b'0.1.55',b'childlock',b'up:cl:%d',
           b'up:ip:ip_ok',b'up:connect:connect_ok',b'up:ota:ota_ok',b'up:reboot_ok',b'wifirssi',b'up:query:%d',b'ota.bin')


def off(addr:int)->int: return addr-LOAD+16

def exact_source(path:Path)->bytearray:
    raw=path.read_bytes()
    if len(raw)!=EXPECTED_SIZE: raise SystemExit(f'REFUSE: source size {len(raw)}')
    digest=hashlib.sha256(raw).hexdigest()
    if digest!=EXPECTED_SHA256: raise SystemExit(f'REFUSE: source SHA {digest}')
    d=bytearray(decode_fwr(raw))
    if bytes(d[VERSION_OFFSET:VERSION_OFFSET+len(SOURCE_VERSION)])!=SOURCE_VERSION:
        raise SystemExit('REFUSE: source version mismatch')
    if bytes(d[MEMORY_COMMAND_OFFSET:MEMORY_COMMAND_OFFSET+len(SOURCE_MEMORY_COMMAND)])!=SOURCE_MEMORY_COMMAND:
        raise SystemExit('REFUSE: memory command source mismatch')
    if bytes(d[MEMORY_FORMAT_OFFSET:MEMORY_FORMAT_OFFSET+len(SOURCE_MEMORY_FORMAT)])!=SOURCE_MEMORY_FORMAT:
        raise SystemExit('REFUSE: memory format source mismatch')
    if bytes(d[TEST_COMMAND_OFFSET:TEST_COMMAND_OFFSET+len(SOURCE_TEST_COMMAND)])!=SOURCE_TEST_COMMAND:
        raise SystemExit('REFUSE: local-queue command source mismatch')
    for marker in PRESERVED:
        if marker not in d: raise SystemExit(f'REFUSE: source marker missing {marker!r}')
    return d


def fill_region(buf:bytearray,start:int,end:int,fill:bytes=b'\x00\xbf'):
    if (end-start)%len(fill): raise SystemExit('REFUSE: odd code region')
    o=off(start); buf[o:o+(end-start)]=fill*((end-start)//len(fill))


def put(buf:bytearray,addr:int,data:bytes):
    o=off(addr); buf[o:o+len(data)]=data


def build(source:Path,output:Path)->dict:
    original=exact_source(source); patched=bytearray(original); seg=build_segments()

    patched[VERSION_OFFSET:VERSION_OFFSET+len(TARGET_VERSION)]=TARGET_VERSION
    patched[MEMORY_COMMAND_OFFSET:MEMORY_COMMAND_OFFSET+8]=TARGET_SFN_COMMAND
    patched[MEMORY_FORMAT_OFFSET:MEMORY_FORMAT_OFFSET+len(TARGET_ACK_FORMAT)]=TARGET_ACK_FORMAT
    patched[TEST_COMMAND_OFFSET:TEST_COMMAND_OFFSET+8]=TARGET_VGUARD_COMMAND

    # Clear only code/data windows whose ownership was explicitly established, then overlay exact segments.
    for lo,hi in ((TASK,CREATOR),(CREATOR,CREATOR_END),(MEM,MEM_END),(TEST,TEST_END),(MAIN,MAIN_END),
                  (ACK,ACK_END),(LEGACY,LEGACY_END),(TEXT,TEXT_END),(WRAPPER,WRAPPER_END)):
        fill_region(patched,lo,hi)
    # ACK window is code plus DATA_PTR/data; zero data tail rather than leaving NOP halfwords.
    patched[off(ACK):off(ACK_END)]=b'\x00'*(ACK_END-ACK)

    for name in ('task','creator_patch','creator_helper','memory','main','ack','vguard','legacy','gate','timer_create','timer_get','hook','relay_patch'):
        addr,data=seg[name]; put(patched,addr,data)
    # DATA_PTR must be cold-zero before the lazy allocator publishes state.
    patched[off(DATA_PTR):off(DATA_PTR)+4]=b'\x00'*4

    # Strict changed-byte allowlist.
    allowed=set(range(VERSION_OFFSET,VERSION_OFFSET+len(TARGET_VERSION)))
    allowed.update(range(MEMORY_COMMAND_OFFSET,MEMORY_COMMAND_OFFSET+8))
    allowed.update(range(MEMORY_FORMAT_OFFSET,MEMORY_FORMAT_OFFSET+len(TARGET_ACK_FORMAT)))
    allowed.update(range(TEST_COMMAND_OFFSET,TEST_COMMAND_OFFSET+8))
    for lo,hi in ((TASK,CREATOR),(CREATOR,CREATOR_END),(MEM,MEM_END),(TEST,TEST_END),(MAIN,MAIN_END),
                  (ACK,ACK_END),(LEGACY,LEGACY_END),(TEXT,TEXT_END),(WRAPPER,WRAPPER_END)):
        allowed.update(range(off(lo),off(hi)))
    for name in ('hook','relay_patch'):
        addr,data=seg[name]; allowed.update(range(off(addr),off(addr)+len(data)))
    allowed.update(range(len(patched)-4,len(patched)))

    struct.pack_into('<I',patched,len(patched)-4,checksum(patched))
    changed=[i for i,(a,b) in enumerate(zip(original,patched)) if a!=b]
    bad=[i for i in changed if i not in allowed]
    if bad: raise SystemExit(f'REFUSE: unexpected decoded change at {bad[0]:#x}')
    if bytes(patched[VERSION_OFFSET:VERSION_OFFSET+7])!=TARGET_VERSION: raise SystemExit('REFUSE: target version')
    if bytes(patched[MEMORY_COMMAND_OFFSET:MEMORY_COMMAND_OFFSET+8])!=TARGET_SFN_COMMAND: raise SystemExit('REFUSE: sfn ingress')
    if bytes(patched[TEST_COMMAND_OFFSET:TEST_COMMAND_OFFSET+8])!=TARGET_VGUARD_COMMAND: raise SystemExit('REFUSE: vguard ingress')
    if patched[off(DATA_PTR):off(DATA_PTR)+4]!=b'\x00'*4: raise SystemExit('REFUSE: DATA_PTR not cold zero')
    for marker in PRESERVED:
        if marker not in patched: raise SystemExit(f'REFUSE: preserved marker lost {marker!r}')

    encoded=transform(bytes(patched))
    if len(encoded)!=EXPECTED_SIZE: raise SystemExit('REFUSE: encoded size')
    verify=bytearray(decode_fwr(encoded))
    # generic FWR decoder is independent of target version; verify byte identity.
    if verify!=patched: raise SystemExit('REFUSE: encode/decode mismatch')
    output.parent.mkdir(parents=True,exist_ok=True)
    tmp=output.with_suffix(output.suffix+'.tmp'); tmp.write_bytes(encoded); tmp.replace(output)
    return {
      'source_sha256':EXPECTED_SHA256,'output_sha256':hashlib.sha256(encoded).hexdigest(),'size':len(encoded),
      'decoded_changed_bytes':len(changed),'target_version':'1.0.152','timer_engine':True,
      'state_bytes':776,'schedule_max_events':38,'childlock_preserved':True,
      'unified_flash_sectors':'0xFC000/0xFD000','check_ota_ssl_task_disabled':True,
      'no_device_io':True,
    }


def main():
    ap=argparse.ArgumentParser(); ap.add_argument('source',type=Path); ap.add_argument('output',type=Path); ap.add_argument('--ack-lab-only',action='store_true'); a=ap.parse_args()
    if not a.ack_lab_only: raise SystemExit('REFUSE: pass --ack-lab-only')
    meta=build(a.source,a.output)
    print('LAB-ONLY FULL LOCAL ENGINE 1.0.152 TIMER BUILT')
    for k,v in meta.items(): print(f'{k}={v}')
    print('NO DEVICE IO; NO OTA; NO CATALOG; NO DEPLOY')
if __name__=='__main__': main()
