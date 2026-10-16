# SFN Offline Scheduler 1.0.74 — New canary

Date: 2026-10-16
Status: LAB candidate; physical canary pending.
Canary: `New`, MAC `88D039394D73`, model `lgutap`.

## Proven source

Built only from proven `0.1.54-1.0.71`, SHA-256 `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`, size 327944. Any other source SHA is refused.

## Why 1.0.74 exists

Physical 1.0.73 proved that correcting the ACK format pointer and preserving inline-parser registers was necessary but not sufficient. The scheduler task still allocated an additional 1152-byte heap block. The emulator had always forced this allocation to succeed. When the emulator was changed to fail that allocation, 1.0.73 reproduced the physical symptom: boot path remained alive but the clock command produced no ACK.

The stock task initializer was then verified from the exact 1.0.71 source: it requests `0x400` stack words, and the RTOS task-create implementation multiplies the depth by four before allocating the stack, proving a 4096-byte task stack.

1.0.74 stores the persistent scheduler runtime state inside that already-owned task stack and performs no scheduler-state heap allocation. Its task frame reserves 1176 bytes including saved registers/alignment/state, leaving 2920 bytes before nested calls.

## Candidate

- target: `0.1.54-1.0.74`
- file: `comMTTL-W01_1.0.74-offline-scheduler-new-lab.fwr`
- SHA-256: `d19db4b95236eee7726c41a3512aaea5b69cdd5c25e3c76044dda5224fb95319`
- size: 327944
- decoded changed bytes: 1051

Two independent builds are byte-for-byte identical.

## Strengthened QA

- scheduler state is stack-backed; emulator runs with malloc forced unavailable and records `malloc_calls=0`
- exact ACK format string and pointer validated from final artifact
- inline handler preserves parser registers
- handler continues through the stock parser tail and reaches the receive loop (`parser_tail_roundtrip=PASS`)
- clock ACK, atomic transaction, Flash commit/readback/status, cold-boot table recovery, clock resync semantics, offline relay execution and corrupt-newest A/B fallback all pass
- maximum-table Flash model passes all 344 interruption cut points
- exact stock task creation proves 4096-byte task stack and 2920-byte headroom after scheduler reservation

No physical device has received 1.0.74 at this checkpoint.
