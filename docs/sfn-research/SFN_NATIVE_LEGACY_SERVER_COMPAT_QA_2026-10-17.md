# SFN Native 2.0.13 — Existing Server Compatibility QA — 2026-10-17

> **2026-10-17 follow-on:** Native 2.0.14 supersedes the 2.0.13 lab artifact by adding read-only vendor Wi-Fi migration. The unchanged-server relay design below remains valid. See `SFN_NATIVE_VENDOR_WIFI_MIGRATION_QA_2026-10-17.md`.

Status: **LAB PASS / PRODUCTION SERVER UNCHANGED / DO NOT FLASH**

## Final architecture decision

SFN Power production server **must remain on its existing 1.6.9 relay-control contract** for this fix. The research-only server branch/commit `fix/native-control-confirm-20261017` / `40367fb` is **superseded for rollout and must not be deployed**.

Native firmware 2.0.13 is responsible for fitting the existing production contract:

1. Production sends the normal outlet command(s).
2. Production keeps its historical `0.3 s` settle delay.
3. Production sends one normal `up:getinfo:all` request.
4. If the native relay queue is still moving, 2.0.13 does **not** return a partial/stale report. It remembers that same refresh request for the current TCP link.
5. As soon as the relay queue is physically stable, 2.0.13 sends the same complete four-outlet report the current production server already understands.
6. Production performs its existing `confirmed + applied` check. No new server capability, polling mode, MAC exception, or device-name exception is required.
7. A reconnect clears any deferred report belonging to the old TCP link.

This keeps one server behavior for legacy and Native devices and fixes the difference inside the Native firmware where it belongs.

## Exact production-server proof

The E2E test imported the currently deployed artifact directly from `/opt/sfn-power/sfn-power.pyz`; no server source branch was used and no production file was modified.

- Production artifact SHA-256 before/after: `828997ffa4ddf77fcefa1ae975e02a9daeb14b57dda9a33f942fd9177236908a`
- Four simulated physical relay operations: `125 ms` each.
- Existing production server still requested its refresh after the historical `0.3 s` settle interval.
- Native firmware deferred that refresh until the batch was complete.
- Observed all-four ON completion: about `0.514 s`.
- Server result: `confirmed=true`, `applied=true`.
- All-four OFF: PASS.
- Single-outlet ON/OFF round-trip: PASS.
- Result marker: `SFN_UNMODIFIED_PROD_SERVER_E2E=PASS`.

## Other firmware gates

PASS:

- SFN UP client/reconnect tests.
- Final-state helper tests.
- Voltage Guard + child-lock feature tests.
- Schedule transaction/wire tests.
- Voltage wire-policy tests.
- Dedicated final-state E2E, including relay fault, Voltage Guard veto, and stale old-link confirmation rejection.
- WAN dead-TCP recovery logic from the same 2.0.13 source remains present.
- Boot Guard/rescue remains embedded; earlier 19 injected power-cut recovery test remains the recovery baseline.

## ARM build

Pinned Ameba toolchain, Native profile, relay + sensor enabled, Boot Guard enabled, runtime OTA writing disabled.

Final packaged FWR was built twice from the same source and was byte-identical:

- Version: `2.0.13`
- FWR bytes: `315584`
- FWR SHA-256: `6cfde9a985234fae7741ccc55c4f557f10b01d88688490aca442df43b00bf4f2`
- BIN SHA-256: `e63a450ff480247307f1dd8ac103371ff5b85559604f3896701db81ad04563d9`
- Static gap to reserved MSP: `12704` bytes
- Reserved MSP: `4096` bytes
- Initial RTOS heap: `39936` bytes
- `ota_write_enabled=false`
- No new compile warning from `sdk_worker.c` or `sdk_up_socket.c`.

Stack review remains within the 6144-byte network-task allocation. The new deferred-report helper frame is `1048` bytes; the existing worker frame is `1136` bytes and existing `publish_snapshots` frame remains larger at `2056` bytes.

## Safety boundary

- New2 `88D03907723D`: **not written, not rebooted, not upgraded, no OTA sent**.
- No other physical strip touched.
- Production server not restarted and not deployed.
- New FWR is lab-only and must not be registered in the active firmware catalog yet.
- Preserved lab artifact path: `/srv/sfn-power-research/lab-artifacts/sfn-native-legacy-server-compatible-20261017/DO-NOT-FLASH-sfn-native-2.0.13-legacy-server-compatible-lab.fwr`.
- Physical success is still unproven until an explicitly approved New2 canary test.

## Physical next step — not executed

There is **no server deployment step** for this fix.

Before any New2 flash, the owner must receive the exact change, risk, rollback/recovery plan, and explicitly approve the physical update. Physical QA must include single outlet, all outlets, rapid commands, WAN loss/recovery without power-cycling, Voltage Guard, child lock, schedules, and UI/server result consistency.
