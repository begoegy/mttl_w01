# SFN Native 2.0.14 — Vendor Wi-Fi Migration QA — 2026-10-17

Status: **LAB PASS / DO NOT FLASH / NOT PRODUCTION DEPLOYED**

## Decision

First stock-to-Native migration must behave like a normal upgrade whenever the vendor Wi-Fi record is valid. The firmware, not the production server, owns this compatibility step.

Native 2.0.14 adds a fail-closed, read-only importer for the vendor Wi-Fi record. Production server 1.6.9 remains unchanged.

## Proven vendor format

Prior static analysis of stock 1.0.66 and the proven neutral 1.0.71 path established the same vendor persistence implementation:

- record address: `0xFA000`;
- record length: `0xB4` (180) bytes;
- two fixed `0x58` Wi-Fi profiles;
- validity marker at `+0xB0`: `0x00005A5A`;
- stock boot rejects an erased first word even if a trailing marker exists;
- stock connection logic can select/retry the two stored profiles;
- normal bounded application OTA writes remain below `0xF0000`, so the vendor Wi-Fi record is outside that update range.

Native settings remain separate at `0xEE000` / `0xEF000`. No SFN data is written into the vendor sector.

## 2.0.14 migration contract

Migration is attempted **only** when the Native settings journal is proven blank (`mttl_sdk_settings_load() == 0`).

It is deliberately not attempted when:

- Native settings are already valid (normal Native-to-Native upgrade);
- Native settings contain corrupt or unknown data;
- the vendor record marker/first-word/field lengths fail validation.

On an eligible first boot:

1. Read exactly 180 bytes from `0xFA000` under the flash mutex.
2. Never erase or program `0xFA000`.
3. Validate each vendor profile with strict SSID/password bounds and terminators.
4. Before Setup AP is started, submit the first valid stored profile to the existing Wi-Fi lifecycle.
5. If it cannot join, recovery returns to AP state and the second valid profile is tried once.
6. A profile is persisted to Native settings **only after association + DHCP/IP succeeds**, using the existing atomic Native settings journal and the pinned SFN backend.
7. If no stored profile succeeds, the normal `MTTL-W01-SETUP` / `192.168.4.1` manual path remains available.
8. Manual setup/recovery bypasses automatic vendor import.

This follows the vendor's two-profile retry concept without modifying the vendor record.

## Host migration tests

`SFN_VENDOR_WIFI_TESTS=PASS` under host GCC `-Wall -Wextra -Werror -pedantic`.

Coverage includes:

- two valid stored profiles;
- first profile unavailable, second profile succeeds;
- no persistence of the failed first profile;
- one commit only after successful DHCP/IP;
- duplicate profile de-duplication;
- valid open network;
- invalid short password rejected;
- profile 1 valid when profile 0 is structurally invalid;
- erased first word + surviving marker rejected to match stock boot behavior;
- erased/invalid record falls back without import.

## Regression gates

PASS after the 2.0.14 change:

- `SFN_UP_PROTOCOL_TESTS=PASS`
- `SFN_UP_CLIENT_TESTS=PASS`
- `SFN_NATIVE_FEATURE_TIME_GUARD_TESTS=PASS`
- `SFN_FINAL_STATE_TESTS=PASS`
- `SFN_VOLTAGE_WIRE_POLICY_TESTS=PASS`
- `SFN_SCHEDULE_WIRE_TESTS=PASS`
- `SFN_SCHEDULE_SYNC_TRANSACTION_TESTS=PASS`
- `SFN_BACKEND_PROVISION_TESTS=PASS`
- `SFN_SETTINGS_SIDECAR_V9_DST_MIGRATION_TESTS=PASS`
- `SFN_BOOT_GUARD_POWER_CUT_TESTS=PASS`
- exact unmodified production server E2E: `SFN_UNMODIFIED_PROD_SERVER_E2E=PASS`
- complete Native production-protocol lab E2E: `SFN_PROD_1_6_9_NATIVE_E2E=PASS`

The broad legacy host compile still has its pre-existing misleading-indentation warnings in legacy settings/protection files. New migration source compiles warning-free under strict `-Werror`. The Ameba linker retains its established enum-ABI warnings across C objects.

## ARM build

Pinned Ameba build, SFN Native profile, sensor + relay + Boot Guard enabled, runtime OTA write disabled.

Two final builds were byte-identical:

- version: `2.0.14`
- FWR bytes: `316216`
- FWR SHA-256: `c8f1450ef4b0a3d6dc02e19d752a7443ae77ab9e9ef660446f78304a73b8eb34`
- BIN SHA-256: `3d112b40577a6d6106f7a058816dc881ac5b9689c61915195ebbaf86f81fb1d4`
- static gap to reserved MSP: `11864` bytes
- initial RTOS heap: `39936` bytes
- network worker compiled frame: `1288` bytes vs 6144-byte task allocation
- vendor parser frame: `128` bytes
- `ota_write_enabled=false`

Boot Guard retains the exact 1.0.70 rescue FWR SHA-256:
`0cbf05da978ef3985439e9a1d40e71a8bcd5dea7988a800217615873783472a2`.

Recovery is a safety mechanism, not a guarantee if a new image cannot execute its guard path.

## Production / physical boundary

- Production server was not changed, restarted, or deployed for this work.
- New2 was not written, rebooted, upgraded, or sent an OTA command during this work.
- No customer strip was touched.
- New2 currently already has valid Native settings from its manual 2.0.12 provisioning, so a future 2.0.12 -> 2.0.14 update should exercise the normal Native-settings-preservation path, **not** the first-stock-import path.
- Therefore stock -> Native automatic Wi-Fi migration is lab-proven but not yet physically proven on a fresh stock canary.

Do not claim customer rollout readiness until a stock device with blank Native settings passes a guarded physical first-migration test.
