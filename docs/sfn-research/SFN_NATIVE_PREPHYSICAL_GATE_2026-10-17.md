# SFN Native Firmware — Pre-Physical Gate — 2026-10-17

Status: **PRE-PHYSICAL LAB PASS / DO NOT FLASH / PHYSICAL BLOCKED BY RECOVERY GATE**.

## Safety boundary
- New2 `88D03907723D` was not written, rebooted, reconfigured, or sent an OTA command by this gate.
- New2 remains recorded as `0.1.32-1.0.38`.
- No customer device was touched.
- Production server artifact remains SHA-256 `828997ffa4ddf77fcefa1ae975e02a9daeb14b57dda9a33f942fd9177236908a`.
- Build backend is TEST-NET only: `192.0.2.10:10086`.
- Native runtime OTA write remains disabled.

## What this gate adds
- Time-based Voltage Guard instead of sample-count semantics.
- Minimum supported voltage hold: 10 seconds, matching the currently proven meter cadence; faster policies fail closed.
- Voltage protection mask 1..15: a fault turns off and blocks ON only for protected outlets.
- No automatic relay restore after voltage recovery.
- Versioned settings migration through v9 without changing the legacy v4 physical base record.
- SFN4 sidecar with backend, Voltage Guard, Child Lock, schedule generation and DST metadata.
- Durable Daily/Weekly offline schedules with maximum 8 device events.
- Once schedules are deliberately rejected until a correct durable UTC-date representation is implemented.
- Clock sync is RAM-only; schedule commit is the only persistent schedule write.
- Two upcoming timezone transitions are carried by the server and persisted with the committed table.

## Host gates
All passed:
- `SFN_NATIVE_FEATURE_TIME_GUARD_TESTS=PASS`
- `SFN_VOLTAGE_WIRE_POLICY_TESTS=PASS`
- `SFN_SCHEDULE_WIRE_TESTS=PASS`
- `SFN_SCHEDULE_SYNC_TRANSACTION_TESTS=PASS`
- `SFN_UP_PROTOCOL_TESTS=PASS`
- `SFN_UP_CLIENT_TESTS=PASS`
- `SFN_BACKEND_PROVISION_TESTS=PASS`
- `SFN_SETTINGS_SIDECAR_V9_DST_MIGRATION_TESTS=PASS`
- `SETTINGS_MOCK_FAILURES=0`
- `HOST_GATE_ALL=PASS`

The settings mock includes two-slot commit-last torn-write coverage. A cut save never exposes a half-written endpoint/config generation.

## Exact production-server compatibility gate
The test imports the exact deployed `/opt/sfn-power/sfn-power.pyz` and runs the same C UP client/protocol over a real TCP socket against an isolated in-process DeviceHub/PowerService instance.

Result: `SFN_PROD_1_6_9_NATIVE_E2E=PASS`.

Covered:
- BootInfo / registration
- GetInfo refresh
- outlet ON/OFF
- voltage / RSSI
- Child Lock
- valid Voltage Guard policy
- rejection of too-fast Voltage Guard policy
- rejection of auto-restore policy
- Clock Sync
- Daily/Weekly schedule commit/status
- rejection of Once schedule
- rejection of >8 schedule items
- forced TCP disconnect/reconnect
- committed schedule surviving reconnect
- post-reconnect relay command
- OTA command rejected/no ACK while the connection remains usable

Final host summary:
`HOST_STRIP_DONE reconnects=2 commands=50 rejected=1 io_failures=1 child_lock=31 on_mask=2 vguard_revision=17 vguard_mask=5 schedule_generation=42 schedule_count=2 state=2`

## ARM build / reproducibility
Pinned Ameba Linux toolchain, full sensor + relay + SFN Native profile, OTA write disabled.

Two clean builds produced byte-identical FWR files:
- SHA-256: `649245c6d41f342280adfbd0af0a4155ef4a35ada7117f8353dd1580771df930`
- bytes: `319684`
- static gap to reserved MSP: `8680` bytes
- link warning lines: `1037`, from the legacy Realtek SDK/toolchain baseline; no compile warning matched SFN/settings/scheduler/worker source paths in the final build log.
- `ota_write_enabled=false`
- `device_written=false`
- `production_ready=false`

Lab-only preserved artifact:
`/srv/sfn-power-research/lab-artifacts/sfn-native-prephysical-20261017/DO-NOT-FLASH-sfn-native-2.0.12-prephysical-lab.fwr`

## Physical blocker
This gate proves buildability, persistence behavior, server compatibility, deterministic packaging, and host/runtime logic. It does **not** prove target boot on an MTTL-W01.

No physical write is approved until a recovery route has itself been proven. New2 remains reference-only. A normal working strip must not be used as the first native-firmware boot experiment while the recovery route is unproven.
