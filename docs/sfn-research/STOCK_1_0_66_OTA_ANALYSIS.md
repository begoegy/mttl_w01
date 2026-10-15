# SFN research: stock MTTL-W01 1.0.66 OTA

Date: 2026-10-15

This note records read-only/static analysis and live protocol observations made before any test-unit firmware write.

## Stock image identity

File: `work/firmware/1.0.66/comMTTL-W01_1.0.66.fwr`

- Size: `327944` bytes
- SHA-256: `d780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04`
- Decoded internal additive checksum: `0x01f8a3cd` (stored value equals calculated value)
- Decoded image contains `LGU+-TAP-HW002`, `MTTL-W01`, `0.1.54`, and `1.0.66`
- Decoded image contains the exact RSSI protocol strings `wifirssi` and `up:query:%d`
- Decoded image contains the local OTA acknowledgement `up:ota:ota_ok`

The repository describes this file as the unmodified stock/official 1.0.66 image. That description is useful provenance, but it is not a cryptographic manufacturer signature; SFN therefore pins the exact known file by SHA-256 before any test.

## RSSI evidence

A real older test unit reporting `0.1.32-1.0.38` was observed receiving `up:query:wifirssi` on its normal SFN Power TCP session. Its subsequent 258-byte application payload was zero-filled and contained no numeric RSSI value.

During the same production environment, multiple real units reporting exactly `0.1.54-1.0.66` returned numeric responses such as `up:query:-58`, `up:query:-40`, and `up:query:-60` to the same command.

This makes stock 1.0.66 a strong, directly evidenced candidate for restoring RSSI support on the older test unit. It is still not proof of hardware-revision compatibility for every MTTL-W01 ever produced.

## Local OTA protocol

The MatterBridge OTA tool source uses the strip setup network and connects to:

- Device: `192.168.1.1`
- TCP port: `30300`
- Command: `up:ota:<PC_LOCAL_IP>\r\n`
- Required device acknowledgement: `up:ota:ota_ok`
- Firmware download: HTTP `GET /ota.bin` from the PC on TCP port 80

The tool serves the selected firmware in 512-byte chunks with short pacing. Its original implementation accepts any non-empty selected file and does not enforce the MTTL-W01 model, version, size, or SHA-256. SFN must add validation rather than blindly automate this executable.

## Static reverse-engineering result

The decoded 1.0.66 image contains a setup command parser with branches for `ip`, `connect`, `ota`, and `reboot`. The `ota:` branch is inside the parser whose only identified call site is the setup TCP server. That caller opens TCP port `30300` (`0x765c`).

Therefore the existing evidence supports **local setup-mode OTA on port 30300**. It does **not** establish that `up:ota` is accepted on the strip's normal outbound SFN Power session on port 10086/21197. SFN should not assume remote raw-command OTA works.

A second OTA mechanism exists in the stock ecosystem through the manufacturer-style MEF update-check/download flow. The local-server project intercepts that flow with router destination NAT. SFN Power's current direct-TCP deployment does not provide evidence that this MEF path can be redirected remotely without access to the customer's router/network.

## Safety decision before first write

No public stock `1.0.38` FWR was found in the inspected repositories/history/searches, so an exact old-version rollback file is not currently available.

Before the first physical-device test:

1. Use only the exact pinned 1.0.66 image above.
2. Test on the already-authorized old unit only, not fleet-wide.
3. Perform the first test locally while physical setup/recovery access is available.
4. Keep power stable during transfer/write/reboot.
5. Verify post-boot firmware, SFN reconnection, outlet state, voltage/current/energy, and numeric RSSI before considering any product OTA feature.
6. If Wi-Fi provisioning is lost after the update, re-provision the same unit to SFN Power; do not infer failure solely from the first disconnect.

No production firmware write had been performed when this note was created.
