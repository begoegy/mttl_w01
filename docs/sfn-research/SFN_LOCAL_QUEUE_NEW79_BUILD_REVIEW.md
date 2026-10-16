# SFN Local Queue 1.0.79 — CRLF root-cause fix / build review

Date: 2026-10-16  
Owned future canary: `New` / `88D039394D73`  
Source baseline: exact proven `0.1.54-1.0.71`  
Source SHA-256: `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`

## Decision

`READY_FOR_CANARY_REVIEW_NOT_FLASHED`

No OTA/Flash/Deploy is authorized by this document. New is currently restored to 1.0.71.

## Root cause closed for the 1.0.78 status failure

The physical 1.0.78 status probe returned exactly:

`up:sfnq:0:0:0:0`

The same exact 1.0.78 artifact was re-executed under the machine-code harness using the **real server wire ending** `sfnq:4\r\n`. It reproduced the hardware result exactly:

`new78_real_wire_ack=up:sfnq:0:0:0:0`

The old host test had used an artificial NUL-only string `sfnq:4\0`, which returned the expected code 4. That difference hid the bug.

Static exact-source review confirms the device receive buffer is zeroed and then filled with the bytes received from TCP; the candidate parser sees the CR/LF bytes. The 1.0.78 numeric helper stopped only at NUL or `:`. On the final numeric field it therefore consumed the digit and then rejected `\r` as a non-digit, entering the fail-closed ACK path.

This also explains the physical heap observation. Before the first status probe current free heap was 34,728 bytes; after it was 34,416, a 312-byte delta. The allocator model charges 264 bytes for the 256-byte state plus 48 bytes for the 40-byte timer object = 312 bytes. The exact 1.0.78 harness with CRLF likewise allocates state and timer and then returns code 0. This corroborates that initialization succeeded and the failure happened in command parsing, not in state/timer allocation.

## 1.0.79 fix

Only the strict numeric helper was widened to treat the normal ASCII line endings `CR (13)` and `LF (10)` as valid token terminators, in addition to NUL and `:`. Junk text remains rejected.

The extra eight bytes use the eight bytes that were still free in the reviewed factory-test region:

- 1.0.78: 190 / 198 bytes
- 1.0.79: 198 / 198 bytes
- main code remains 720 bytes and unchanged in size
- no new task, stack, Flash store, RTC, or persistence mechanism

Exact built artifact:

`/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.79-local-queue-crlf-new-lab.fwr`

- size: 327,944 bytes
- SHA-256: `f7dcc62cdab631ac92d55db31a22dcb56f7b6475f75b5e33f11adba0941a97ca`
- built twice from the exact 1.0.71 source; outputs are byte-identical
- builder remains exact-source/SHA gated and refuses changes outside reviewed regions

## Host QA

The exact 1.0.79 artifact was executed with CRLF on every queue command. PASS:

- real-wire CRLF parsing
- initial status
- atomic replace / incomplete replacement
- offline local execution
- stale timer after replacement
- duplicate commit
- malformed and prefixed numeric rejection
- one state / one stock timer
- no new worker task
- timer-create failure fail-closed
- timer-command failure reporting
- relay failure reporting
- cold boot queue loss semantics

The independent Local Event Queue/pre-canary model suite was rerun: **57/57 PASS**.

## Stop condition

The next device action, if approved later, is a fresh MAC-locked 1.0.79 canary with the exact 1.0.71 rollback artifact ready. The first post-boot action must again be read-only `sfnq:4`; no relay-changing schedule is allowed until it returns healthy code 4 on real hardware.
