# SFN Native New2 Guarded Canary Preparation — 2026-10-17

Status: **READY FOR EXPLICIT PHYSICAL CANARY DECISION / NOT SENT**.

## Physical New2 baseline established safely
MAC `88D03907723D` (`New2`) was moved through two previously proven steps:

1. `0.1.32-1.0.38` → exact stock `0.1.54-1.0.66`.
   - job `b0f884f01131f803`
   - full 327944-byte transfer
   - reconnect + target-version verification PASS
2. `0.1.54-1.0.66` → neutral bridge `0.1.54-1.0.70`.
   - job `b5d4e14023643cb6`
   - full 327944-byte transfer
   - reconnect + target-version verification PASS
   - exact 1.0.66 rollback was available and not needed.

New2 remained online with all four outlet reports, voltage/RSSI telemetry and healthy memory after the bridge. No native firmware has been sent yet.

## Why native first boot is different
No vendor-Wi-Fi credential location was accepted on inference alone. The native build therefore does **not** claim transparent import of the stock Wi-Fi password. First native boot intentionally enters its own setup AP when no native settings record exists:

- AP: `MTTL-W01-SETUP`
- setup address: `http://192.168.4.1`
- successful join commits Wi-Fi + pinned SFN backend atomically.

A minimal setup-only page replaced the large standalone UI in the SFN-native profile. Compressed UI fell to 972 bytes, increasing static memory headroom while preserving the setup API.

## Exact rescue boot guard
The native canary embeds the exact first 4 KiB of the proven 1.0.70 rescue and SHA-256 identities for the complete normalized rescue/tail.

Guard rules:
- exact geometry only: 1 MiB flash, Image2 slots `0xB000` / `0x80000`;
- guard journal uses `0xED000` only if it is erased; unknown/nonblank guard state fails toward the verified rescue;
- exact old-slot tail/full hash is verified before destructive rescue work;
- native slot is identified by one build marker present exactly once in the image;
- first two untrusted native boots are allowed; the third requests rescue;
- 15 minutes without becoming trusted requests rescue;
- trust requires native settings to be persistent **and** 60 seconds of continuous SFN connectivity;
- rescue sector 0 is rebuilt and verified before the native boot signature is demoted;
- native remains boot-selectable through interrupted rescue writes;
- OTA runtime write remains disabled in the native application.

Residual limitation: if the native image never executes far enough to run the guard at all, no in-application software guard can recover it. This remains a real physical-canary risk.

## Guard power-loss gate
The pure C guard was exercised with the exact 1.0.70 rescue and with a permanent synthetic fixture.

- 19 distinct write-cut positions across rescue-sector reconstruction and signature commit: PASS.
- Every interrupted pre-commit case left native boot-selectable.
- Next native boot detected incomplete rescue through the intact exact tail and completed recovery.
- Unknown/nonblank guard journal + exact rescue: rollback path PASS.
- corrupted rescue tail: destructive rollback blocked.

Result: `SFN_BOOT_GUARD_REAL_RESCUE_POWER_CUT_TESTS=PASS` and `SFN_BOOT_GUARD_POWER_CUT_TESTS=PASS`.

## Production-endpoint candidate
Pinned backend: `169.58.193.85:10086`.

Exact FWR:
`/srv/sfn-power-research/lab-artifacts/sfn-native-new2-canary-20261017/READY-NOT-SENT-sfn-native-2.0.12-new2-guard.fwr`

- bytes: `314540`
- SHA-256: `b829e849753a2c953c398c826315c864bddc7fec5f50321ff33991c76605f26f`
- two independent builds: byte-identical PASS
- static gap to reserved MSP: `13800` bytes
- initial RTOS heap: `39936` bytes
- guard marker occurs exactly once
- embedded rescue FWR SHA: `0cbf05da978ef3985439e9a1d40e71a8bcd5dea7988a800217615873783472a2`
- native OTA write: disabled
- `device_written=false`
- `production_ready=false`

## Server contract regression
Against exact deployed `/opt/sfn-power/sfn-power.pyz` SHA `828997ffa4ddf77fcefa1ae975e02a9daeb14b57dda9a33f942fd9177236908a`:

`SFN_PROD_1_6_9_NATIVE_E2E=PASS`

Coverage includes BootInfo, ON/OFF, voltage/RSSI, Child Lock, guarded voltage policy, clock/schedules, disconnect/reconnect, schedule persistence across reconnect, fail-closed unsupported policy/schedule requests, and OTA rejection.

## Hard physical procedure
Do not silently send this candidate. Immediately before a physical canary:
1. verify New2 is still online on exact `0.1.54-1.0.70`;
2. preserve DB + exact 1.0.70 artifact;
3. register a New2-only native package and New2-only exact 1.0.70 rollback;
4. owner must be ready to join `MTTL-W01-SETUP` and submit Wi-Fi credentials after reboot;
5. use the exact tested FWR above — do not rebuild after this gate.
