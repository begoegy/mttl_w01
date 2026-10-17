# SFN Universal 1.0.150 — New physical canary — 2026-10-17

Status: BASE CANARY PASS; Local Queue physical command proof pending production server capability-gate deployment.

## Scope

Owner canary only: `New` / `88D039394D73`. No customer strip, `الصاله`, or `الانتريه` was targeted.

Source firmware: `0.1.54-1.0.81`.
Target firmware: `0.1.55-1.0.150`.
Target file: `comMTTL-W01_1.0.150-universal-envelope-lab.fwr`.
Target SHA-256: `b75e394e77c7ec2e8598bb98d157980bd889cc06989cf8fc9702d6bfc20fba8f`.
Rollback: exact `0.1.54-1.0.81`, SHA-256 `5b96489849e8808512a0e3f918013102ced8cb5610f939ae49da60cc2220ff0a`.
Firmware-manager job: `bd1028030e833504`.

## Preflight

Before OTA the owner canary was online and stable:

- all four outlets ON;
- voltage `217.8 V` immediately before job creation (`219.9 V` on the explicit preflight refresh);
- RSSI `-52 dBm` (`-51 dBm` on the explicit preflight refresh);
- memory probe `current=34648`, `minimum=30312` bytes;
- Local Queue status `generation=1`, `count=0`, `meta=0`;
- no unfinished firmware job;
- exact rollback artifact present and SHA-pinned.

The target and rollback manifests were MAC-restricted to `88D039394D73`; target `offer=false` prevented ordinary/customer rollout.

## OTA result

The official root/operator-only `firmware_start` path was used. It calls the production `FirmwareUpdateManager.create_job()` path; no firmware job was hand-written in SQLite.

The complete `327944 / 327944` bytes transferred. The strip rebooted and reconnected as `0.1.55-1.0.150`. The firmware manager completed `succeeded / verified`; automatic rollback was not required and remains available.

Post-update verification:

- all four outlets remained ON;
- voltage `218.0 V`;
- RSSI `-52 dBm`;
- memory `current=34728`, `minimum=30960` bytes;
- device remained responsive to read-only state and memory probes.

## Gateway transition proof

Before OTA, New was still on the legacy direct device leg. Its reboot caused a natural reconnect through the already-active persistent gateway. Gateway log:

`device 88D039394D73 accepted on 11086 from 41.33.44.154 -> backend 127.0.0.1:10086`

This proves New now uses the gateway ingress without a manually forced disconnect.

## Local Queue server gate found after canary

A post-update `local_queue_status` returned `local_queue_unsupported`. This was traced to the production server's explicit firmware allowlist, not a device response: production recognized Local Queue only on `0.1.54-1.0.79`, `.80`, `.81`.

Server fix branch: `fix/local-queue-1.0.150-capability-20261017`.
Server fix commit: `7493669`.
The fix adds only `0.1.55-1.0.150` to the Local Queue status/sync allowlists. It does not claim Local Cycle, Offline Schedule, Voltage Guard, or any other unproven capability.

Focused Local Queue tests: 8/8 PASS.
Complete server suite: 536/536 PASS.
Reproducible server artifact SHA-256: `eea4148d745d5c98291d3e76062ea6610b59ce6e385e2b735af182a03d8f2403`.
Artifact selftest: PASS.

At the post-canary checkpoint there were still `22` old direct physical sessions and `7` gateway-owned physical sessions. Therefore the production server was deliberately NOT restarted. The persistent-gateway restart guard remains authoritative. The exact tested server artifact must be deployed only after old direct physical sessions reach zero.

## Remaining physical gate

After the server capability fix is deployed safely, perform read-only `local_queue_status` first. Only if it reports healthy code/state should a no-op/controlled Local Queue synchronization be considered. Do not perform a fleet OTA from this canary result.
