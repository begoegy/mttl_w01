# SFN Local Event Queue — New 1.0.71 timer-driven pre-canary gate

Date: 2026-10-16  
Owned canary: `New` / `88D039394D73`  
Exact baseline: `0.1.54-1.0.71`  
Source artifact: `/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.71-neutral-lab-v2.fwr`  
Source SHA-256: `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`

## Decision

`READY_FOR_CANARY_BUILD_REVIEW`

This decision authorizes **review of a lab-only build plan only**. It does not authorize Build, Flash, OTA, Deploy, or Production enablement.

The prior `NOT_READY_FOR_CANARY_BUILD` result was correct for the **new-worker-task** design. This gate changes the architecture: the queue no longer creates a worker task. It uses the stock FreeRTOS software-timer service already present and already used by this exact New 1.0.71 firmware.

## Why the task blocker is removed

The exact 1.0.71 timer subsystem initializes its own `Tmr Svc` task at `0x10012248`. Its call to the 8-argument task creator at `0x10012272` explicitly initializes both extra stack arguments (`sp+8=0`, `sp+0x0c=0`) and requests a `0x200`-word timer-service stack. The Local Queue candidate does not call task-create at all.

The timer API is native stock code:

- timer create-like entry: `0x1001227E`
- timer generic command: `0x100122B8`
- timer object allocation: `0x28` = 40 bytes
- callback pointer stored at timer object `+0x24`
- timer ID stored at `+0x20`
- timer callback invoked by the existing timer-service task

Stock New 1.0.71 already proves `timer callback -> relay dispatcher`: the factory `pctool_timer` callback at `0x10015AE4` calls relay dispatcher `0x1001AB42` at `0x10015AFC`.

## Runtime memory

The read-only physical probe on New returned:

- current free heap: `34,960` bytes
- minimum free heap since boot: `30,960` bytes

The new queue is deliberately capped at **8 resident events** per rolling local window. This is not a customer schedule limit; the server retains the full schedule and refreshes the local window.

Candidate state layout is 256 bytes total and contains two 8-event banks. Each event is 8 bytes (`offset_seconds`, outlet, action, reserved). One stock timer object is 40 bytes before allocator overhead. There is **no new task stack**. Even if either allocation fails because of fragmentation, the command fails closed: no pointer is published and normal vendor relay/control paths continue.

## Timing model

The exact source initializes SysTick with a 1000 divisor, giving the existing 1 ms RTOS tick used by the timer service. The queue stores event offsets in seconds and converts them to ticks from the commit anchor. A 24-hour rolling horizon is therefore far inside the signed half-wrap window.

The device never needs wall-clock time, timezone, NTP, or DST. Those remain server responsibilities.

## Transaction model

Wire ingress reuses the unused stock factory `test` command path. The same-length keyword becomes `sfnq`; the existing `memory` diagnostic remains untouched.

Proposed operations:

- `sfnq:1:<generation>:<count>` — begin staging, count 0..8
- `sfnq:2:<slot>:<offset_seconds>:<outlet>:<0|1>` — one staged event
- `sfnq:3:<generation>` — atomic commit
- `sfnq:4` — status

The parser is strict decimal-only, allows at most nine digits per numeric field, rejects empty/prefixed/junk numeric tokens, validates outlet/action/count/horizon, requires ordered slots and non-decreasing offsets, and invalidates malformed staging without replacing the active bank.

The active bank remains live until commit. Commit swaps banks atomically under the existing RTOS critical-section wrappers. Lost commit ACKs are handled idempotently by generation. A stale timer expiry racing with a replacement reads the new due time; if it is early it performs no relay action and rearms from the new state.

Timer-command failure is fail-safe and recorded in status. Relay-dispatch rejection is also recorded. Neither is silently reported as a successful transition.

## Reconnect and power-loss behavior

Network/server loss does not destroy the committed RAM queue; the stock timer service continues locally.

A full strip power loss does destroy the RAM queue. On reconnect the server must first query queue status:

- expected generation still present -> this was only a network/session reconnect; do not restart or re-anchor the queue;
- generation reset/missing -> treat as a cold reset, evaluate the schedule at the **current wall-clock time**, correct the outlet state that should exist now, then send only future events.

