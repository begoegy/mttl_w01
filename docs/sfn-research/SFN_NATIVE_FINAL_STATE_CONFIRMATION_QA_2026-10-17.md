# SFN Native Final-State Confirmation QA — 2026-10-17

> **2026-10-17 follow-on:** Native 2.0.14 supersedes the 2.0.13 lab artifact by adding read-only vendor Wi-Fi migration. The unchanged-server relay design below remains valid. See `SFN_NATIVE_VENDOR_WIFI_MIGRATION_QA_2026-10-17.md`.

Status: **LAB PASS / DO NOT FLASH / NOT PRODUCTION DEPLOYED**

> **SUPERSEDED ROLLOUT DECISION:** Do **not** deploy server commit `40367fb` for this fix. Production server 1.6.9 stays unchanged. Native 2.0.13 now defers the existing `getinfo:all` response until relay work is physically stable, so it fits the historical server contract. See `SFN_NATIVE_LEGACY_SERVER_COMPAT_QA_2026-10-17.md`.

## Scope and safety boundary

- Firmware worktree: `/srv/sfn-power-research/worktrees/sfn-native-firmware-20261017`
- Firmware branch: `fix/native-final-state-confirm-20261017`
- Server worktree: `/srv/sfn-power/worktrees/native-control-confirm-20261017`
- Server branch: `fix/native-control-confirm-20261017`
- New2 `88D03907723D` was **not** written, rebooted, upgraded, or sent an OTA command during this change.
- No other physical strip was touched.
- Production server was not deployed or restarted.
- Android UI was not changed.

## Root cause

Native 2.0.12 acknowledged `up:onoff` when the command was admitted to the relay queue. Relay execution is asynchronous and each physical pulse takes about 100 ms. `up:getinfo:all` could therefore report the old or partially completed outlet state while a relay batch was still active. The server could interpret that transient report as a failed operation even though the relay completed moments later.

## Firmware contract in 2.0.13

The first firmware admitted to the final-state protocol is **2.0.13**. 2.0.12 is deliberately not advertised as capable.

- SFN TCP `up:onoff:<channel>:<state>` queues work without emitting an immediate success ACK.
- Remote commands reserve a bounded final-confirmation slot and are stamped with the current link epoch.
- The relay task emits eligibility for `up:onoff` ACK only after the physical relay operation finishes and the internal known/on state matches the requested result.
- Relay fault produces no success ACK and cancels waiting remote confirmations.
- `getinfo:all` is suppressed while a relay operation/batch is active, so an in-flight state cannot masquerade as the final state.
- Confirmation publication is non-blocking and uses the existing SFN client TX buffer.
- A reconnect changes the link epoch; confirmations belonging to a prior TCP epoch are discarded instead of leaking into a new transaction.
- Voltage-guard admission failures produce no success confirmation.
- Physical buttons and the local web control path remain local and do not consume SFN remote-confirmation slots.
- Child lock continues to affect physical-button events only; it does not block authenticated remote SFN commands.
- The confirmation queue is bounded to four remote commands, matching a four-outlet batch.
- The native TCP socket now enables TCP keepalive (30 s idle, 10 s interval, 3 probes). This fixes the observed WAN-outage case where Wi-Fi can remain associated while the old TCP path is dead. A failed keepalive is reported through the existing socket error path, which closes the stale connection and reconnects with normal bounded backoff; no device reboot is required.

## Server compatibility — final decision

The earlier gated-server design is retained as research history only and is **not the rollout path**. Server branch `fix/native-control-confirm-20261017` / commit `40367fb` must not be deployed for this fix.

Production server 1.6.9 remains unchanged: it sends outlet commands, waits the historical `0.3 s`, then sends one `up:getinfo:all` and evaluates the complete report. Native 2.0.13 now remembers that normal refresh when relays are still moving and emits the same complete legacy-format report only after the relay queue is stable. A reconnect discards any deferred report from the old link.

Exact E2E against `/opt/sfn-power/sfn-power.pyz` (SHA-256 `828997ffa4ddf77fcefa1ae975e02a9daeb14b57dda9a33f942fd9177236908a`) passed with four 125 ms relay operations: the unchanged server returned `confirmed=true` and `applied=true` after about `0.514 s`. Production artifact SHA was unchanged before/after.

