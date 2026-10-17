# SFN Native Provisioning + Core E2E Gate — 2026-10-17

Status: **LAB PASS / DO NOT FLASH / NOT PHYSICAL APPROVED**.

## Safety boundary
- No physical strip was written, rebooted, OTA-offered, or reconfigured by this gate.
- New2 `88D03907723D` remains reference-only.
- Build endpoint is TEST-NET-1 `192.0.2.10:10086`, deliberately non-production.
- Native build has `ota_write_enabled=false`.

## Provisioning design
- SFN Native builds now require an explicit pinned backend IPv4; omission fails the build.
- The build endpoint is compiled into `sfn_native_build_config.h`.
- On successful Wi-Fi commit, Wi-Fi credentials and the pinned SFN backend are persisted in the same atomic settings generation.
- Backend validation rejects zero-port, loopback, multicast/reserved first-octet ranges and broadcast; zero IPv4 is allowed only as the pre-provisioned state.

## Persistence / power-loss
- Existing two-slot commit-last settings journal was reused; no new flash partition was introduced.
- SFN backend torn-write test changes A=`192.0.2.10:10086` to B=`192.0.2.20:21197` while cutting programming at multiple byte offsets.
- Every interrupted save reloads endpoint A; endpoint B becomes visible only after a complete commit.
- Result: `SETTINGS_MOCK_FAILURES=0`.
- A stale maximum settings-size assertion was corrected from the pre-SFN value to the measured v7 maximum of 990 bytes.

## ARM build gate
Pinned Linux Ameba GCC/SDK build, sensor + relay + SFN Native, no OTA:
- backend: `192.0.2.10:10086`
- Build A SHA-256: `56daebd93b7fd3457673b32bc5e67ad48725e2a751a8ae02cc2ecac75269b0b1`
- Build B SHA-256: `56daebd93b7fd3457673b32bc5e67ad48725e2a751a8ae02cc2ecac75269b0b1`
- byte-identical reproducibility: PASS
- static gap to reserved MSP: 12,608 bytes
- settings persistent: true
- OTA write: false
- device_written: false
- production_ready: false

## Production-server compatibility gate
The host test imports the exact deployed `/opt/sfn-power/sfn-power.pyz` whose SHA-256 is:
`828997ffa4ddf77fcefa1ae975e02a9daeb14b57dda9a33f942fd9177236908a`.

A separate Linux process runs the same C `sfn_up_client` + `sfn_up_protocol` used by the firmware and connects over a real TCP socket to an isolated DeviceHub/PowerService instance from that zipapp. The lab process admits firmware `2.0.12` for Child Lock only in memory; production files/process/config are not changed.

PASS coverage:
- BootInfo / registration
- GetInfo refresh
- outlet ON confirmation
- outlet OFF confirmation
- voltage 224.5 V
- RSSI -55 dBm
- Child Lock set/read mask 31
- forced TCP disconnect
- backoff + reconnect + fresh BootInfo
- command after reconnect
- `up:ota` rejected/no ACK while connection remains usable

Host summary from final run:
`reconnects=2 commands=18 rejected=1 io_failures=1 child_lock=31 on_mask=2`.

## Not yet claimed
This gate does **not** prove target boot on real hardware. It also does not yet expose the full production Voltage Guard / Offline Schedule wire semantics. Those require their own capability adapter/gate before any physical canary discussion.
