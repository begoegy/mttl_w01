# SFN Full Local Engine 1.0.151 — Phase 1 result

Date: 2026-10-17
Status: **LAB PHASE 1 PASS — NO DEVICE I/O / NO OTA / NO CATALOG REGISTRATION**

## What Phase 1 establishes

The 1.0.151 candidate is built from the exact physically-proven neutral 1.0.147 vendor lineage and replaces the mutually-overlapping standalone research payloads with one shared local runtime and one A/B persistent record.

The shared engine now proves, in the exact machine-code artifact:

- durable wall-clock schedules and local relay execution;
- persistent local voltage policy;
- voltage trip enforcement at the common relay dispatcher, so protected ON requests cannot bypass the latch;
- safe OFF commands while tripped;
- stable-voltage recovery/optional restore;
- A/B flash recovery of schedules and voltage policy after a cold boot model;
- the vendor `childlock` parser/implementation remains byte-for-byte preserved for the later child-lock semantics phase;
- normal universal FWR envelope generation is deterministic.

The former standalone `sfnq` Local Queue payload is **not stacked into 1.0.151**. Its old code windows collide with both the durable scheduler and voltage guard. Phase 1 deliberately uses one local event engine rather than carrying a second competing firmware runtime. Any compatibility mapping for old short-horizon queue callers must target this engine at the server/protocol layer and is not claimed by this phase.

## Artifact identity

Direct R3 artifact SHA-256:
`e70ba96726dc2a0093685fe5f5fb9e0bc8c71017a6676c51bf2abb3301a76098`

Universal-envelope R3 artifact SHA-256:
`44e46fa430d99492e37ca0ad50fdec3d8eefb7a8d4ac8abf5f8344fc05f4c36b`

Both direct builds were reproduced byte-for-byte. Both universal-envelope builds were reproduced byte-for-byte. The universal repack changes only the FWR envelope/header/checksum; the application body is unchanged.

## QA evidence

- host storage/arbitration model: 9/9 PASS;
- legacy-payload collision gate: PASS by refusing legacy stacking and requiring a shared engine;
- exact Unicorn execution of the final universal R3 bytes: PASS for boot recovery, voltage-policy persistence/ACK, schedule persistence/due execution, forced OFF on voltage trip, common relay ON veto, safe recovery restore, and preserved vendor child-lock bytes;
- exact 1.0.147 rollback lineage remains available; no 1.0.151 device action is authorized by this phase.

## Next gate

Phase 2 is to prove the vendor child-lock command semantics (`0/1` and persistence/physical-button behavior) and wire that capability without replacing the vendor implementation. No physical OTA should be started until that gate and the final pre-canary QA are complete and Salem explicitly approves the new flash.