## Firmware host tests

PASS:

- single outlet delayed confirmation;
- four queued outlet confirmations;
- relay fault with no success ACK;
- stale ACK rejection across reconnect epoch;
- 10,000 deterministic confirmation cycles without queue overflow;
- voltage-guard ON veto;
- child-lock local-button semantics independent from remote confirmation;
- SFN protocol/client tests, including oversized input reconnect behavior and dead-TCP-path close/backoff/reconnect without device reboot;
- schedule transaction/wire tests;
- settings migration tests;
- Boot Guard power-cut test at 19 injected write-failure points.

The new final-state helper and simulator compile with host GCC `-Wall -Wextra -Werror -pedantic`. Existing legacy settings/OTA sources still emit their established misleading-indentation/SDK warnings under the broader host compile; they are not introduced by this change.

## Server tests

No production server code is changed by the final design. The earlier research-only gated server branch had 557/557 tests passing, but that branch is now superseded for rollout. The required compatibility proof is the unmodified-production-server E2E above.

## Latency-aware E2E

The host strip simulator no longer flips `on_mask` immediately. It models relay latency, a bounded four-command queue, delayed final ACKs, busy `getinfo`, relay fault, voltage-guard rejection, and reconnect epochs.

Production-protocol lab E2E PASS includes:

- boot/connect and refresh;
- single outlet ON/OFF;
- all four outlets ON/OFF;
- rapid serialized commands;
- child lock then subsequent remote relay command;
- voltage + RSSI;
- voltage-guard policy and fail-closed unsupported policies;
- clock/schedules and schedule persistence across reconnect;
- reconnect;
- post-reconnect relay command;
- OTA command rejected while the link remains usable.

Dedicated final-state contract E2E PASS:

- four 125 ms relays took about **0.519 s** and were not confirmed early;
- injected relay fault emitted no success ACK;
- injected voltage-guard fault blocked ON without a success ACK;
- connection loss during a 500 ms pending relay dropped the stale ACK after reconnect while retaining the physical final state.

## ARM build and reproducibility

Pinned Ameba toolchain, source-built Native profile, sensor + relay enabled, Boot Guard enabled, OTA runtime write disabled.

The final packaged FWR was built twice after the compatibility change and was byte-identical:

- Version: `2.0.13`
- FWR bytes: `315584`
- FWR SHA-256: `6cfde9a985234fae7741ccc55c4f557f10b01d88688490aca442df43b00bf4f2`
- BIN SHA-256: `e63a450ff480247307f1dd8ac103371ff5b85559604f3896701db81ad04563d9`
- Static gap to reserved MSP: `12704` bytes
- Reserved MSP: `4096` bytes
- Initial RTOS heap: `39936` bytes
- `ota_write_enabled=false`
- `device_written=false`
- setup ARM emulation success/failure cases: `2/2`
- Image2 package round-trip: PASS

Stack compile estimates remain within task allocations. The deferred-report helper frame is `1048` bytes; the network worker frame is `1136` bytes with a 6144-byte task allocation, and the existing `publish_snapshots` frame remains `2056` bytes. No compile warning is attributed to `sdk_worker.c` or `sdk_up_socket.c`.

## Recovery material

Boot Guard still embeds the exact 1.0.70 rescue material:

- Rescue FWR SHA-256: `0cbf05da978ef3985439e9a1d40e71a8bcd5dea7988a800217615873783472a2`

This is a recovery mechanism, **not a guarantee** if a new image fails before it can execute the guard path.

## Preserved lab artifact

The superseded older lab artifact must not be used for rollout. The current compatibility artifact is preserved separately after final commit and is not registered in the active production firmware catalog.

## Required physical rollout order (not executed)

1. **No server deployment is required for this fix.** Production 1.6.9 stays unchanged.
2. Do not flash any device until owner approval.
3. With explicit approval, use New2 only as the physical 2.0.13 canary and verify the exact final FWR SHA immediately before write.
4. Physical checks must cover single outlet, all outlets, rapid commands, Voltage Guard, child lock, reconnect, WAN loss then WAN recovery without power-cycling the strip, schedules, and server/UI result consistency.
5. Stop and use the existing guarded recovery path if New2 fails any physical gate.

Lab success is not physical success.
