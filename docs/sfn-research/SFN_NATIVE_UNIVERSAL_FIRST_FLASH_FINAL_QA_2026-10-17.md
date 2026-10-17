# SFN Native Universal First Flash — Final Lab QA (2026-10-17)

Status: **LAB QA PASS / NOT PRODUCTION READY / NO PHYSICAL WRITE**

## Scope

This work closes the first-flash rollback gap for the MTTL-W01 / `LGU+-TAP-HW002` hardware family without selecting a different Native firmware per stock version.

The first Native boot now adopts the exact image that was already running on the device as its rollback source. It does not rely on an embedded fixed 1.0.70 recovery image for the first transition.

## Firmware source

- Branch: `fix/native-universal-first-flash-rollback-20261017`
- Final firmware commit for this QA: `5f605a7dc997c0fff509ee81edf37a42b0024646`
- Key safety change in the final commit: first-flash adoption is erase-free. Empty metadata/backup space is programmed directly; unexpected non-empty space fails closed rather than being erased.

## Production-key builds

Both versions were built twice from clean detached worktrees at the commit above, using only the production **public** OTA key in the firmware.

### 2.0.19 / sequence 20019

- FWR SHA-256: `d0c8270f8d919689b1f1df3ca2420008052a7a64620edf0342be9d0f5820e8b4`
- Build 1 and Build 2 FWR: byte-identical
- Build 1 and Build 2 BIN: byte-identical
- OTA enabled: yes
- Signature required: yes
- Boot/Update Guard: yes
- Backend: `169.58.193.85:10086`
- `production_ready`: false

### 2.0.20 / sequence 20020

- FWR SHA-256: `16cc4e654709669abde5d87fe91b5bd1a85214db9de445b78ebfe7bce490bc59`
- Build 1 and Build 2 FWR: byte-identical
- Build 1 and Build 2 BIN: byte-identical
- OTA enabled: yes
- Signature required: yes
- Boot/Update Guard: yes
- Backend: `169.58.193.85:10086`
- `production_ready`: false

## First-flash rollback QA using real stock images

The final guard code was tested using real decoded stock images, in both possible slot orientations.

- `1.0.66 -> 2.0.19 -> same 1.0.66`: PASS
- `1.0.105 -> 2.0.19 -> same 1.0.105`: PASS
- `1.0.146 -> 2.0.19 -> same 1.0.146`: PASS

Each stock image was restored byte-for-byte as the rollback target; no fixed downgrade image was substituted.

The stock files all decode to the same `LGU+-TAP-HW002` hardware identity. Bootable stock signatures (`81958711` / `01958711`) are accepted by the first-flash checksum validation rather than being mistaken for a corrupt OTA package.

## Power-cut and failure QA

- First-flash adoption interruption recovery: PASS
- Update Guard prepare power cuts: PASS (`17` tested write-cut points)
- Update Guard rollback power cuts: PASS (`17` tested write-cut points)
- Boot Guard power-cut suite: PASS
- Native `2.0.19 -> 2.0.20` remote OTA pipeline: PASS
- Partial download cuts: PASS (`8` cut points)
- Activation cut states: PASS (`4` states)
- Native rollback after failed trial: PASS

## OTA authentication / policy QA

- Production public-key wiring in the actual 2.0.20 build: PASS
- OTA auth message format: PASS (`56` bytes)
- Signature length contract: PASS (`64` bytes)
- Same sequence rejected: PASS
- Downgrade sequence rejected: PASS
- Forward sequence accepted: PASS
- Explicit `up:sfn:96` guard-status command/reply: PASS
- Older valid production signatures were tested against the new 2.0.19/2.0.20 FWR hashes and were rejected: PASS. Signatures are therefore artifact/hash-bound and cannot be reused on the modified release.

## Server-side safety dependency

The matching server change is on branch `fix/universal-first-flash-guard-status-20261017`, commit `665471098b0492f11ace74cde1f564293a51a1fd`.

It requires the device to explicitly report rollback guard availability/trust. It also requires a real persistent-gateway physical session identity before a guarded update can start. A device being merely online is no longer enough to claim first-flash success.

Server full regression suite: **555 / 555 PASS**.

## Remaining production gates

1. Generate new detached production signatures for the exact final 2.0.19 and 2.0.20 FWR artifacts above. The OpenAI remote execution safety layer blocks use of the private signing key, so no new final signatures were produced by this session.
2. Do not reuse signatures from the older `115b4a9` artifacts; they correctly fail against the final hashes.
3. Stage/deploy the matching server commit only through the normal safe deployment/rollback process.
4. Run one physical canary only after the exact final artifact signatures and server runtime are in place.
5. Physical proof sequence should be: stock New3 `1.0.116` -> final signed 2.0.19 -> final signed 2.0.20 -> forced failed trial/rollback proof.
6. Keep fleet rollout blocked until the physical canary passes.

## Device safety

- New2 was not touched.
- New3 (`2CE032C679FC`, stock `0.1.54-1.0.116`) was not flashed, restarted, or used for any write during this work.
- No production device firmware write was performed.
