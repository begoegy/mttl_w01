# SFN Local Queue 1.0.78 — New physical canary

Date: 2026-10-16
Device: `New` / `88D039394D73` / `lgutap`
Decision scope: one owned canary only; no fleet offer.

## Result

`CANARY_BOOT_AND_BASE_HEALTH_PASS`

The exact lab candidate `comMTTL-W01_1.0.78-local-queue-new-lab.fwr` was installed on New through the existing guarded firmware update manager.

- source firmware: `0.1.54-1.0.71`
- target firmware: `0.1.54-1.0.78`
- candidate SHA-256: `87287d6d597ddf8cc4caaab27ab9f78ae08bfa18ec68e9a90b98520c437742c3`
- bytes transferred: `327944 / 327944`
- firmware job: `d2fed03212071a05`
- job result: `succeeded / verified`
- automatic rollback package: exact proven 1.0.71 SHA `c8882481a19876e2e809e4d031138617f51c6f42b8fb706d66a37fe101a57081`
- explicit recovery manifest: `sfn-recovery-1.0.71-from-1.0.78-new`

The package remains `offer:false`, source-version locked to `0.1.54-1.0.71`, model locked to `lgutap`, and MAC locked to `88D039394D73`.

## Preflight

Before the OTA:

- server `sfn-power 1.6.7` active
- zero unfinished firmware jobs
- New online on exact `0.1.54-1.0.71`
- read-only memory probe: current free `34960`, minimum-ever free `30960`
- all four outlets ON
- voltage `205.9 V`
- RSSI `-53 dBm`
- rollback artifact SHA verified
- candidate artifact SHA verified
- production state/catalog/DB backup: `/srv/sfn-power/backups/pre-new78-canary-20261016T192522Z`

## OTA / reconnect proof

The guarded update manager recorded:

1. preflight passed;
2. OTA offer accepted;
3. download reached 25%, 50%, 75%, then 100%;
4. exact `327944` bytes transferred;
5. device rebooted;
6. New reconnected reporting `0.1.54-1.0.78`;
7. post-update verification passed.

Post-update snapshot:

- all four outlets still ON
- voltage `204.7 V`
- RSSI `-53 dBm`
- device online on `0.1.54-1.0.78`

A read-only memory probe after reconnect returned current free `34728` and minimum-ever free `30960`. A second probe returned the same values and proved the command path remained responsive after the update.

No rollback was triggered because base health verification passed. The exact 1.0.71 recovery path remains installed and MAC-restricted if later queue-specific hardware validation exposes a problem.

## What is NOT yet claimed

This physical canary proves the candidate boots, reconnects, preserves normal outlet state/telemetry, and remains responsive to the existing memory diagnostic.

It does **not** yet prove the new `sfnq` queue parser/timer/relay behavior on hardware. The production server currently has no operator action for the new queue protocol. Queue-specific hardware testing must be a separate next phase, preferably beginning with a read-only `sfnq:4` status query before any scheduled relay transition.

No other strip was updated or touched by this canary.

## Queue readiness probe and rollback — 2026-10-16

After the base-health canary, production gained a narrow operator-only parser for the candidate's read-only `sfnq:4` status command. No queue begin/item/commit command was exposed.

The first and only physical queue-status probe on New returned:

`up:sfnq:0:0:0:0`

The candidate emulator already reserves `code=0` for a fail-closed initialization/error path; a healthy empty queue status was expected to be `up:sfnq:4:0:0:0`. Therefore **Local Queue runtime readiness failed on real hardware**. No queue item was staged or committed and no relay was switched by this probe.

A following stock heap probe still reported `34416` current free bytes and `30960` minimum free bytes. This makes gross heap exhaustion unlikely, but does not distinguish state-allocation failure, fragmentation, timer-create ABI/contract failure, or another initialization precondition. Do not claim a root cause until static evidence closes it.

Decision: `1.0.78 LOCAL QUEUE CANARY REJECTED FOR FEATURE READINESS`.

The device itself remained healthy, but the feature gate failed. Per the predeclared recovery plan, New was restored to the exact proven `0.1.54-1.0.71` artifact. Recovery job `4b89548e825da0ea` transferred `327944/327944` and finished `succeeded / verified`; New reconnected on `1.0.71`.

No second `sfnq` probe, no queue transaction, no relay test, and no fleet action is authorized from this result. The next work is static/lab-only proof of the initialization/timer-create path before any new candidate is built.
