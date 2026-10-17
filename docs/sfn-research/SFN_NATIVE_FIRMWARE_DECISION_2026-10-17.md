# SFN Native Firmware decision — 2026-10-17

## Decision
Stop using binary-reclamation / machine-code injection as the product path for new SFN Power firmware features.
Use a source-built MTTL-W01 firmware base for future scheduler, voltage guard, child lock and SFN integration work.

## Why
Two owner devices became unavailable during earlier experimental binary-patch canaries. Lab PASS is not equivalent to hardware-safe.
The source-built standalone base already contains persistent schedules, relay/meter control, OTA, settings persistence and real voltage measurement.

## Hard safety gates
1. No OTA or flash to New2 (`88D03907723D`) from this branch.
2. No customer-device OTA.
3. No physical canary until exact recovery is proven first.
4. Hardware approval requires boot/reconnect plus feature validation and power-cycle soak, not compilation alone.
5. Preserve a stock/native recovery artifact before any future physical canary.

## Phase 1
Implement pure, host-testable SFN modules:
- voltage guard state machine with hysteresis and ON veto;
- 5-bit child-lock policy that blocks only short physical presses and preserves long-press setup/recovery.

## Phase 1 result
Status: **LAB PASS / NOT BUILD VERIFIED / NOT PHYSICAL APPROVED**.

Implemented in source:
- `sfn_voltage_guard`: low/high trip, independent recovery window, consecutive-sample trip/recovery, fault latch, ON veto, no automatic ON.
- `sfn_child_lock`: 5-bit Master + Outlet1..4 mask; short physical events obey the mask; Master long-hold setup/recovery remains available.
- settings logical codec v6 while retaining v1/v4/v5 reads.
- sequence-bound `SFN1` flash sidecar at offset 1720; physical base remains v4-compatible and schedule remains its own `SC01` sidecar.
- worker runtime integration: new-voltage-sample-only evaluation, All-OFF on trip, ON command veto while faulted, scheduler ON veto while faulted, child-lock gating.

Host verification:
- `SFN_NATIVE_FEATURE_TESTS=PASS` including 200,000 deterministic mixed voltage samples.
- `SFN_SETTINGS_SIDECAR_TESTS=PASS` including v5 migration, v6 roundtrip, CRC corruption rejection and old-base preservation.

Not yet claimed:
- no Ameba firmware build was produced from this branch;
- no target boot was attempted;
- no device was flashed;
- no SFN/UP transport client is integrated yet.

## Phase 2 — SFN compatibility
Implement a clean-room device-side UP client from SFN Power's own documented wire contract. Do not copy third-party unlicensed firmware implementation code.
Required compatibility surface:
- TCP backend session on port 10086;
- `up:getinfo:all` response compatible with current SFN parser;
- `up:onoff:<1-4>:on|off` command + acknowledgement;
- `up:power_report:1:vol` voltage response;
- `up:query:wifirssi` response;
- `up:event:onoff:<0-4>:on|off` physical event push;
- `up:childlock` and `up:childlock:<0..31>`;
- boot identity/version handshake expected by current gateway;
- OTA only after the native OTA path is proven independently.

## Phase 3 — reproducible target build
Provision the pinned Ameba-1 SDK/compiler on `vmi3653994` only, build twice, require identical FWR SHA, run format/roundtrip/emulator/size gates, and archive build provenance.

## Phase 4 — physical gate
No physical candidate until a recovery path is proven first. New2 remains reference-only. A dedicated sacrificial device is the preferred first native canary.

## Phase 2A — UP wire codec result
Status: **HOST PASS / NO SOCKET TRANSPORT / NO OTA / NO DEVICE WRITE**.

Implemented clean-room, from SFN Power's own wire contract:
- strict server-command parser for GetInfo, outlet ON/OFF, voltage query, RSSI query, Child Lock read/set;
- BootInfo encoder;
- complete four-outlet + aggregate GetInfo encoder;
- ON/OFF ACK, physical event, voltage, RSSI and `up:cl:<mask>` encoders;
- deliberately no `up:ota` command handling yet.

Security/robustness:
- max bounded input line;
- non-printable command bytes rejected except permitted line/padding terminators;
- glued second command after CRLF is rejected instead of silently accepting the first command;
- invalid outlet and Child Lock ranges rejected.

Verification:
- `SFN_UP_PROTOCOL_TESTS=PASS` under host GCC with `-Wall -Wextra -Werror -pedantic`;
- C-generated BootInfo/GetInfo/Voltage/RSSI/ACK/Event frames parsed by the current production SFN Python protocol parser: `SFN_PRODUCTION_PARSER_CROSSCHECK=PASS`;
- Child Lock response parsed by current child-lock server branch: `SFN_CHILDLOCK_PARSER_CROSSCHECK=PASS`.

## Phase 2B — UP client engine
Status: **HOST PASS / SDK SOCKET ADAPTER NOT YET LINKED / OTA DISABLED**.

Added a bounded, allocation-free UP client engine:
- non-blocking connect state;
- reconnect backoff (1/2/4/8/16/30 seconds);
- BootInfo sent first on every new session;
- bounded 256-byte RX and 1024-byte TX buffers;
- partial send/receive handling;
- multiple CRLF-framed commands handled without unbounded queues;
- oversized unterminated input drops the session instead of overflowing/desynchronizing;
- network-down closes immediately;
- command responses are single-flight/bounded;
- unsolicited events can publish only when the client is fully ready;
- `up:ota` remains unsupported/rejected by design.

Verification: `SFN_UP_CLIENT_TESTS=PASS` with host GCC `-Wall -Wextra -Werror -pedantic`, including partial I/O, reconnect/backoff, malformed/unknown command behavior, overflow recovery and physical-event publish.
