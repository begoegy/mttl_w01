# Voltra 1.0.146 -> SFN Neutral 1.0.147 canary — 2026-10-15

Target: owner canary `88D039FA2729` (`الصاله`), model `lgutap`, source `0.1.55-1.0.146`.

## Source evidence

The public Voltra DNS documented for MTTL-W01 provisioning maps the legacy LG service names to `5.182.18.10`. An unauthenticated GET of `/ota.bin` with the MEF Host header returned a 327,944-byte MTTL-W01 FWR.

Public image SHA-256: `ae997d79f8e54376a4eb94e447cf94582311544aa128ff4a487d2190f67208ad`.
Decoded with the image's 16-byte XOR key `3f5a27e8d8fb85f97c8397d3ad879f97`; additive checksum validates. The image identifies itself as software `0.1.55`, firmware `1.0.146`, hardware `LGU+-TAP-HW002`, model `MTTL-W01`.

The four legacy service slots contain `log.toi.ommeq.com` twice and `dev.toi.ommeq.com` twice.

## Neutral candidate

Builder: `tools/sfn_build_neutral_lab_1_0_146.py`.
Candidate version: `0.1.55-1.0.147`.
SHA-256: `1a3b66c4a43e63aa54615646b50f2c8507c57c3ce604be3f72211616f1379881`.
Size: 327,944 bytes.

Only the four service-host slots, version bytes, and trailing additive checksum are permitted to differ. Controller/provisioning `up:ip`, OTA, relay, meter, Wi-Fi, and executable code are preserved.

## Physical remote result

Job `75996e98f7ad9edb` transferred all `327944/327944` bytes, rebooted, and verified successfully on `0.1.55-1.0.147`.
Post-state: online; outlets 1-4 ON; voltage 204.7 V; RSSI -59 dBm. Exact public 1.0.146 package remained available as the canary rollback image and was not needed.

A legacy-edge routing mismatch was exposed: the live TCP session is seen by the main server as `10.254.254.1`, while the direct HTTP OTA request arrives from the customer's public IP. The one-off canary corrected the lease only after a no-byte probe established that public source. This routing identity issue must be fixed before unattended fleet rollout; it is separate from firmware compatibility.
