# SFN Full Local Engine 1.0.151 R4 gate — 2026-10-17

Status: **pre-canary machine-code PASS; physical OTA not yet authorized/executed.**

Owner canary target, when the production server can be safely updated, is `New / 88D039394D73` only. `الانتريه` is excluded and no customer strip is eligible.

## Exact candidate

Universal FWR: `comMTTL-W01_1.0.151-full-local-engine-universal-r4-a.fwr`
SHA-256: `44e46fa430d99492e37ca0ad50fdec3d8eefb7a8d4ac8abf5f8344fc05f4c36b`
Size: `327944` bytes.
Vendor-wrapped build SHA-256: `e70ba96726dc2a0093685fe5f5fb9e0bc8c71017a6676c51bf2abb3301a76098`.
Both vendor and universal forms are reproducible byte-for-byte across independent A/B builds. The universal envelope changes only the Image2/FWR envelope/checksum; application bytes are identical.

Exact base is the physically proven owner-canary neutral `0.1.55-1.0.147`, SHA-256 `1a3b66c4a43e63aa54615646b50f2c8507c57c3ce604be3f72211616f1379881`.

## Requested physical features

1. Durable local schedules: up to 38 switch events, stored in A/B Flash sectors `0xFC000/0xFD000` together with the voltage policy. The table executes locally after server/router connectivity is lost while the strip remains powered.
2. Local voltage guard: persistent low/high thresholds, hysteresis, hold/recovery times, protected outlet mask and optional restore. A tripped protected outlet cannot be turned ON through the common relay dispatch path until voltage is safe; OFF remains allowed.
3. Vendor child lock: original `childlock` implementation is preserved byte-for-byte. Mask `0..31`, bit 0 master and bits 1..4 outlets; set bits remove physical-button callbacks and the vendor `CL` record persists the mask in Flash. App/server relay control remains separate.

The wall clock is RAM state. After a strip power reboot, the server must resynchronize the clock. If only the router/server becomes unavailable while the strip stays powered, the already-synchronized clock and local schedules continue.

## Fail-closed design

Old experimental Queue/Scheduler/Voltage-Guard payloads overlap in executable/data/Flash ownership and are forbidden from being stacked. 1.0.151 uses one task/state record and one relay-safety arbitration path. The old oneM2M task is disabled before its isolated body is reclaimed; externally called helpers after the reclaimed body are preserved.

## Rejected candidates

R1 was rejected before physical use because a short `CBZ` branch exceeded its real Thumb range and wrapped into the wrong runtime path. R2 was rejected because the voltage parser temporarily reused the vendor parser cursor register. R3 was rejected because it temporarily reused the vendor session-context register needed by the ACK path. None was installed on any strip. R4 fixes all three and is the only candidate eligible for the next gate.

## QA

- unified storage/arbitration model: PASS, including every interrupted-write cut point;
- full layout bounds: PASS with no segment overlap;
- independent FWR validator: PASS;
- exact final R4 bytes under Unicorn: PASS for boot/Flash recovery, voltage policy ACK/persistence, schedule persistence/due execution, sustained high-voltage trip and forced OFF, common relay ON veto, OFF allowance, stable safe recovery/restore, cold-boot data recovery, and byte-preserved vendor child-lock routines;
- no device I/O occurred in these tests.

## Remaining gate

Production currently requires a server artifact that explicitly understands `0.1.55-1.0.151` schedule/voltage/child-lock capability. That server source passed the full suite separately. It must not be deployed until the persistent gateway owns every physical strip and the safe restart gate is green. Only then may the owner canary OTA be started with exact `1.0.150` rollback.
