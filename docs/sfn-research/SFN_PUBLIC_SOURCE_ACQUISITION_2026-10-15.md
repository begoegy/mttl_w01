# SFN public firmware source acquisition — 2026-10-15

Goal: obtain an exact original MTTL-W01 `0.1.50-1.0.60` FWR without touching an unowned device.

## Result so far

No exact `1.0.60` FWR was found in the searched public GitHub repositories, their relevant history, the authorized SFN server/archive, or the currently reachable vendor paths tried. Guessed public vendor paths for `1.0.60` returned 404 and were not treated as evidence that the image never existed.

The public Voltra source endpoint for `MTTL-W01_V1.0.105.fwr` is still reachable. The downloaded file was accepted only after its size and published SHA-256 were verified:

- size: `327944`
- SHA-256: `f78aee903d392f2b8470c845d9e76c33d91fdcf1da57e6c7e714cbedc9edaefa`
- reported firmware: `0.1.54-1.0.105`

Running the SFN firmware-family analyzer over that exact public source classified it as `mttl-w01-hw002-r1` with critical fingerprint:

`5d5464470f0b6e9474fbdc34da0614b84a88caa1ae51e2d49b7a6eff7a74abfc`

This is the same critical fingerprint already observed on exact original `1.0.66` and exact original `1.0.146`. The generic neutral builder also accepted the source and produced a source-derived `0.1.54-1.0.106` candidate without touching any physical strip.

## Meaning

There are now three widely separated original source versions (`1.0.66`, `1.0.105`, `1.0.146`) with the same version-independent critical firmware-family fingerprint. This materially strengthens the family-based approach and the hypothesis that application version numbers alone should not be the compatibility gate.

It still does not justify fabricating `1.0.60` bytes or using another version as the rollback for a live `1.0.60` strip. The exact source bytes are still required once for an exact rollback/source-derived Neutral package.

## Safe next sources

1. Retry public archives/mirrors for the historical exact `1.0.60` image when available.
2. Ask a public project/vendor maintainer for an archived original `1.0.60` FWR.
3. If neither exists, dump a user-owned laboratory unit that is actually on `1.0.60` using a physical hardware recovery interface, then verify format/checksum/family before any write operation.

No customer device was flashed, dumped, or reconfigured during this source-acquisition work.
