# SFN Native Firmware laboratory base

This directory is an isolated firmware-development path for MTTL-W01.

Upstream base: `af950833/mttl_w01_standalone` at commit
`5879da89e653682645147227d300344e72325782`.

Rules:
- LAB ONLY until explicit physical approval.
- Never OTA customer devices from this branch.
- New2 (`88D03907723D`) is reference-only and must not be flashed from this branch.
- Physical release requires a proven recovery path and a dedicated sacrificial canary.
- Successful compilation/emulation is not hardware approval.

The imported project-authored upstream code is MIT licensed; retained license files are under `native/upstream/`.
