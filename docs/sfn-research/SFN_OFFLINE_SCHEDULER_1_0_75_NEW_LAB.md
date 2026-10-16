# SFN Offline Scheduler 1.0.75 — New canary

Date: 2026-10-16
Status: REJECTED after physical New canary; recovered to proven 1.0.71.
Canary: `New`, MAC `88D039394D73`, baseline `0.1.54-1.0.71`.

## Root cause carried forward from rejected 1.0.74

1.0.74 stored the scheduler state pointer at stock RAM `0x10052DA4`. Exact 1.0.71 disassembly proved that address is also a live 64-byte OTA buffer, so the design violated RAM ownership and is rejected. New was remotely rolled back to proven 1.0.71.

## 1.0.75 design change

The scheduler keeps its 1152-byte state in the already-owned 4096-byte task stack, as in 1.0.74, but publishes the stack pointer only at `0x1001EE98`. This four-byte slot is inside the reclaimed MEF string tail between scheduler ACK code and the next surviving function. In the exact proven 1.0.71 source it is `00 00 00 00`; exhaustive halfword PC-relative reference scanning and raw-pointer scanning find zero stock references to the slot. The Image2 header loads the application at `0x10006000`, so the slot resides in the loaded application SRAM image.

No customer/fleet flash is authorized by this document.

## Candidate and QA result

- target: `0.1.54-1.0.75`
- artifact: `comMTTL-W01_1.0.75-offline-scheduler-new-lab.fwr`
- size: `327944` bytes
- SHA-256: `bd8c48dfb78e6b8bdd1c2c4b3b9d96d3884f64ef60287e52ebe3e67a5da5af38`
- decoded changes from 1.0.74: 11 bytes total, consisting only of the two encoded `DATA_PTR` address constructions, version digit `4 -> 5`, and additive checksum bytes.

Repeated QA passes:

- exact-source ownership gate: `0x1001EE98` is zero in proven 1.0.71 and has zero stock PC-relative/raw-pointer references
- two independent builds are byte-for-byte identical
- one-byte-mutated source is refused by SHA gate
- final artifact disassembly writes and reads `0x1001EE98` exactly
- stack-backed state, no scheduler-state malloc
- parser tail/register preservation
- clock ACK and atomic schedule transaction
- Flash commit/readback/status and cold-boot recovery
- offline due relay execution without a server on/off command
- corrupt-newest A/B fallback
- all 344 maximum-table interruption cut points pass
- daily/weekly/once event model and invalid-event rejection pass

The artifact remains LAB-only. No device was flashed while generating these results.

## Production canary preflight checkpoint

The candidate artifact was copied into the protected production firmware catalog with `offer:false`, exact SHA pinning, source `0.1.54-1.0.71`, model `lgutap`, and `allowed_macs=[88D039394D73]`. A dedicated `1.0.75 -> 1.0.71` recovery manifest points to the physically proven 1.0.71 bytes. Production `FirmwareCatalog` validation reports the target allowed for New and rollback available; all rollback manifests for reported 1.0.71 resolve to one identical SHA (`c8882481...`). No unfinished firmware job exists for New at this checkpoint.

The physical OTA has **not** been started at this checkpoint. It must be initiated only through the authenticated Platform Admin firmware-manager path so the audited job, byte progress, post-boot verification, and automatic rollback remain in force.

## Physical canary result — rejected

New received exact 1.0.75 artifact SHA `bd8c48df...` in firmware job `6965f872dd709772`. Transfer reached 327944/327944, device rebooted, reconnected as `0.1.54-1.0.75`, and generic firmware verification succeeded. The first production offline-schedule clock sync then failed with `clock_not_acknowledged`, so 1.0.75 is rejected for scheduler use. Recovery job `00b94ca717a7da8a` transferred the proven 1.0.71 artifact, rebooted, and finished `succeeded/verified`; New is back on `0.1.54-1.0.71`.

Post-canary static review found a second QA gap: the emulator always called `run_boot()` before any scheduler command, directly starting the patched task. Exact stock 1.0.71 creates entry `0x10009915` through `xTaskCreate` only on the conditional branch at `0x10009AFA`, after checking `0x1005781C` for magic `55 AA`; the alternate path does not create that task. Therefore a physical boot can reach the command parser with scheduler `DATA_PTR` still zero. This exactly reproduces the no-ACK behavior if the patched task was not started. Treat this as the leading root-cause hypothesis until a lazy-init regression closes it; do not flash another scheduler candidate that assumes the task already exists.
