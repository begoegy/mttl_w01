# SFN Neutral Firmware / Recovery Plan

Date: 2026-10-15
Status: research/design only — no fleet flash authorized by this document.

## Key finding

The safest first-generation "neutral" path does **not** require inventing a new firmware image.
Stock MTTL-W01 firmware already exposes a physical setup service at `192.168.1.1:30300` and accepts:

- `up:ip:<SERVER_IPV4>`
- `up:connect:<SSID>:<PASSWORD>`
- `up:reboot:0`

The strip then dials TCP port `10086` on the provisioned server IP. SFN Power already implements this provisioning flow in production source.

This is materially better than a Direct-local binary patch that hardcodes one server IP into the firmware: a hardcoded IP requires another firmware build/flash when the customer changes server, while `up:ip` allows the service technician to repoint the same stock firmware without modifying the image.

## Current proven baseline

Pinned stock image:

- model marker: `MTTL-W01` / `LGU+-TAP-HW002`
- firmware: `0.1.54-1.0.66`
- size: `327944` bytes
- SHA-256: `d780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04`

Physical SFN canary already proved `0.1.32-1.0.38 -> 0.1.54-1.0.66`, full transfer, reboot, reconnect, four outlets, voltage, and numeric RSSI.

This proves one upgrade path only. It does **not** prove every hardware revision or every later firmware can safely downgrade to 1.0.66.

## Recommended product direction

### Phase A — Neutral recovery baseline

Use the exact pinned stock 1.0.66 image only for hardware/firmware combinations that pass compatibility testing.
After flash, put the strip in setup mode and provision the customer's chosen server IP with `up:ip`.

For SFN customers that server is the SFN Power device endpoint. If the owner later moves to another compatible service, the technician can put the strip back in setup mode and provision the new server IP without reflashing.

### Phase B — Compatibility matrix

Test one physical canary from each source family before enabling it in the manager. Current fleet examples include:

- 1.0.38 — proven to 1.0.66
- 1.0.58 — unproven
- 1.0.60 — unproven
- 1.0.62 — unproven
- 1.0.66 — already baseline
- 1.0.111 / 1.0.114 / 1.0.116 / 1.0.141 / 1.0.145 / 1.0.146 — do not downgrade until separately proven

Each canary must verify: setup mode, OTA/recovery path, reboot, server reconnect, all four relays, voltage/current/energy where available, RSSI, Wi-Fi reprovisioning, cold power-cycle recovery.

### Phase C — Locked-device intake

For a customer device reported as locked to another service:

1. Record exact model, firmware and hardware-visible identity before writing anything.
2. Test whether the physical setup AP still appears and whether TCP `30300` accepts `up:ip`.
3. If server repointing alone works, do **not** flash: repoint and verify. This is the lowest-risk repair.
4. If repointing is blocked but an approved OTA path exists and the source family is in the compatibility matrix, flash the approved recovery image and then provision the chosen server.
5. If both setup and software OTA are blocked, classify as hardware-recovery-required; do not improvise a fleet downgrade.

## What not to do

- Do not treat 1.0.66 as universal merely because it worked on one old unit.
- Do not downgrade later versions automatically.
- Do not patch arbitrary firmware offsets unless the exact source SHA and image layout are pinned.
- Do not call a firmware "signed" unless a real cryptographic signature is verified; the known stock file is currently pinned by SHA-256 and internal checksum, not by a verified manufacturer signature.
- Do not hardcode SFN's IP into a supposed neutral firmware if the existing setup protocol can keep server selection separate from firmware.

## Static cloud-path result

Read-only analysis of the exact pinned 1.0.66 image found independent vendor-network code in addition to the technician-configurable controller path. The decoded image contains and actively references:

- `mef.onem2m.uplus.co.kr` — MEF / certificate / OTA flow;
- `brk2.onem2m.uplus.co.kr` — MQTT flow, with the stock `18831` port visible at the connection site;
- `hdslog.lguplus.co.kr` — QMS / diagnostic HTTP/TLS POST flow.

These are not merely orphaned printable strings: code references were found for the MEF and MQTT hosts, and the QMS host is embedded beside the diagnostic POST construction. Public Direct-local 1.0.68 tooling independently patches these same hosts and service ports away from the vendor endpoints.

What this proves: stock 1.0.66 contains separate vendor-cloud-capable networking paths even though `up:ip` can point the normal controller session at any IPv4 address.

What this does **not** yet prove: whether every one of those vendor paths actually executes after a strip is manually provisioned with `up:ip`. Therefore stock 1.0.66 must not be labelled fully neutral/cloud-free yet.

## Remaining proof before calling 1.0.66 fully neutral

A lab packet capture is still required on a freshly provisioned 1.0.66 unit:

- capture from cold boot through at least one normal control session;
- record DNS, TCP and TLS destinations;
- confirm whether any manufacturer/third-party endpoint is contacted after `up:ip` provisioning;
- repeat after another cold power cycle.

If those vendor paths are active, the neutral image must remove or safely isolate them while preserving the technician-controlled `up:ip` path. Do not solve this by hardcoding SFN instead.

Until that capture is complete, call 1.0.66 the **candidate neutral recovery baseline**, not a fully cloud-free firmware.

## Custom SFN firmware later

A custom SFN firmware remains a possible second-generation path, but not the first choice today. Public standalone firmware demonstrates that a complete independent implementation is possible, but its repository currently does not publish the full firmware build SDK/source and explicitly warns that all hardware variants are not guaranteed.

If SFN later builds its own image, requirements are:

- server profile stored as configuration, not compiled-in vendor lock-in;
- technician-authorized reprovisioning;
- fail-safe OTA / recovery path;
- watchdog and cold-boot recovery;
- hardware compatibility gates;
- signed release artifacts under SFN-controlled keys;
- no automatic third-party firmware channel.

## Decision

For the first service version, pursue **Recovery + Reprovision**, not a new binary firmware from scratch:

`inspect -> compatibility gate -> recover to approved baseline only if needed -> provision chosen server -> full QA -> hand back to owner`.

This gives the customer portability without replacing one vendor lock-in with an SFN lock-in.
