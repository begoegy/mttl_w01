# SFN Native Production Base / Signed OTA QA — 2026-10-17

Status: **LAB PASS / NOT PRODUCTION READY / NO PHYSICAL WRITE**

## Scope

Base: clean Native 2.0.14 source at `393458c`. The production-base candidate keeps the already tested Native features and adds Native-to-Native signed OTA; it does not replace or remove child lock, Wi-Fi migration, offline schedules, Voltage Guard, final-state relay confirmation, local/offline operation, or Boot Guard.

## OTA trust model

- Native release identity is `sequence + encoded FWR byte count + SHA-256`.
- The OTA command carries no IP/host. The device uses its configured backend address.
- The candidate fetches a detached Ed25519 signature and verifies it with the public key embedded at build time.
- The signing private key is not stored in the repository or firmware.
- Same-version and downgrade sequences fail closed.
- Only the inactive Image2 slot is staged; bootloader/factory/calibration areas are not replaced.
- Download integrity, decoded-image validation, readback, release marker, and signature checks must pass before activation.
- Update Guard preserves recovery material for the previous Native image. A candidate must earn trust after sustained SFN connectivity; failed trial boots/timeouts request rollback.
- Cloud/server failure does not disable local relay, schedules, child lock, or Voltage Guard behavior.

## Regression gates

PASS:

- SFN UP protocol and reconnect tests.
- Child lock + sustained Voltage Guard feature tests.
- Schedule transaction/wire/DST tests.
- Vendor Wi-Fi migration test.
- Settings v9 migration/persistence test.
- Final-state relay confirmation test.
- Existing Boot Guard power-cut recovery test: 19 injected write-cut points.
- Update Guard preparation power cuts: 17 points.
- Update Guard rollback power cuts: 17 points.
- OTA HTTP/integrity parser tests.
- OTA sequence policy: downgrade/same reject, higher sequence accepts.
- Ed25519 authentication wiring test, including verifier success/failure behavior.
- Actual-artifact A/B flash model: `2.0.15 -> 2.0.16 -> rollback` PASS.
- Partial download cuts: 8.
- Activation cut states: 4.
- Existing deployed server 1.6.9 relay contract E2E: PASS with production PYZ SHA unchanged.
- Full Native feature E2E against 1.6.9 lab process: relay, child lock, voltage, Voltage Guard, schedules, reconnect PASS.

## Reproducible 2.0.15 candidate

Built twice from the same source/inputs; FWR and BIN are byte-identical.

- FWR bytes: `322864`
- FWR SHA-256: `a9ef2e3477609e14da9cb79ca723a2d0d3b6586875b09528062aed0eb5036a7f`
- BIN SHA-256: `75bc452c1dc73978e55e32f2a6ba57bd9ab3dfd475ab6dc8e63b78dc2fe9adcc`
- sequence: `20015`
- static gap to reserved MSP: `6500` bytes
- initial RTOS heap: `39936` bytes
- relay/sensor enabled: yes
- signed Native OTA enabled: yes
- Boot Guard enabled: yes

Lab successor used for A/B model:

- version: `2.0.16` / sequence `20016`
- FWR SHA-256: `f6bf63a70ab3b9497862fc55bcb9b9ae45871cef85a289a5e5d00a70b199e32b`
- BIN SHA-256: `b923efeecaf530fbec81da39b9807329e11bafb209d658d8cce26b12dba67e3c`

Lab public-key fingerprint embedded in these lab builds: `6aff370c85e533b0f3359fd973e778c950ceff13e9de956abaf15db36a4a7ea1`. This is not a production signing key.

## Server compatibility

The production 1.6.9 relay behavior is intentionally unchanged. A separate server lab branch adds a family gate so exact legacy firmware remains explicitly gated while Native `2.0.x` releases from `2.0.15` onward expose child lock, local Voltage Guard and offline schedules. That server branch passed the full suite and is not deployed.

Signed OTA delivery/trigger support on the server is still a separate pre-production task; current production 1.6.9 emits the legacy OTA command and must not be used to update this Native candidate.

## Safety boundary

- New2 `88D03907723D` remains physically on Native `2.0.12`; no OTA, reboot or write was sent during this QA.
- New and Salon are untouched.
- Production server process/files were not deployed or restarted.
- The 2.0.15 candidate is **LAB PASS**, not `SAFE FOR USERS`.
- Physical Native-to-Native OTA and physical rollback are not yet proven.
- A dedicated physical test unit is required before any customer rollout.
