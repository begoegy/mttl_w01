# SFN Native production-signed canary lock — 2026-10-17

Status: **CANARY READY IN LAB / NOT FLEET READY / NO PHYSICAL WRITE**

Artifact source commit: `65580434656e806d8f24720b6acb28eca6253ce4`.
Server integration source commit: `351bbcad516bde36cddaa00f31b4bdeabc72fb6c`.

## Locked first canary

The only first-physical-canary Native artifact is **2.0.21 / sequence 20021**.

- FWR bytes: `323920`
- FWR SHA-256: `a98102c41775820a2fcbc2d91fe754a59967fc8f3cf0154573b97640baecc806`
- BIN SHA-256: `490837cd7decd914c01fdedd4920ac5da8d4f0336b464c15c7d8aa96ecb2a5ac`
- detached signature SHA-256: `23b662c9e8a9eef0ebe2e11caf7c8f07d7a7e2d12d5df5f4b90eb499a30d38ce`
- production public-key fingerprint: `1af9dcb260cfaba8dd37a46ad7d6b5a1520c0dbec1c77665104820a4dff50b44`
- two independent builds are byte-identical for both FWR and BIN.

The locked Native successor is **2.0.22 / sequence 20022**:

- FWR SHA-256: `7abe9bed41dd564e410a3becc0eb94cafea01d6118744642522a34d20178c4a7`
- BIN SHA-256: `93deb72961a421e5c9215ec64adfa9fd03f5dabb2edb718954d433a48df0ee72`
- detached signature SHA-256: `7e881af62aad873bbee897161019c001370a4deb83be866e77b9f687bfd1f6e4`
- two independent builds are byte-identical.

2.0.17, 2.0.19 and 2.0.20 are superseded and must not be selected for the physical canary.

## Lab proof on locked artifacts

PASS:

- actual stock 1.0.66 -> 2.0.21 -> exact same stock, both flash-slot orientations;
- actual stock 1.0.105 -> 2.0.21 -> exact same stock, both orientations;
- actual stock 1.0.146 -> 2.0.21 -> exact same stock, both orientations;
- 2.0.21 -> 2.0.22 -> failed trial -> 2.0.21 rollback;
- 8 partial-download cuts and 4 activation-cut states;
- Update Guard: 17 preparation cuts and 17 rollback cuts;
- Boot Guard power-cut suite;
- production Ed25519 public-key wiring and signature success/failure behavior;
- downgrade and same-sequence rejection;
- vendor Wi-Fi migration and retry/fallback;
- settings persistence/migration;
- child lock and local Voltage Guard;
- offline schedules / DST;
- final relay-state confirmation;
- SFN backend and reconnect protocol.

Server integration passed **573/573 tests**. The reproducible candidate PYZ SHA-256 is `4014fcbcadc383cde6b9c62b9b22d89c76e45e8936bc12336ac3963f4a3d59a0`. It contains signed Native OTA, dynamic previous-image recovery, guard-status verification and a mandatory persistent-gateway physical-session identity for guarded updates.

## Remaining physical boundary

Exact stock 1.0.116 bytes are not available, so its vendor Wi-Fi record cannot be byte-proven before the owned New3 canary. The importer remains read-only and fail-closed; if the vendor record is not accepted, Setup AP remains the recovery path.

The final manifests intentionally remain `production_ready=false` and `target_boot_verified=false` until the owned stock 1.0.116 canary physically proves first boot and guard availability/trust. No software-only test can make the first physical boot risk exactly zero.

Production server files were not deployed or restarted during this repair. New3 was not flashed, rebooted or sent an OTA command.
