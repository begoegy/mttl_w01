# SFN Full Local Engine 1.0.152 R3 — PHYSICAL REJECTED — 2026-10-17

## Status

**PHYSICAL REJECTED. DO NOT FLASH. DO NOT OFFER. DO NOT FLEET-ROLLOUT.**

Owner canary only: `New`, MAC `88D039394D73`.
No customer device was targeted. The production server binary was not changed.

Rejected artifact SHA-256:
`fafa3f7e5a789bbe634e9e45c95ceb92962caad4574105c545e0a42aa1d7d872`

Firmware job: `9a073ea14ec71d22`.

## Physical result

The official firmware manager transferred the exact tested artifact completely:

- `327944 / 327944` bytes;
- progress `100%`;
- OTA manager entered `rebooting`;
- the strip never returned a fresh `bootinfo` / device session on the expected firmware;
- after the full reconnect deadline the job terminated `failed` with `reconnect_timeout`;
- automatic rollback was `blocked_offline`, because the strip was not online to receive a rollback offer.

The active 1.0.152 catalog entry was immediately retired to `firmware/rejected/`. Exact 1.0.150 rollback SHA
`b75e394e77c7ec2e8598bb98d157980bd889cc06989cf8fc9702d6bfc20fba8f` remains registered for the owner canary if it reconnects.

## What the laboratory QA did and did not prove

The R3 exact-byte emulator, branch-target gate, A/B persistence model and independent FWR validator all passed before the physical canary. Those results prove the modeled code paths and package integrity; they did **not** prove that every reclaimed vendor startup byte was actually disposable on hardware.

The physical failure therefore overrides the lab promotion status.

## Leading root-cause class

1.0.152 changed a hardware-startup assumption that 1.0.150 did not change: it reclaimed the vendor `check_ota_ssl_task` body and replaced its creator entry with `bx lr`.

That is now prohibited. Historical physical work on this same device family also showed that the successful neutral-v2 path preserved RTOS task creation rather than disabling vendor task startup.

A new preservation gate now requires candidate firmware to keep exact 1.0.150 bytes over:

`0x10009914 .. 0x10009A36`

The rejected 1.0.152 image fails that gate with 273 changed bytes; exact 1.0.150 passes.

This is the **leading physically-supported root-cause class**, not a claim that every changed byte in that range has individually been proven causal. No new physical candidate may be built by reclaiming this window.

## Recovery policy

Remote rollback is allowed only if New reconnects. Otherwise recovery is physical and read-first:

1. capture UART log / JTAG diagnostics without modifying flash;
2. if SPI access is used, read the complete 1 MiB flash twice and require identical SHA-256 values;
3. analyze the two Image2 slots (`0xB000`, `0x80000`) with `tools/sfn_analyze_full_flash_recovery.py`;
4. do not erase/write bootloader, factory/settings or either image slot until the dump proves the exact surviving 1.0.150 bytes and the minimum recovery mutation is independently reviewed.

The analyzer is intentionally read-only and contains no programmer/write/erase implementation.

## Evidence

- `docs/sfn-research/evidence/full-local-engine-1.0.152-physical-20261017/job-final.txt`
- `docs/sfn-research/evidence/full-local-engine-1.0.152-physical-20261017/job-events.txt`
- `docs/sfn-research/evidence/full-local-engine-1.0.152-physical-20261017/task-gate-good.txt`
- `docs/sfn-research/evidence/full-local-engine-1.0.152-physical-20261017/task-gate-rejected-152.txt`
- `docs/sfn-research/evidence/full-local-engine-1.0.152-physical-20261017/recovery-analyzer-test.txt`
