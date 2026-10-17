# SFN Full Local Engine 1.0.152 — Timer Architecture Gate

Date: 2026-10-17
Status: **LAB PASS / PHYSICAL CANARY PENDING**
Scope: MTTL-W01 HW002 family, owner `New` canary path only. This document does **not** authorize OTA.

## Why 1.0.151 is rejected

Physical canary `88D039394D73` proved that 1.0.151 could boot, but it entered a ~26–27 second reconnect loop. Clock sync was not acknowledged, diagnostics/polling timed out, and the automatic rollback offer could not be acknowledged. The exact 1.0.150 rollback subsequently succeeded and was verified.

The repair work found three concrete design/tooling problems that 1.0.152 addresses:

1. 1.0.151 reused the old `check_ota_ssl_task` permanent-task scheduler architecture. Earlier physical scheduler experiments had already rejected this architecture with the same boot-then-unresponsive behavior.
2. Keystone silently wrapped out-of-range Thumb `CBZ/CBNZ` branches instead of rejecting assembly. In the rejected build, a branch intended for `gs_existing` landed in the middle of initialization. 1.0.152 converts long conditions to normal `B<cond>` and adds an exact branch-target gate.
3. 1.0.152 is pinned to the exact physically-proven 1.0.150 family and uses that family's proven timer/tick address (`0x100103F8`), rather than inheriting an address from a different research baseline.

1.0.151 must remain fail-closed in server capability gates and must not be flashed again.

## 1.0.152 architecture

- Exact source FWR: physical 1.0.150 universal, SHA-256 `b75e394e77c7ec2e8598bb98d157980bd889cc06989cf8fc9702d6bfc20fba8f`.
- No custom permanent vendor task. The conditional old task creator is disabled.
- The local engine runs from a one-shot software timer and rearms every 1000 ms, using the timer pattern physically proven by Local Queue.
- State is lazy-allocated and fail-closed. Voltage sampling can restore local policy before the server sends the first clock command.
- RAM engine state is 776 bytes.
- Schedules and Voltage Guard share one A/B durable record in flash sectors `0xFC000` / `0xFD000`.
- The active image is not replaced until the newly written sector is read back and validated.
- Maximum durable schedule table: 38 physical events.
- Firmware accepts physical outlets 1–4. Product `all outlets` schedules are expanded server-side to 1,2,3,4 before synchronization.
- Vendor Child Lock code remains byte-for-byte preserved.
- Local Queue `sfnq` is deliberately not claimed by 1.0.152; durable schedules use `sfn:90..94`.

## Final R3 artifact

Artifacts were built independently twice and compared byte-for-byte:

- `comMTTL-W01_1.0.152-full-local-engine-timer-r3-a.fwr`
- `comMTTL-W01_1.0.152-full-local-engine-timer-r3-b.fwr`
- Size: 327944 bytes
- SHA-256: `fafa3f7e5a789bbe634e9e45c95ceb92962caad4574105c545e0a42aa1d7d872`
- Reproducible build: PASS

Earlier R1/R2 artifacts are rejected research outputs and are not candidates.

## Exact branch gate

`tools/sfn_full_local_engine_timer_branch_gate.py` validates every remaining compact `CBZ/CBNZ` in custom code against exact address→target pairs.

Result:

- `EXACT_CBZ_CBNZ_TARGETS=PASS count=26 max_delta=78`
- Architectural compact branch limit: 126 bytes.
- Long `get_state` existing/fail/unlock paths use regular conditional branches.

Evidence: `evidence/full-local-engine-1.0.152-r3/branch-gate-r3.txt`.

## Exact-byte Unicorn QA

`tools/sfn_full_local_engine_timer_exact_emulate.py` runs the final R3 machine bytes, not a Python behavior model. It starts with `DATA_PTR=0` and does not bootstrap the rejected vendor task.

PASS gates:

- cold first Clock without vendor task
- lazy 776-byte state allocation
- timer create + 1 second rearm
- durable schedule commit + timer execution
- shared A/B voltage/schedule record
- voltage trip force-OFF + protected ON veto
- safe-voltage recovery + restore
- cold-boot Voltage Guard recovery before first server Clock
- malloc failure/retry
- timer-create failure/retry
- timer-arm failure/retry
- vendor task creator disabled
- vendor Child Lock bytes preserved

Evidence: `evidence/full-local-engine-1.0.152-r3/exact-unicorn-r3.txt`.

## Storage/model QA

`tests/test_full_local_engine_model.py`: 9/9 PASS, including every interrupted-write cut point preserving either the previous complete record or the new complete record, corrupt-newest fallback, schedule/voltage coexistence, and relay arbitration.

Universal envelope tests: 2/2 PASS.
Local Queue 1.0.150 baseline builder tests: 2/2 PASS.

An unrelated Home Assistant MQTT test could not import `paho` in the reverse-engineering venv; this is an environment dependency outside the firmware path and was not treated as firmware evidence.

## Independent package validation

External validator from `af950833/mttl_w01_standalone` accepted R3 as:

- format: `LG U+ FWR (XOR)`
- size: 327944
- body size: 327924
- version candidates: `0.1.55`, `1.0.152`
- checksum valid
- exact SHA-256 matched

Evidence: `evidence/full-local-engine-1.0.152-r3/independent-validator-r3.txt`.

## Physical canary gate

1.0.152 is **not physically proven yet**. The next canary must be explicitly approved before OTA and must target owner `New` only.

Safer staged sequence:

1. Keep production server capability gates unaware of 1.0.152 during the first OTA. This prevents automatic offline-schedule Clock/Sync immediately on boot.
2. Prove stable boot/reconnect, outlet state preservation, voltage/RSSI telemetry, memory, Child Lock, and normal device command responsiveness.
3. Only after that proof, deploy the separately QA'd server capability update and test Clock/status/table sync in controlled steps.
4. Then test local Voltage Guard and network-loss schedule execution under explicit canary controls.
5. No customer rollout follows automatically from one canary.
