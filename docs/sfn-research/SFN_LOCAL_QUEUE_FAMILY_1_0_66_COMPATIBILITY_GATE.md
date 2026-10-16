# SFN Local Queue — 0.1.54-1.0.66 family compatibility gate

Date: 2026-10-17. Scope: host-only compatibility proof for the largest fleet source group (`24` devices). No device command, OTA, Flash, relay operation, production catalog write, database write, service restart, or deployment occurred.

## Result

`BUILD_READY_FOR_DIRECT_1_0_66_CANARY_PREP`

This is **not** canary authorization and is **not** Production-ready. The next physical OTA still requires a separately registered exact rollback path, MAC scope, pre-state capture, and explicit user approval.

## A) Source pin — PASS

Exact source:

- reported firmware: `0.1.54-1.0.66`
- model family evidence: `lgutap / MTTL-W01 / LGU+-TAP-HW002`
- file: `work/firmware/1.0.66/comMTTL-W01_1.0.66.fwr`
- size: `327944`
- SHA-256: `d780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04`
- source history: tracked in the research repository from commit `58e890e` and present byte-identically in multiple independent SFN/public snapshots and the protected SFN firmware catalog
- family fingerprint already dry-run proved by the server intake inspector: `mttl-w01-hw002-r1` / `5d5464470f0b6e9474fbdc34da0614b84a88caa1ae51e2d49b7a6eff7a74abfc`
- exact rollback bytes for a future direct-source canary: the same stock 1.0.66 artifact and SHA above

## B) Exact-binary static proof — PASS

A dedicated static gate decoded the exact stock 1.0.66 bytes and independently proved the exact addresses/xrefs used by the Local Queue architecture; no address was accepted merely because it existed in 1.0.71.

PASS on exact 1.0.66:

- `memory` diagnostic and formatter preserved
- factory `test` parser ingress present
- Local Queue state pointer candidate at `0x1001EE98` is zero in source
- 1000 Hz tick constant at `0x10013714`
- timer-service task creation call at `0x10012272 -> 0x10010068`
- timer object allocation path uses `0x28` bytes and allocator `0x100120A0`
- timer callback dispatch loads callback at object `+0x24`
- stock timer callback path `0x10015AFC -> relay dispatcher 0x1001AB42`
- native timer control path `0x100160B2 -> 0x100122B8`
- timer-service creator explicitly initializes the extra stack arguments and requests the existing `0x200`-word timer-service stack

The exact stock 1.0.66 decoded image was then compared byte-for-byte with the exact 1.0.71 golden source used for the successful 1.0.81 canary. There are **zero unexpected differences**. Every differing byte is confined to the already-known neutral cloud-host data, firmware version identity, or additive checksum. Therefore the reviewed code regions are not merely similar; their bytes are identical on these exact two sources.

Pinned source-region hashes:

- factory-test candidate region `0x1001606A..0x10016130`: `5a59b6ffe697a8e26a9a08ffa2ae9c08f40f941a052008b7a56c5f18eea40318`
- main candidate region `0x1001EB6C..0x1001EE54`: `a54fc12574a6b67d382380f24c8e16f7d0e28674ba15b90d9eec3f370eb25fc1`
- candidate data region `0x1001EE5C..0x1001EE9C`: `0af8fcc4674748c23616887479821710c663976aac92551f8431aee49e5e97d4`

## Direct source -> exact tested target proof — PASS

No new firmware was built.

The existing physically canary-tested target was used unchanged:

- target: `0.1.54-1.0.81`
- exact artifact: `comMTTL-W01_1.0.81-local-queue-r8fix-new-lab.fwr`
- SHA-256: `5b96489849e8808512a0e3f918013102ced8cb5610f939ae49da60cc2220ff0a`

A decoded byte-for-byte comparison of **stock 1.0.66 directly to this exact 1.0.81 artifact** found zero changes outside the approved union of:

1. the four neutral host slots;
2. version bytes;
3. `test` -> `sfnq` command bytes;
4. the reviewed factory-test code region;
5. the reviewed main code region;
6. the reviewed queue data/pointer region;
7. checksum bytes.

All preserved model, OTA, reboot, memory, RSSI, query, connection, and hardware markers remain present in the exact target.

This means the 1.0.66 family does **not** require a newly rebuilt firmware artifact for its first direct-path canary. Reusing the exact already-tested 1.0.81 bytes is safer than creating a byte-different rebuild.

## D) Host / emulator QA — PASS for the exact target

The exact 1.0.81 artifact was executed under the accepted Unicorn harness and passed:

- vendor `R8` delimiter preservation
- Begin / Item / Commit / Status stage ACK contract
- real CRLF parser
- timer queue machine code
- incomplete replacement preserves active generation
- stale timer replacement does not relay early
- local execution without server/network interaction
- duplicate commit idempotence
- malformed input fail closed
- strict numeric parser
- single state allocation + single stock timer
- no new worker task
- timer-create failure
- timer-command failure status
- relay failure status
- cold-boot queue loss
- capacity rejection above 8 resident events

The accepted 57 Local Event Queue / contract model tests were also rerun and passed `57/57`. They include same-time OFF-over-ON behavior, stale replacement, reconnect behavior, reboot loss/rebuild semantics, manual override/revision behavior, capacity/resource failures, group/master safety contracts, and fail-closed error cases.

Server-side schedule compilation/reconciliation remains the already-deployed 1.6.8 path whose full server suite is `517/517 PASS`; no server behavior was changed by this research gate.

## E) Reproducible artifact decision

The accepted 1.0.81 build review already records two independent builds as byte-identical. Because this family gate reuses that **same SHA-pinned artifact**, it deliberately does not rebuild and create a second target.

Reproducibility evidence therefore remains attached to SHA `5b964898...`; the direct 1.0.66 static-diff gate adds source-path compatibility evidence without changing target bytes.

## F) Rollback gate — bytes ready, manager registration still pending

Exact stock 1.0.66 rollback bytes exist and are SHA-pinned. However a future direct 1.0.66 -> 1.0.81 canary still needs a MAC-scoped rollback manager entry that explicitly permits target 1.0.66 from source 1.0.81 for the selected canary.

No production firmware manifest was changed by this gate.

## Physical canary plan boundary

The safest dedicated device remains `New / 88D039394D73`, which is owned and already proved on both stock 1.0.66 and the final 1.0.81 line historically. To prove the **direct** source path without using a customer device, the physical sequence would need to establish New on exact stock 1.0.66, verify it, then perform one direct OTA to the exact tested 1.0.81 artifact with exact 1.0.66 rollback registered.

That sequence contains real OTA operations and is **not authorized by this document**. Stop before the first OTA and present the exact transition/risk/rollback to the user.

## Evidence

- `evidence/local-queue-family-1.0.66-20261017/static_proof_1_0_66.py`
- `evidence/local-queue-family-1.0.66-20261017/static-proof-results.txt`
- `evidence/local-queue-family-1.0.66-20261017/exact-1.0.81-emulator.txt`
- `evidence/local-queue-family-1.0.66-20261017/model-tests.txt`

No physical strip was contacted by this stage.
