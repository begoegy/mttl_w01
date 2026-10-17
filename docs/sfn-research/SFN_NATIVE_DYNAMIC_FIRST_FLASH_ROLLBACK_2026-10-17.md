# SFN Native dynamic first-flash rollback — 2026-10-17

Status: **LAB PASS / PHYSICAL CANARY REQUIRED / NO DEVICE WRITE**

## Root cause fixed
The prior first-boot Boot Guard recognized only the exact 1.0.70 rescue image. That made a direct stock 1.0.116 -> Native migration unable to prove rollback to the actual 1.0.116 bytes.

The corrected first-flash path no longer assumes a stock version. On the first Native boot it validates the actual previous Image2 slot, requires hardware identity `LGU+-TAP-HW002`, validates the image header/checksum/signature, records the exact byte count and normalized SHA-256, and stores a verified copy of the previous first 4 KiB before arming the trial.

Unknown/nonblank spare sectors are never erased during adoption. They must be blank or bitwise-compatible with the exact interrupted adoption material; otherwise adoption fails closed and the server must not promote the canary.

## Recovery contract
- Previous stock image remains in its existing slot.
- Native trial image remains boot-selectable while rollback metadata is prepared.
- Failed Native trial restores the previous image first sector with the active boot signature, verifies the stored full-image identity, then demotes the Native candidate.
- Native-to-Native Update Guard continues to use the same previous-image preservation model.
- Legacy exact 1.0.70 rescue remains only as a compatibility fallback; it is not the primary universal first-flash rollback mechanism.

## Device/server proof signal
Native adds read-only command `up:sfn:96` and replies `up:sfn:96:<available>:<trusted>`.
The server integration must refuse to mark a dynamic first flash successful unless the device reports `available=1`, and after the trust window it must report `trusted=1`.

## Lab results
- strict compile of the modified Update Guard source: PASS
- two flash-slot orientations: PASS
- first-flash adoption write cuts: 34 PASS (17 per orientation)
- fail-closed cases: 4 PASS (occupied spare sector, wrong hardware field, corrupt checksum, invalid candidate signature)
- existing Update Guard preparation cuts: 17 PASS
- existing Update Guard rollback cuts: 17 PASS
- Boot Guard power-cut suite: PASS
- vendor Wi-Fi migration: PASS
- child lock / Voltage Guard: PASS
- offline schedule / DST / wire protocol: PASS
- settings migration/persistence: PASS
- final relay-state confirmation: PASS

Actual stock image model tests, both slot orientations and exact previous-image restoration:
- stock 1.0.66: PASS
- public stock 1.0.105: PASS
- stock 1.0.146: PASS

Stock package checksum normalization accepts only the three known boot-signature checksum forms: erased signature bytes, `81958711`, or `01958711`. Unknown forms fail closed.

## Boundary
Exact stock 1.0.116 bytes are still unavailable. The dynamic design avoids needing a server-side copy for rollback because the device validates and preserves its own exact previous slot. This removes the old 1.0.70 dependency, but physical 1.0.116 -> corrected Native boot/rollback is still not proven until the owned New3 canary is explicitly approved and run.

New3 was not written, rebooted, or sent OTA during this work.
