# SFN Local Queue — exact 0.1.55-1.0.146 family compatibility gate

Date: 2026-10-17
Status: **Build-ready for a future owned/dedicated physical canary. No OTA authorized by this gate.**

## Exact source and rollback

Fleet source: `0.1.55-1.0.146`, model family `mttl-w01-hw002-r1`.
Exact vendor image size: 327,944 bytes.
SHA-256: `ae997d79f8e54376a4eb94e447cf94582311544aa128ff4a487d2190f67208ad`.
The exact same bytes are the rollback image. No guessed or cross-family rollback is accepted.

The already-proven Neutral 1.0.147 lineage was independently rechecked: stock 1.0.146 ->
Neutral 1.0.147 differs only in the four reviewed legacy-service host slots, version identity,
and checksum. That Neutral image previously completed a real physical canary on the owner's
`الصاله` device; this document does not perform or rely on any new physical operation.

## Static compatibility proof

The 1.0.146 binary is not assumed to equal 1.0.66 globally. Broad parser/relay areas do contain
version-specific differences. The port therefore proves only the exact code/data it uses:

- stock timer core is byte-identical to the proven 1.0.66 lineage;
- the exact reclaimed factory-test, main payload and queue-data regions are byte-identical;
- every external function entrypoint called by Local Queue has identical code at the reviewed
  entry window;
- the relay jump-table dispatcher used by the queue is exactly identical;
- the timer-service, relay-dispatcher and native timer-control xrefs are independently decoded
  from the exact 1.0.146 image and resolve to the reviewed addresses;
- the generated Local Queue command/test/main/data bytes are exactly identical to the physical
  1.0.81 New canary artifact.

The direct stock 1.0.146 -> lab 1.0.150 diff is bounded to: four neutral host slots, firmware
version, `test` -> `sfnq`, the reviewed queue code/data regions, and checksum. There are zero
unexpected decoded-byte changes.

## Lab target

Lab-only target: `0.1.55-1.0.150`.
Artifact path: `/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.150-local-queue-hw002-r1b-lab.fwr`.
SHA-256: `14a89fdf7cff3961d6bc116c10601ff2b319656c557ea7fc7460d85f64aaa9b4`.

Two independent builds produced the same SHA and `cmp` passed. The artifact was not copied into
the production firmware catalog and was not offered to any device.

## Host/emulator and model QA

The exact 1.0.150 artifact passed the same Unicorn machine-code Local Queue emulator used for the
accepted New 1.0.81 candidate, including Begin/Item/Commit barriers, CRLF parsing, incomplete
replacement safety, stale timer handling, local/offline relay execution, duplicate commit,
strict numeric parsing, one state/one stock timer, no queue worker task, timer-creation failure,
timer-command failure, relay failure status and cold-boot RAM queue loss.

The accepted Local Event Queue + pre-canary contract model suite also passed **57/57**.

## Promotion decision

`0.1.55-1.0.146` is promoted from **Research** to **Build-ready** only.

Before any first physical OTA for this family:

1. select one owned/dedicated canary currently on exact stock 1.0.146;
2. register exact stock 1.0.146 rollback for that MAC only;
3. register the exact SHA-pinned 1.0.150 candidate as canary-only, hidden from general offers;
4. show the user the exact device, transition, risk and rollback;
5. obtain explicit approval for that first OTA;
6. stop immediately on any transfer/install/post-boot verification failure.

No customer device is a valid first-family canary merely because it reports 1.0.146.

## Evidence

- `evidence/local-queue-family-1.0.146-20261017/static_proof_1_0_146.py`
- `evidence/local-queue-family-1.0.146-20261017/static-proof-results.txt`
- `evidence/local-queue-family-1.0.146-20261017/exact-1.0.150-emulator.txt`
- `evidence/local-queue-family-1.0.146-20261017/model-tests.txt`
- `evidence/local-queue-family-1.0.146-20261017/reproducible-build.txt`

No physical strip was contacted by this stage.
