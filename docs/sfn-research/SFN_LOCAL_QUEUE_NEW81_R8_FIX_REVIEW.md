# SFN Local Queue 1.0.81 — vendor R8 delimiter fix review

Date: 2026-10-16
Target canary: New / `88D039394D73` only
Golden build source: exact `0.1.54-1.0.71` neutral image

## Why 1.0.81 exists

Physical New on `0.1.54-1.0.80` proved the stage ACK barrier itself works for Begin, but the first Item was rejected:

- `sfnq:4` -> `up:sfnq:4:0:0:0`
- Begin -> `up:sfnq:1:0:0:0`
- first Item -> `up:sfnq:0:0:0:0`
- active queue remained generation 0/count 0
- all four outlets remained ON
- heap remained 34416 current / 30960 minimum

No relay event was committed or executed.

## Exact root cause

The stock controller initializes `R8` at `0x10015D5A` to the address `0x10015FEC`, whose bytes are `":\0"`.
The stock helper at `0x10016BEC` passes `R8` as the delimiter argument to the ROM string-search import.

The 1.0.80 Item parser incorrectly did:

`mov r8,r0`

after parsing the item slot. That replaced the vendor delimiter pointer with the slot number. The following numeric-field parse therefore could not locate the next `:` delimiter and the Item failed closed.

The earlier host emulator did not reveal this because it stubbed the stock delimiter helper with a Python search that ignored `R8`.

## Hardware failure reproduced before the fix

A corrected host harness was made to:

1. seed `R8=0x10015FEC`, exactly like the stock controller;
2. execute the real `0x10016BEC` helper path;
3. emulate only the ROM string-search import target `0x293CD`;
4. use the exact built 1.0.80 bytes.

It reproduced the physical sequence exactly:

- Status ACK 4
- Begin ACK 1
- Item ACK 0

This closes the previous emulator-assumption gap.

## 1.0.81 change

1.0.81 is rebuilt from the exact 1.0.71 golden image, not patched on top of 1.0.80.

The Item parser no longer uses `R8` as scratch storage. The validated slot is already equal to the staging write index, so the entry offset is reloaded from the SFN-owned staging counter at `[context+36]` when the item is stored.

Result:

- stock `R8` delimiter pointer is never repurposed;
- handler still saves/restores `R8`;
- no extra task is created;
- state remains one 256-byte allocation and one stock software timer;
- code remains inside the previously reviewed regions.

## Exact artifact

`comMTTL-W01_1.0.81-local-queue-r8fix-new-lab.fwr`

- size: 327944 bytes
- SHA256: `5b96489849e8808512a0e3f918013102ced8cb5610f939ae49da60cc2220ff0a`
- source SHA256: `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`
- resident events: 8
- SFN state: 256 bytes
- new worker task: false

Two independent builds are byte-identical.

## Exact-byte emulator gate

The 1.0.81 emulator executes the exact built candidate and now uses the vendor delimiter contract instead of the old simplified NEXT-token stub.

PASS:

- `vendor_delimiter_r8_preserved`
- `stage_barrier_ack_codes`
- real CRLF wire parsing
- timer queue machine code
- incomplete replacement preserves active generation
- stale timer after replacement does not relay early
- offline local execution
- duplicate commit idempotence
- malformed input fail-closed
- strict numeric parser
- single state/single timer
- no new worker task
- timer create failure
- timer command failure reporting
- relay failure reporting
- cold boot queue loss model

The harness also asserts that `R8` returns as `0x10015FEC` after every queue handler invocation.

## Model regression

The existing local-event-queue model and pre-canary contract model remain 57/57 PASS.

## Current gate

Host/research result: `READY_FOR_NEW_CANARY_WITH_ROLLBACK`

This does not by itself prove physical timing or relay execution. The next physical test must remain New-only with an exact rollback artifact already registered. The first post-flash queue event must be ON on an outlet freshly confirmed already ON; only after its terminal index advances with zero error may any real OFF/ON test be considered.
