# SFN Offline Scheduler 1.0.76 — New canary lab

Date: 2026-10-16
Status: LAB candidate; no fleet rollout.
Canary: `New`, MAC `88D039394D73`, proven baseline `0.1.54-1.0.71`.

## Why 1.0.75 was rejected

The 1.0.75 physical canary booted and verified as `0.1.54-1.0.75`, but the first offline-scheduler clock command received no ACK. New was immediately recovered and verified back on the proven 1.0.71 image.

Static analysis then proved the original vendor creation path for `check_ota_ssl_task` is conditional. The earlier emulator always executed the patched task bootstrap before exercising the parser, so it hid the exact hardware failure: if the vendor path does not create the task, `DATA_PTR` remains zero and 1.0.75 exits the `sfn` handler without an ACK.

## 1.0.76 design

1.0.76 keeps the owned SRAM state pointer slot at `0x1001EE98` and the 1152-byte scheduler state inside the existing 4096-byte RTOS task stack.

The parser-side `get_data` now:

1. reads `0x1001EE98`;
2. if non-zero, returns it without creating anything;
3. if zero, invokes the untouched stock task-creation wrapper at `0x10009A12`;
4. yields for 1000 ms on this first-start path only;
5. reads the pointer again and proceeds only if initialization published it.

The scheduler task publishes the pointer only after its state has been zeroed and both A/B flash images have been read and validated, so the parser never receives a partially initialized state pointer.

The exact final artifact proves the stock creator literal at `0x10009B24` is `0x10009915`, the patched scheduler task entry, and the stock task name is still `check_ota_ssl_task`. The creator requests a 0x400-word stack with priority 2.

## Candidate

- target: `0.1.54-1.0.76`
- artifact: `comMTTL-W01_1.0.76-offline-scheduler-new-lab.fwr`
- size: `327944` bytes
- source SHA-256: `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`
- output SHA-256: `848e9f766ce86d3a440e5ea69b25365f8eebecfccc78128662ed6dac1d95ccf7`
- task region: 250 bytes at `0x10009914..0x10009A0E` (limit `0x10009A12`)
- main region: 744 bytes
- ACK region: 54 bytes

## Repeated QA gates

- exact-source SHA pin and one-byte-mutated source refusal
- owned-state gate: `0x1001EE98` is zero in exact 1.0.71 and has no surviving stock PC-relative/raw-pointer references
- two independent builds are byte-for-byte identical
- rejected 1.0.75 failure reproduced when no scheduler task exists: no clock ACK
- same initial condition on 1.0.76: stock task creator invoked once and clock ACK returned
- second clock command reuses the same task; no duplicate creation
- lazy path completes Begin → Item → Commit → Status with one task only
- normal boot/task path still passes without scheduler-state malloc
- parser-tail/register preservation passes
- atomic incomplete generation, flash commit/readback/status, cold-boot table recovery pass
- offline due relay execution passes without a server on/off command
- corrupt-newest A/B fallback passes
- all 344 maximum-table interrupted-write cut points pass
- daily/weekly/once event model and invalid-event rejection pass

No device was flashed while generating these 1.0.76 QA results.

## Physical canary result — REJECTED

Two 1.0.76 physical canaries booted and were firmware-verified but failed the first scheduler clock command on hardware:

- SHA `fb191157569d519f71c6f7aa0a9cc8da84db51135df19af90b1461bb51c61696` (100 ms lazy-start yield): booted as 1.0.76; repeated `clock_not_acknowledged` after reconnects/reboot.
- SHA `848e9f766ce86d3a440e5ea69b25365f8eebecfccc78128662ed6dac1d95ccf7` (1000 ms lazy-start yield): booted and verified as 1.0.76; failed with the same `clock_not_acknowledged` result.

Therefore the parser-side lazy-start hypothesis is rejected; increasing the yield does not solve the hardware failure. Do not reflash either 1.0.76 artifact.

New was recovered to the proven 1.0.71 image. The final recovery transferred all 327944 bytes and finished `succeeded / verified`; New reported `0.1.54-1.0.71` afterward. The 1.0.76 target manifest was moved out of the active catalog into `firmware/rejected` and production server bytes were restored to the pre-canary SHA `f5515f736144425b285f0e689c6b5238cfb0617ea6a20b4bd690cbb47f2674e1`.

Next research must not assume task liveness from emulator bootstrap. A future candidate needs hardware-relevant proof that the scheduler task is created during the real boot path (or an equivalent design that does not depend on that task), before another physical canary is authorized.
