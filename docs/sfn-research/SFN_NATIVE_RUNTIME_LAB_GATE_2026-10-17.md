# SFN Native Runtime Lab Gate — 2026-10-17

Status: **LAB PASS / DO NOT FLASH / NOT PHYSICAL APPROVED**

## Safety boundary
- New2 `88D03907723D` was not written, upgraded, rebooted, or sent an OTA command during this work.
- No customer device was touched.
- The generated FWR is kept only under `lab-artifacts` with a `DO-NOT-FLASH` name.
- No file was added to the active production firmware catalog.

## What is in the native runtime
- Source-built RTL8711AF/Ameba firmware path.
- Meter + relay GPIO profile.
- Offline persistent scheduler.
- SFN dynamic voltage guard with trip/recovery hysteresis and ON veto.
- 5-bit child lock; short physical presses can be locked while the master long-hold recovery/setup path remains available.
- Clean-room SFN UP wire codec and bounded TCP10086 client.
- Persistent SFN backend address/port in a versioned sidecar while retaining legacy settings read compatibility.
- Settings flash persistence in `0xEE000` / `0xEF000`.
- **No runtime OTA write profile.**

## Build isolation
Builds run only on `vmi3653994` under the research worktree.

Pinned lab toolchain:
- GNU Arm Embedded GCC 4.8.3 / 2014-q1 Linux.
- Locked Ameba SDK plus 49 verified overlays.
- Locked Terser dependency tree.
- Unicorn 2.1.4 + Zopfli 0.4.3 build-time modules.

The Linux builder accepts the same source while resolving `arm-none-eabi-*` tools without Windows-only `.exe` assumptions.

## Build profile
`NETWORK_TEST_NO_OTA_RAM_SETTINGS`

Feature flags:
- `sfn_native_up_only = true`
- `relay_enabled = true`
- `sensor_gpio_enabled = true`
- `ota_write_enabled = false`
- `settings_persistent = true`
- initial RTOS heap = 39 KiB
- reserved MSP = 4096 bytes
- static gap to MSP = 12680 bytes

The SFN native profile deliberately decouples the 39 KiB hardware heap and settings flash handle from the historical OTA-test profile. Enabling relay/meter no longer requires enabling OTA runtime.

## Lab FWR
Size: `315820` bytes

SHA-256:
`4a8864d1a85b96b732dcaba893169e70135f72664d1c1c2502cc677595da1038`

Two clean builds produced byte-identical FWR files with the same SHA-256.

Preserved lab-only artifact:
`/srv/sfn-power-research/lab-artifacts/sfn-native-20261017/DO-NOT-FLASH-sfn-native-2.0.12-lab.fwr`

Marker gates on the decoded image:
- `NETWORK_TEST_NO_OTA_RAM_SETTINGS` present.
- `up:bootinfo`, `up:getinfo`, `up:childlock`, `up:cl` present.
- `up:ota` absent.
- `NETWORK_OTA_TEST_RAM_SETTINGS` absent.

## Test results
Host gates:
- `SFN_NATIVE_FEATURE_TESTS=PASS`
- `SFN_SETTINGS_SIDECAR_V7_TESTS=PASS`
- `SFN_UP_PROTOCOL_TESTS=PASS`
- `SFN_UP_CLIENT_TESTS=PASS`
- production SFN parser cross-check = PASS
- child-lock parser cross-check = PASS

ARM gates:
- all SFN runtime modules compile with GCC 4.8.3.
- link = PASS.
- Image2 package round-trip = PASS.
- setup machine-code start call = PASS.
- setup ARM emulation success/failure cases = 2/2.
- reproducible FWR = PASS.

## Linker warnings
The legacy Ameba SDK/toolchain emits a large warning baseline (old C++ literal syntax and enum ABI warnings). The SFN native build has 1033 warning lines; the historical non-SFN profile compiled with the same SDK/toolchain emits the same warning classes and slightly more enum warnings. No SFN source compile warning was found; SFN object names only appear in the generic linker enum warning shared with the SDK objects.

## Failures caught before the successful gate
1. ARM compile caught forward declarations for `clock_ms` and `control_enqueue`; declarations were fixed without changing behavior.
2. First non-OTA hardware link exposed a 100 KiB heap inherited from the old network-only profile and a missing shared flash handle.
3. The root cause was historical coupling of relay/meter + persistent flash to the OTA-test profile.
4. Native runtime now has a dedicated heap profile and settings-only flash runtime with no OTA session.

## Hard next gate
Do **not** flash New2 or any other normal device from this artifact.

Before a physical native canary:
1. a proven recovery path must exist first;
2. preferably use a sacrificial dedicated test strip;
3. finish provisioning/backend-address flow and runtime integration QA;
4. preserve and verify the exact artifact hash before any physical approval;
5. explicit owner approval is required before any device firmware write.
