#!/usr/bin/env python3
# Static ownership map for previously-proven experimental payloads. No device I/O.
features = {
    'local_queue_1.0.150': [
        (0x1001606A,0x10016130,'factory handler'),
        (0x1001EB6C,0x1001EE54,'main payload'),
        (0x1001EE5C,0x1001EE9C,'queue data/ack'),
        (0x100166D4,0x100166D9,'test->sfnq command'),
    ],
    'offline_scheduler_1.0.149': [
        (0x10009914,0x10009A12,'scheduler task'),
        (0x1001600C,0x1001602E,'memory handler hook'),
        (0x1001EB6C,0x1001EE54,'scheduler runtime'),
        (0x1001EE5C,0x1001EEA8,'scheduler ack/state'),
        (0x100166AC,0x100166B4,'memory->sfn command'),
    ],
    'voltage_guard_1.0.77': [
        (0x10009914,0x10009A12,'guard task'),
        (0x10012918,0x1001291C,'measurement hook'),
        (0x1001600C,0x1001602E,'guard memory hook'),
        (0x1001606A,0x10016130,'guard command handler'),
        (0x1001EB6C,0x1001EE54,'guard runtime'),
        (0x1001EE5C,0x1001EEA8,'guard ack/state'),
        (0x100166E4,0x100166EC,'test->vguard command'),
    ],
}
def overlap(a,b): return max(a[0],b[0]) < min(a[1],b[1])
conf=[]
keys=list(features)
for i,a in enumerate(keys):
    for b in keys[i+1:]:
        for ra in features[a]:
            for rb in features[b]:
                if overlap(ra,rb):
                    conf.append((a,b,max(ra[0],rb[0]),min(ra[1],rb[1]),ra[2],rb[2]))
assert conf, 'expected collisions disappeared: re-audit source maps'
print('LEGACY_STACKING=REFUSE')
for a,b,s,e,la,lb in conf:
    print(f'{a} <-> {b}: {s:#010x}..{e:#010x} ({la} / {lb})')
print('DESIGN_REQUIREMENT=single_shared_local_engine')
print('PRESERVE_VENDOR_CHILDLOCK=required')
print('NO_DEVICE_IO NO_FLASH NO_OTA')
