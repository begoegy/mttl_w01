# SFN Native Production-Signed Canary — 2026-10-17

## Status

**CANARY READY / NOT FLEET READY / NOT DEPLOYED / NEW3 NOT WRITTEN**

The Native firmware source remains commit `36164089db544c4167c3926ceee88c62fc3aeaca`. The Phase 3 server rollback orchestration is commit `f09c679c65887e2601310413945aafa261449a38`.

## Production signing identity

A new Ed25519 production signing identity was created on the new VPS only. The private key is outside Git with restrictive permissions and was not copied to the laptop or repository.

Public raw-key SHA-256:
`1af9dcb260cfaba8dd37a46ad7d6b5a1520c0dbec1c77665104820a4dff50b44`

## First-flash candidate: 2.0.17

- OTA sequence: `20017`
- FWR bytes: `322864`
- FWR SHA-256: `a7698eea0b746c30910e01a2925a09291d03e398bcae3cb8e899467d0def85e9`
- BIN SHA-256: `4cfdcfae9ca020c28c4eb5107ea6d11bdc07a2778e2c696737a884dd1e40ff30`
- Signature SHA-256: `7e00af3020d00988ac0d51f1964b8f24235739f016db68d37b4cfc6c646e3825`
- Two independent clean worktree builds are byte-identical for both FWR and BIN.
- Production public key is embedded and matches the signing identity.
- OTA write support and Boot Guard are enabled.

## Successor candidate: 2.0.18

- OTA sequence: `20018`
- FWR SHA-256: `1af7ad919e51c94d48564680bb3f05a9bd3fd6ab7b077325921758c6a7073b1f`
- BIN SHA-256: `8369d0809be2fd3f689a4b976b186be48d8189b493e6fd19a707e1f33e947f8b`
- Signature SHA-256: `a7ca6d026bdd5cd8e3ab5fc835cc622c3fe736e839a8df473a35f6c3db72878f`
- Production public key is embedded and matches 2.0.17.

## Exact-artifact QA

The production-signed artifacts were used in the Native remote OTA host pipeline:

`2.0.17 -> 2.0.18 -> forced trial failure -> rollback to 2.0.17`

Result:
`SFN_REMOTE_OTA_PIPELINE=PASS partial_download_cuts=8 activation_cut_states=4 rollback=PASS`

Phase 3 server regression remains `552/552 PASS`.

## Safety boundary

This is now ready for one explicitly approved owned physical canary. It is **not** a fleet release yet. Before customer rollout, the owned canary must prove the real physical chain Stock -> 2.0.17 -> 2.0.18 and automatic recovery. New3 was not written, restarted, or flashed during production signing preparation.

Release metadata on the new VPS:
`/srv/sfn-power-research/releases/sfn-native-prod-20261017/release-manifest.json`
