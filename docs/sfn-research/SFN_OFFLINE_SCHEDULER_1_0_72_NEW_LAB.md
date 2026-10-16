# SFN Offline Scheduler 1.0.72 — New canary

Date: 2026-10-16
Status: **REJECTED after physical canary**. Do not flash or register this image.
Canary: `New`, MAC `88D039394D73`, model `lgutap`.

## Proven source

The candidate is built only from the physically proven neutral-v2 `0.1.54-1.0.71` image:

- file: `comMTTL-W01_1.0.71-neutral-lab-v2.fwr`
- SHA-256: `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`
- size: 327944 bytes

The builder refuses any different source SHA, including a one-byte mutation.
Decoded 1.0.71 differs from pinned stock 1.0.66 only in the already-reviewed neutral host slots, version identity, and checksum; no unexpected executable differences were found.

## Port gate

The scheduler injection regions are byte-for-byte identical between the prior 1.0.148 research base and New's 1.0.71 base:

- task: `0x10009914..0x10009A12`
- memory hook: `0x1001600C..0x1001602E`
- reclaimed main region: `0x1001EB6C..0x1001EE54`
- ACK region: `0x1001EE5C..0x1001EEA8`

Required runtime helpers retain the same addresses. The stock 1.0.71 on/off parser itself calls relay dispatcher `0x1001AB42`, confirming its calling interface on this exact family.
Flash table sectors remain `0xFC000` / `0xFD000`; no exact literals for those sectors occur in the source image.

## Candidate

- target version: `0.1.54-1.0.72`
- file: `comMTTL-W01_1.0.72-offline-scheduler-new-lab.fwr`
- SHA-256: `69f67268926dcd7c23fa0d149537f8b9b8e42705a1107ea75be71b327912907a`
- size: 327944 bytes
- decoded changed bytes: 1061

Two independent builds were byte-for-byte identical.

## Emulator / persistence gates

Unicorn payload QA passed:

- clock sync ACK
- incomplete generation cannot commit
- Flash commit/readback/status
- cold-boot table recovery
- cold-boot clock requires resync
- offline due relay execution without a server on/off command
- corrupt newest A/B copy falls back to the prior valid copy
- 8-byte stack alignment is enforced at every stub call

Maximum-table Flash model passed all 344 interruption cut points. Maximum persisted table size is 344 bytes / 40 switch events.

No physical device was flashed while producing these results.

## Physical rejection record

The 1.0.72 image transferred completely to `New` (327944/327944) and booted as `0.1.54-1.0.72`, but the first offline-scheduler clock command was not acknowledged and the device command session stopped producing normal telemetry. The device was safely recovered with the exact proven 1.0.71 image (`c8882481...`) and returned to fresh telemetry.

Root cause was reproduced from the exact rejected artifact: the generated ACK routine still loaded format pointer `0x1001EE30`, which is executable code in this layout, instead of the intended `0x100166A4` string. The old emulator hid this because its `snprintf` stub synthesized the response without reading the firmware's actual format pointer. A second test gap was also closed: the inline parser hook now must preserve `r5/r6/r7`.
