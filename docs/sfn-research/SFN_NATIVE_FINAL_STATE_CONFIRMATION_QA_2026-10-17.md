# SFN Native Final-State Confirmation QA — 2026-10-17

Status: **LAB PASS / DO NOT FLASH / NOT PRODUCTION DEPLOYED**

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

## Server compatibility

The server hotfix was narrowed to an explicit capability allowlist:

`NATIVE_FINAL_STATE_CONFIRMATION_FIRMWARES = {"2.0.13"}`

For legacy firmware, `set_outlets()` retains the historical behavior: send the commands, wait the existing 0.3 s settle interval, then perform one refresh. No global polling semantic is applied to 1.x devices.

For 2.0.13, the server:

1. sends the requested outlet commands;
2. waits for a matching final ACK for every requested channel;
3. only then requests a complete outlet report;
4. succeeds only when that fresh complete report matches the requested final state;
5. fails closed when final ACK/report confirmation does not arrive within the bounded transaction timeout.

No MAC, device name, or New2 special case is used.

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

Full suite against the gated server source:

- **557 tests**
- **0 failures**
- PASS

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

Two clean builds after final source review were byte-identical:

- Version: `2.0.13`
- FWR bytes: `315360`
- FWR SHA-256: `33fa9cabede0fd144eaf77b2b9b668db86181497b6c7862c80e61c097d3aeda8`
- ELF SHA-256: `8f68de1fcf180dc678f7182294257ebe00323dcaad9a6b5d762d07a8f4418a24`
- Static gap to reserved MSP: `12920` bytes
- Reserved MSP: `4096` bytes
- Initial RTOS heap: `39936` bytes
- `ota_write_enabled=false`
- `device_written=false`
- setup ARM emulation success/failure cases: `2/2`
- Image2 package round-trip: PASS

Stack compile estimates from `-fstack-usage`:

- relay task frame: `80` bytes; task allocation is 384 stack words (1536 bytes on Cortex-M3);
- network worker frame: `1136` bytes; task allocation is 1536 stack words (6144 bytes);
- largest individual compiled frame in the image: `publish_snapshots` at `2056` bytes.

No compile warning is attributed to `sdk_worker.c`, `sfn_final_state.c`, `sdk_up_socket.c`, or `sfn_up_client.c`; the linker warning baseline is from the legacy Realtek SDK libraries/toolchain enum ABI warnings.

## Recovery material

Boot Guard still embeds the exact 1.0.70 rescue material:

- Rescue FWR SHA-256: `0cbf05da978ef3985439e9a1d40e71a8bcd5dea7988a800217615873783472a2`

This is a recovery mechanism, **not a guarantee** if a new image fails before it can execute the guard path.

## Preserved lab artifact

`/srv/sfn-power-research/lab-artifacts/sfn-native-final-state-reconnect-20261017/DO-NOT-FLASH-sfn-native-2.0.13-final-state-reconnect-lab.fwr`

This path is lab-only and is not registered in the active production firmware catalog.

## Required physical rollout order (not executed)

1. Back up production server state and deploy the **gated** server change first; verify legacy devices stay on the unchanged path.
2. Do not flash any device until owner approval.
3. With explicit approval, use New2 only as the physical 2.0.13 canary and verify the exact FWR SHA above before write.
4. Physical checks must cover single outlet, all outlets, rapid commands, voltage guard, child lock, reconnect, **WAN loss then WAN recovery without power-cycling the strip**, schedules, and server/UI result consistency.
5. Stop and use the existing guarded recovery path if New2 fails any physical gate.

Lab success is not physical success.
