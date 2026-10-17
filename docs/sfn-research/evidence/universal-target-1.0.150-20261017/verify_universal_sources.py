#!/usr/bin/env python3
from pathlib import Path
import hashlib, struct
XOR_BASE=bytes.fromhex('ca5923e7d79a84e843b3afe794b1afa7')
LOAD=0x10006000
P={
 '1.0.66':Path('/var/lib/sfn-power/firmware/comMTTL-W01_1.0.66.fwr'),
 '1.0.105':Path('/srv/sfn-power-research/vendor-sources/MTTL-W01_V1.0.105.fwr'),
 '1.0.146':Path('/var/lib/sfn-power/firmware/comMTTL-W01_1.0.146-voltra-public.fwr'),
 '1.0.150-universal':Path('/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.150-universal-envelope-lab.fwr'),
}
def key(h): return bytes(((s^v)+1)&255 for s,v in zip(XOR_BASE,h))
def dec(raw):
 k=key(raw[:16]); return raw[:16]+bytes(v^k[i%16] for i,v in enumerate(raw[16:],16))
def verify(name,p):
 raw=p.read_bytes(); d=dec(raw); size,load=struct.unpack_from('<II',d); entry=struct.unpack_from('<I',d,16)[0]
 assert len(d)==size+20 and load==LOAD and entry==0x10006071
 assert struct.unpack_from('<I',d,len(d)-4)[0]==sum(d[:-4])&0xffffffff
 assert d.find(b'LGU+-TAP-HW002')==40 and b'MTTL-W01' in d
 return raw,d
D={}
for n,p in P.items():
 raw,d=verify(n,p); D[n]=d
 print(f'{n}: sha256={hashlib.sha256(raw).hexdigest()} size={len(raw)} header_tail={raw[8:16].hex()} childlock={b"childlock" in d}')
def rd(d,a,b): return d[a-LOAD+16:b-LOAD+16]
regions={
 'factory_reclaim':(0x1001606A,0x10016130),
 'queue_reclaim':(0x1001EB6C,0x1001EE54),
 'timer_service':(0x10012248,0x100123CA),
}
for name,(a,b) in regions.items():
 s=[rd(D[x],a,b) for x in ('1.0.66','1.0.105','1.0.146')]
 assert s[0]==s[1]==s[2],name
 print(f'{name}=IDENTICAL_66_105_146 sha256={hashlib.sha256(s[0]).hexdigest()}')
# 105 is exact-match to the already-proved 66 family in parser + relay regions.
for name,a,b in [('timer_parser',0x10015A00,0x10016140),('relay_dispatch',0x1001AB00,0x1001AC00)]:
 assert rd(D['1.0.66'],a,b)==rd(D['1.0.105'],a,b),name
 print(f'{name}_66_105=IDENTICAL')
# Universal target must preserve newer 146 child-lock protocol and relay dispatcher.
assert b'childlock' in D['1.0.146'] and b'up:cl:%d' in D['1.0.146']
assert b'childlock' in D['1.0.150-universal'] and b'up:cl:%d' in D['1.0.150-universal']
assert rd(D['1.0.146'],0x1001AB00,0x1001AC00)==rd(D['1.0.150-universal'],0x1001AB00,0x1001AC00)
for marker in (b'memory\0',b'up:memory:%d:%d\r\n\0',b'wifirssi',b'up:ota:ota_ok',b'up:ip:ip_ok',b'up:connect:connect_ok'):
 assert marker in D['1.0.150-universal'],marker
print('newer_childlock_preserved=PASS')
print('newer_relay_dispatch_preserved=PASS')
print('controller_ota_memory_rssi_markers_preserved=PASS')
print('UNIVERSAL_SOURCE_COMPATIBILITY_STATIC_GATE=PASS')
print('NO_DEVICE_IO NO_FLASH NO_OTA NO_DEPLOY')
