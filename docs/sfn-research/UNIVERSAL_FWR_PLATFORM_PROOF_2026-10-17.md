# MTTL-W01 universal FWR platform proof — 2026-10-17

Status: static/platform proof only. **No OTA, flash, catalog registration, or customer-device change was performed.**

## Question

Do the observed MTTL-W01 firmware versions represent incompatible flashing platforms, or the same hardware/boot image format with different software contents?

## Evidence set

Local exact vendor/public images:
- stock `0.1.54-1.0.66`, SHA-256 `d780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04`.
- Voltra public `0.1.55-1.0.146`, SHA-256 `ae997d79f8e54376a4eb94e447cf94582311544aa128ff4a487d2190f67208ad`.

Independent public GitHub project `af950833/mttl_w01_standalone`:
- `2.0.0`, SHA-256 `ef41bb2e0db562ab43bc35838907a8890f17306ac0de2e12aefbaac3b284a66f`.
- `2.0.11`, SHA-256 `085ed5ecdd00abd31ff95c24a7bf60c90a5add05d9c7aa4eec7cc5174596c957`.
- `2.0.14`, SHA-256 `d156330522b931bfd8c3efbc2a71ba5e6cfb759bdd2e462a7154fd4e67d36fb1`.

Public source reference: `https://github.com/af950833/mttl_w01_standalone`. Its published `source/standalone/ota.py` derives the FWR XOR key from the first 16-byte header rather than using a firmware-version-specific key.

## Result

All five images decode as the same MTTL-W01 Image2/FWR platform signature:
- load address `0x10006000`;
- Thumb entry `0x10006071`;
- identical RTK start-table tail `52544b57696e00ff000101009581010100000000`;
- hardware marker `LGU+-TAP-HW002` at decoded offset `0x28`;
- model marker `MTTL-W01` present;
- decoded length relation is valid;
- additive image checksum is valid.

The fixed-key observation between 1.0.66 and 1.0.146 was therefore not evidence of separate flashing families: both keys are instances of the same header-derived transform.

## Important boundary

This proves a common **hardware/boot image format and flashing envelope**, not byte-identical application firmware. Stock 1.0.66 and 1.0.146 differ in 662 decoded bytes (~0.2%), including real feature/code differences such as `childlock` in 1.0.146. The independent 2.x firmware is substantially rewritten while still targeting the same platform.

Therefore the preferred engineering direction is now a **single universal SFN target firmware for the proved MTTL-W01 / LGU+-TAP-HW002 platform**, built from a modern compatible base and preserving required newer functions, instead of producing one target image per source version by default.

Before physical rollout, remaining gates are:
1. prove OTA acceptance/boot compatibility from each source tuple we intend to support;
2. preserve source-version features that matter (for example child lock semantics where applicable);
3. have exact rollback bytes for each source tuple or an independently proved common recovery path;
4. one owned-device canary with explicit user approval before any physical OTA;
5. never infer compatibility for a different hardware marker/model.

This document does **not** authorize OTA to any customer device.
