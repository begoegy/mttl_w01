# Vendor child-lock semantics — MTTL-W01 / 0.1.55 lineage — 2026-10-17

Status: STATIC + EXACT-BYTE EMULATOR PROOF PASS. No device I/O, OTA or flash was performed.

The exact vendor-derived 0.1.55 image exposes `childlock` and acknowledgement `up:cl:%d`. The parser accepts decimal masks 0..31 and persists a four-byte `CL` record at vendor key/address `0x900`: magic `CL`, one mask byte, and its bitwise inverse. Invalid/uninitialized persistent state is reported with the vendor sentinel before a confirmed reset path is used by the server.

The vendor callback applicator at `0x10026114` consumes exactly five bits. For each bit, a set bit removes the corresponding physical-button callback; a clear bit restores the vendor callback. The exact-byte Unicorn proof exercises masks 0, 1, 2, 4, 8, 16 and 31 against the vendor callback table at `0x100541E0`.

Mapping:

- bit 0 / value 1: master physical button;
- bit 1 / value 2: outlet 1 physical button;
- bit 2 / value 4: outlet 2 physical button;
- bit 3 / value 8: outlet 3 physical button;
- bit 4 / value 16: outlet 4 physical button.

Therefore mask `0` enables every physical button and mask `31` disables every physical button. Slots 1..4 use the vendor per-outlet callback and pass their slot number as the device/outlet index; slot 0 uses the distinct master callback.

This child lock disables local physical-button callbacks only. It does not remove authenticated server/app control. SFN must preserve the vendor implementation rather than replace it.

Evidence:

- `docs/sfn-research/evidence/full-local-engine-1.0.151-20261017/childlock_semantics_proof.py`
- `docs/sfn-research/evidence/full-local-engine-1.0.151-20261017/childlock-semantics.txt`
