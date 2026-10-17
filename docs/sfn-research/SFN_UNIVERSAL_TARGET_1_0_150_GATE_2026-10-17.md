# SFN Universal MTTL-W01 target 1.0.150 — lab gate (2026-10-17)

Status: **LAB PASS / PHYSICAL CANARY NOT AUTHORIZED**.

## Goal

Use one SFN target firmware for the proven `LGU+-TAP-HW002 / MTTL-W01` platform instead of treating every vendor application version as a separate hardware family. Keep rollback exact to each device's pre-flash source image.

## Exact target

- Application lineage: exact Voltra `0.1.55-1.0.146` -> neutral `1.0.147` -> reviewed Local Queue `1.0.150`.
- Universal-envelope artifact: `/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.150-universal-envelope-lab.fwr`.
- SHA-256: `b75e394e77c7ec2e8598bb98d157980bd889cc06989cf8fc9702d6bfc20fba8f`.
- Size: `327944` bytes.
- Internal version: `0.1.55-1.0.150`.
- Generic FWR header tail: `ff ff ff ff ff ff ff ff`.

The universal-envelope build changes only the FWR envelope bytes 8..15 and the trailing additive checksum versus the already-reviewed 1.0.150 application candidate. Decoded application bytes 16..(end-4) are byte-identical.

## Source compatibility evidence

Exact originals checked:

- `0.1.54-1.0.66` SHA `d780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04`.
- `0.1.54-1.0.105` SHA `f78aee903d392f2b8470c845d9e76c33d91fdcf1da57e6c7e714cbedc9edaefa`.
- `0.1.55-1.0.146` SHA `ae997d79f8e54376a4eb94e447cf94582311544aa128ff4a487d2190f67208ad`.

All three have the same MTTL-W01 Image2 geometry, load address `0x10006000`, entry `0x10006071`, model/hardware identity and valid additive checksum. The three stock sources are byte-identical in the factory-reclaim region, queue-reclaim region and timer-service region. `1.0.66` and `1.0.105` are also byte-identical in the reviewed timer-parser and relay-dispatch regions. `1.0.146` contains the newer `childlock` protocol and its newer relay-dispatch bytes; the universal target preserves both.

Evidence: `docs/sfn-research/evidence/universal-target-1.0.150-20261017/source-compatibility.txt`.

## Independent FWR validation

The universal-envelope artifact passes the independent public `af950833/mttl_w01_standalone` Image2/FWR inspector as `LG U+ FWR (XOR)` with correct geometry, checksum, entry and versions.

Evidence: `docs/sfn-research/evidence/universal-target-1.0.150-20261017/independent-validator.txt`.

The public inspector's strict `ff..ff` uninstalled-header policy rejects the original Voltra 1.0.105/1.0.146 package envelope even though those packages decode and checksum correctly and the 1.0.146 lineage has already completed a physical SFN OTA canary. The universal envelope deliberately uses the generic header while leaving application bytes unchanged.

## Queue / behavior QA

- 1.0.150 source-derived builder reproducibility: PASS.
- Universal-envelope reproducibility/body identity: PASS.
- Machine-code Unicorn Local Queue harness: PASS (real-wire CRLF parser, atomic staging, duplicate commit, malformed fail-closed, offline local execution, timer/relay failure handling, cold-boot queue loss semantics).
- Local Event Queue + pre-canary contract models: **57/57 PASS**.

Evidence: `docs/sfn-research/evidence/universal-target-1.0.150-20261017/model-tests-57.txt` plus the existing exact-1.0.150 emulator evidence.

## Rollback policy

One target artifact does **not** mean one rollback image. `rollback-map-lab.json` pins exact rollback bytes independently for 1.0.66, 1.0.105 and 1.0.146. Every referenced target/rollback SHA exists and was re-verified. No wildcard rollback is allowed.

This map is LAB-ONLY. It has not been registered in the production firmware catalog.

## Remaining physical gate

Static and emulator evidence cannot prove migration of every persisted NVM setting across a real old-version -> 1.0.150 boot. A physical canary is still required to prove at minimum:

1. firmware download/install/reboot from an older source;
2. controller reconnect without manual re-pair;
3. four outlet controls and reported states;
4. voltage/current/RSSI telemetry;
5. saved Wi-Fi/controller state survives or is recovered by the existing provisioning path;
6. `childlock` remains functional on the new target;
7. Local Queue read-only status is healthy before any relay-changing scheduled event;
8. exact source rollback can restore the device if any mandatory check fails.

**STOP GATE:** no physical OTA, no catalog registration, no customer rollout and no relay-changing canary is authorized by this document. Obtain explicit user approval before the first device-changing action.
