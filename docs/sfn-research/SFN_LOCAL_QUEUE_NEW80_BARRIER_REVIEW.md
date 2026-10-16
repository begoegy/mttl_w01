# SFN Local Queue 1.0.80 — stage barrier review

Date: 2026-10-16
Target canary: New / `88D039394D73` only
Current physical firmware: `0.1.54-1.0.79`
Build baseline: exact proven `0.1.54-1.0.71`

## Decision

`HOST_READY_FOR_NEW80_CANARY_REVIEW_ONLY`

This report does **not** authorize OTA/Flash, catalog publication, production deploy, or any relay-changing test.

## Physical 1.0.79 finding

The first harmless scheduled-ON attempt was intentionally guarded by a fresh state read. New was online, all four outlets were ON, queue status was generation 0 / count 0 / meta 0, and heap was healthy.

The server then attempted one transaction: Begin -> one Item (outlet 1, ON, +10s) -> Commit. The transaction returned `local_queue_no_ack`. Device logs recorded two `code=0` queue replies. The active queue remained generation 0 / count 0 / meta 0. A fresh state read confirmed all four outlets still ON and memory remained `current=34416`, `minimum=30960`.

No scheduled relay action was armed or executed. No OFF command was sent.

This live failure matches the already documented framing/serialization gap: the 1.0.79 firmware accepts complete individual CRLF frames but successful Begin/Item do not ACK, so the server cannot establish an application-level barrier before sending the next frame. The evidence does not prove the exact TCP packet boundary/coalescing that occurred, so this report does not claim packet coalescing as the sole root cause.

## Root fix — no timing sleeps

1.0.80 adds a positive ACK after every successful transaction stage:

- Begin success -> `code=1`
- Item success -> `code=2`
- Commit success -> `code=3`
- Status -> `code=4`
- Rejection remains `code=0`

The server-side 1.0.80 path sends exactly one stage, waits for its expected ACK code, and only then sends the next stage. There are no arbitrary sleeps or packet-timing assumptions.

`1.0.79` is explicitly status-only in the future server gate; queue writes are refused before any Begin frame is sent. `1.0.80` alone is write-capable.

## Final preferred host-only artifact

Artifact:

`/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.80-local-queue-barrier-new-lab.fwr`

SHA-256:

`385192607c2b6f92221413bf6692087e205bb73258e3ec033fb8aaa48c0a8c86`

Size: `327944` bytes.

The artifact is built from the exact proven 1.0.71 baseline SHA-256 `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`.

Code budget:

- factory/test ingress: 198 / 198 bytes
- main reviewed region: 736 / 744 bytes
- total: 934 / 942 bytes
- remaining reviewed executable capacity: 8 bytes
- queue state: 256 bytes
- no new worker task

## Host evidence

The exact final 1.0.80 artifact passed the machine-code Unicorn harness, including:

- stage barrier ACK codes
- real-wire CRLF parsing
- atomic incomplete replacement
- stale timer after replacement
- offline local timer execution
- duplicate commit idempotency
- malformed input fail-closed
- strict numeric parsing
- one state / one timer
- no new worker task
- timer-create failure fail-closed
- timer-command failure reporting
- relay failure reporting
- cold-boot RAM queue loss behavior

The existing 57 Local Queue / PRE-CANARY model tests remain PASS.

Independent rebuild produced the same bytes and the same SHA-256 (`reproducible_byte_identical=PASS`).

The future server bridge also passed its focused tests and the complete server suite: `504` tests PASS. It explicitly refuses queue writes on 1.0.79 and serializes Begin/Item/Commit by ACK on 1.0.80.

## Rejected alternative

A separate direct-stage-ACK variant filled all reviewed code capacity (`942 / 942`) and later hit an invalid-instruction failure in the malformed-input emulator path. Its artifact SHA begins `6af19c91...`. It is rejected and must never be catalogued, flashed, or deployed.

## Next gate

The next device-changing action would be a **new 1.0.80 canary OTA to New only** with the exact 1.0.71 rollback artifact pinned and ready. That is a separate approval gate.

After a successful 1.0.80 boot, validation order must be:

1. fresh outlet-state read; no relay action;
2. read-only queue status;
3. one same-state event after 10 seconds (ON->ON if a fresh outlet is ON, otherwise OFF->OFF);
4. require ACK sequence 1 -> 2 -> 3 and then status meta advancement;
5. stop on any mismatch before any OFF/ON cycle test.

**STOP: no 1.0.80 OTA/Flash was sent, no 1.0.80 catalog entry was created, and no production server containing the 1.0.80 write path was deployed by this review.**