Thus after a cold reset the schedule neither starts from the beginning nor resumes an old countdown. It is rebuilt from "now". Missed point events are not replayed blindly. Interval state is reconciled to the state that should be true now.

## Manual control semantics for this canary

The scheduler uses the same existing relay dispatcher as stock timer callbacks. A manual app/button action changes the outlet immediately. A later scheduled event still executes at its scheduled time, which is normal smart-scheduler behavior. This canary does not attempt to intercept every button/deferred path to revoke future schedule events.

Safety arbitration remains a separate final-firmware concern: future **local Voltage Guard** must veto unsafe ON transitions, and future **Child Lock** controls physical-button callbacks. New 1.0.71 does not have the SFN local-cycle feature, so Local Cycle conflict is not present in this canary baseline.

## Code-space proof — host only

No firmware image was built. The final Thumb machine-code prototype was assembled only in host memory.

Approved/reviewed regions on the exact New baseline:

- stock factory-test body `0x1001606A..0x10016130`: candidate uses 190 / 198 bytes, 8 bytes free
- reclaimed neutral MEF region `0x1001EB6C..0x1001EE54`: candidate uses 720 / 744 bytes, 24 bytes free
- reclaimed data area before owned pointer: ACK format + timer name use 31 bytes and fit before `0x1001EE98`
- owned RAM pointer slot `0x1001EE98`: remains a dedicated 4-byte state pointer

Total code: 910 bytes across 942 bytes of reviewed code capacity. The version marker remains outside the main code region.

The candidate does **not** use the risky conditional old task entry at `0x10009914` and does not replace the `memory` command.

## Host execution evidence

Evidence directory:

`docs/sfn-research/evidence/local-queue-timer-new71-20261016/`

Static exact-source gate PASS:

- exact 1.0.71 SHA and checksum
- memory command preserved
- factory test ingress present
- pointer slot zero in source
- timer service uses the full 8-argument task-create setup
- 40-byte timer object allocation
- stock timer callback to relay dispatcher
- 1000 Hz tick base

The assembled candidate machine code was then executed under Unicorn with reviewed stock APIs stubbed. PASS cases include:

- atomic incomplete replacement preserves active queue
- local execution with no server/network interaction
- stale old timer after replacement does not relay early
- duplicate commit idempotency
- malformed staging fails closed
- strict numeric parsing
- one state allocation / one timer object
- **no new worker task / no call to `0x10010068`**
- timer-create failure leaves state unpublished
- timer-command failure reported
- relay failure reported
- cold boot loses the queue as designed

The prior 57 host contract/model tests were re-run and remain PASS. They remain useful background evidence for transactional/reboot/horizon behavior, but their older manual-override policy is not treated as the normative policy for this timer-driven canary.

## Remaining proof that belongs to the physical canary

The following cannot be honestly promoted from host proof to hardware proof:

1. the 256-byte state allocation and 40-byte timer allocation succeed on real New under live heap fragmentation;
2. the existing 2 KB `Tmr Svc` stack has adequate real headroom for this modest callback path;
3. real parser framing/ACKs match host execution;
4. real timer timing remains acceptable over a network-loss interval;
5. relay behavior and normal telemetry remain intact after the patch;
6. cold power loss clears the queue and reconnect status reports reset as expected.

All failure paths above are designed to fail closed; the canary exists to prove operation, not to justify a fleet rollout.

## Canary build plan — not executed

Proposed lab-only target version: `0.1.54-1.0.78` (1.0.77 is kept separate from the Local Queue track).

A future builder must be exact-source gated to the proven 1.0.71 SHA and may change only:

- version bytes + checksum,
- `test\0` -> `sfnq\0`,
- reviewed factory-test body region,
- reviewed reclaimed main region,
- reviewed ACK/data region including the owned pointer slot initial zero.

It must preserve neutral host isolation, `up:memory`, setup/provisioning, normal on/off, telemetry, voltage/RSSI, Wi-Fi, and OTA/recovery markers.

Rollback must be the exact proven 1.0.71 artifact SHA `c8882481...`, and any catalog entry must be `offer:false`, model/MAC locked to New only.

**STOP HERE. No FWR was built, no catalog entry was created, no OTA/Flash was sent, and Production was not modified.**
