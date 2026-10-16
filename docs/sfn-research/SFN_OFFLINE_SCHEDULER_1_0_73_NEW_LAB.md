# SFN Offline Scheduler 1.0.73 — New canary

Date: 2026-10-16
Status: **REJECTED after physical canary**. Do not flash or register this image.
Canary: `New`, MAC `88D039394D73`, model `lgutap`.

## Proven source

Built only from the physically proven neutral-v2 `0.1.54-1.0.71` image, SHA-256 `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`, size 327944. The builder refuses any other SHA, including a one-byte mutation.

## 1.0.72 rejection and fixes

The 1.0.72 physical canary transferred fully and booted, but its first scheduler command was not acknowledged. Exact-artifact disassembly proved the ACK routine still loaded `0x1001EE30` as its format pointer even though that address contains scheduler machine code. The correct string is at `0x100166A4`. The prior emulator did not catch this because it synthesized `snprintf` output without reading the passed format pointer.

1.0.73 closes both discovered gaps:

- ACK pointer rewrite is fail-closed and the final artifact disassembles to `movw r2,#0x66a4` / `movt r2,#0x1001`.
- The strengthened emulator reads the real format string from emulated memory and rejects the exact 1.0.72 image.
- The inline scheduler handler preserves parser registers with `push {r3,r5,r6,r7}` and restores them before returning to the stock parser tail.
- The strengthened emulator asserts `r5/r6/r7` preservation on every scheduler command.

## Candidate

- target: `0.1.54-1.0.73`
- file: `comMTTL-W01_1.0.73-offline-scheduler-new-lab.fwr`
- SHA-256: `8b21bc4909526a7d9a1f4c2ba65af58d54d3b66ffbd46f3f2280d339908d8963`
- size: 327944
- decoded changed bytes: 1063

Two independent builds were byte-for-byte identical. Final decoded artifact contains `up:sfn:%d:%d:%d\r\n` at `0x100166A4`; ACK loads that exact pointer; handler prologue/exit preserve parser registers.

## QA

Strengthened Unicorn QA passes clock ACK, atomic incomplete generation handling, Flash commit/readback/status, cold-boot table recovery, clock resync requirement, offline due relay execution, corrupt-newest A/B fallback, real format-pointer validation, parser-register preservation, and 8-byte stack alignment at every stub call. Maximum-table Flash model passes all 344 interruption cut points.

No physical device has been flashed with 1.0.73 at this checkpoint.

## Physical rejection record

The 1.0.73 image transferred 327944/327944 bytes and booted as `0.1.54-1.0.73`. The corrected ACK format pointer and parser-register preservation were present in the exact flashed artifact, but the first clock synchronization still received no ACK and the session later became unresponsive to normal polling. The strip was recovered to the exact proven 1.0.71 image and the recovery job verified successfully.

A strengthened emulator subsequently reproduced the exact no-clock-ACK signature when the scheduler task's extra `malloc(1152)` fails. The old emulator always forced that allocation to succeed. This does not by itself prove the physical heap was the only cause, so the next design removes this dependency entirely and also exercises the stock parser tail after every scheduler command.
