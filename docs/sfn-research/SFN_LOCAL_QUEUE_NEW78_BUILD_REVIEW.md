# SFN Local Queue — New 1.0.78 Build Review

Date: 2026-10-16 UTC  
Scope: lab-only build and host QA. No device update.

## Decision

`CANDIDATE_BUILT_HOST_QA_PASS_WAITING_FOR_PHYSICAL_CANARY_APPROVAL`

The lab-only candidate was built from the exact proven New 1.0.71 artifact. No firmware was sent to New, no OTA/Flash job was created, no production catalog entry was added, and production service remains unchanged.

## Candidate

- Target: New / `88D039394D73` only
- Baseline: `0.1.54-1.0.71`
- Baseline SHA-256: `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`
- Candidate: `0.1.54-1.0.78`
- Artifact: `/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.78-local-queue-new-lab.fwr`
- Candidate SHA-256: `87287d6d597ddf8cc4caaab27ab9f78ae08bfa18ec68e9a90b98520c437742c3`
- Size: 327944 bytes
- Decoded changed bytes: 985, all inside builder-approved ranges plus checksum

## Architecture frozen for this canary

- Eight resident local events per rolling window; not an account/customer schedule limit.
- Server retains the full schedule and refreshes future windows.
- Uses the stock FreeRTOS timer service; no new worker task and no call to `0x10010068`.
- One 256-byte queue state allocation plus one stock timer object.
- Queue survives network/server loss after commit, but not total strip power loss.
- After cold power loss the server must reconcile the state that should exist *now* and then send only future events.
- Existing `memory` diagnostic is preserved.

## Build / code-space gates

- Factory-test reviewed region: 190/198 bytes used, 8 bytes free.
- Reclaimed main region: 720/744 bytes used, 24 bytes free.
- ACK/timer-name data fit before the owned state-pointer slot.
- State pointer is zero in the file and only published after successful state + timer creation.
- Neutral four-loopback host isolation remains intact.
- Build is deterministic: an independent second build matched byte-for-byte with the same SHA-256.

## QA

Exact candidate artifact execution under Unicorn passed:

- atomic incomplete replacement
- stale timer after replacement
- local execution without server/network interaction
- duplicate commit idempotency
- malformed input fail-closed
- strict decimal parsing
- single state / single timer
- no new worker task
- timer-create failure fail-closed
- timer command failure reporting
- relay failure reporting
- cold-boot queue loss behavior

The existing 57 Local Queue / PRE-CANARY model tests also remain PASS.

Static QA confirms exact source SHA, checksum, target version, `sfnq` keyword, preserved `memory` command/format, exact machine-code blobs, neutral loopbacks, zero initial pointer, and no xTaskCreate call from the candidate payload.

## Rollback

Rollback is pinned to the exact physically proven 1.0.71 artifact:

`/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.71-neutral-lab-v2.fwr`

SHA-256: `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`

The lab rollback manifest is MAC-locked to New and has `offer:false`, `flash_authorized:false`, `deploy_authorized:false`.

## Production untouched proof

At the end of host QA:

- `sfn-power.service`: active
- deployed server: 1.6.7, SHA-256 `88327854a16a003df29598f44599cb026cea853b75d25fb7a3a2859b80b05072`
- New is still recorded on `0.1.54-1.0.71`

No device command was sent during the build/QA stage.

## Next gate

The only next step that can prove the remaining claims is a **physical canary on New**, with a predeclared rollback trigger. Flash/OTA is deliberately not performed by this report. A physical canary requires explicit approval because it is the first step that changes the strip.