# SFN Full Local Engine 1.0.151 — design gate

Goal: one MTTL-W01 firmware target that can be physically tested on owner canary `New` for the three requested local/offline product features without stacking mutually-overwriting research payloads.

Required behavior:

- persistent wall-clock schedules stored locally and executable with the router/server unavailable;
- local voltage protection enforced in firmware, with unsafe ON transitions vetoed while a voltage trip is latched;
- vendor child-lock behavior preserved and exposed safely so physical outlet buttons can be disabled/enabled without removing app/server control;
- Local Queue remains available for short-horizon commands, but it must share the same arbitration/state engine rather than own a second overlapping runtime;
- normal relay control, OTA/recovery, Wi-Fi/setup, power/energy/voltage telemetry, RSSI and memory diagnostics remain intact;
- exact rollback remains the currently proven `0.1.55-1.0.150` owner-canary artifact until a new canary proves 1.0.151.

## Why a direct merge is forbidden

The prior Local Queue, Offline Scheduler and Voltage Guard research payloads all consume the same reclaimed executable/data windows. `collision_gate.py` fails closed and prints every overlap. A 1.0.151 candidate must therefore use one shared parser/runtime/task/state model; concatenating the old builders is explicitly rejected.

## Protocol direction

Use one SFN-owned command family and one local engine. The engine owns persistence, time, queue state and safety arbitration. Child lock remains the vendor implementation unless binary proof shows it cannot coexist; do not replace it merely for naming consistency.

Safety priority for relay ON:

1. voltage safety latch may veto ON;
2. manual/app OFF always remains allowed;
3. child lock affects physical-button callbacks only, not authenticated app/server control;
4. scheduled/queued/cycle ON is allowed only when safety permits it.

## Physical canary gate

No 1.0.151 OTA until host/emulator QA proves: boot recovery, A/B flash interruption safety, clock validity, schedule persistence, voltage trip/restore, child-lock command semantics, queue replacement, relay arbitration and exact rollback. Then only owner canary `New` may be targeted; customer fleet rollout remains forbidden.
