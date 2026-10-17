# SFN Full Local Engine 1.0.151 — Child Lock phase 2

Status: **STATIC + exact-machine-code emulator PASS. No device I/O / no OTA.**

Exact target: `comMTTL-W01_1.0.151-full-local-engine-universal-r4-a.fwr`, SHA-256 `44e46fa430d99492e37ca0ad50fdec3d8eefb7a8d4ac8abf5f8344fc05f4c36b`.

The final 1.0.151 image preserves the vendor Child Lock parser, response strings, boot-load path, persistent setter/getter and reset path byte-for-byte from the pinned 1.0.147 vendor lineage. The record uses vendor `CL` format at key/offset `0x900`; the set command accepts masks `0..31` and the factory/reset clear path writes mask `0`.

Exact Unicorn execution of the preserved vendor apply function proves the mask semantics:

- `0`: Master + Outlet 1..4 physical-button callbacks all active (unlocked).
- `1`: Master locked only.
- `2`: Outlet 1 locked only.
- `4`: Outlet 2 locked only.
- `8`: Outlet 3 locked only.
- `16`: Outlet 4 locked only.
- `31`: all five physical buttons locked.

A set bit removes the corresponding physical-button callback. Remote/app relay control is a separate path, so Child Lock does not disable authenticated app/server switching. The common 1.0.151 voltage-safety gate still applies to ON commands independently.

Independent public MTTL-W01 work retained in the research archive reports the same wire contract (`up:childlock[:mask]` / `up:cl:<mask>`), the same five-bit Master+Outlet mapping, the same `CL` persistent record, factory clear to zero, and a hardware round-trip `0 -> 2 -> 0`. That corroborates the exact local binary proof.

Evidence: `docs/sfn-research/evidence/full-local-engine-1.0.151-childlock-20261017/childlock_semantics_gate.py` and `childlock-semantics.txt`.

Next gate is server/API/UI QA and safe deployment of the exact tested server artifact. Physical 1.0.151 OTA remains separately approval-gated.
