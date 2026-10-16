# SFN Local Queue 1.0.79 — New physical canary

Date: 2026-10-16
Target: `New` / `88D039394D73` only

## Result

`CANARY_BASE_AND_QUEUE_STATUS_PASS`

The exact 1.0.79 candidate was installed on New through the guarded firmware update manager after 1.0.78 had been rejected and rolled back due to the real-wire CRLF parser contract.

## Artifact

- source baseline: `0.1.54-1.0.71`
- target: `0.1.54-1.0.79`
- file: `/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.79-local-queue-crlf-new-lab.fwr`
- SHA-256: `f7dcc62cdab631ac92d55db31a22dcb56f7b6475f75b5e33f11adba0941a97ca`
- size: `327944`
- MAC restriction: `88D039394D73`
- rollback: exact proven `0.1.54-1.0.71`, available

## Server bridge QA

The read-only Local Queue status bridge was regression-tested before deployment: `501/501 PASS`, plus package selftest PASS. Deployed pyz SHA-256: `249af876eb90f7b6e1c142a2a26b357583f173da9f5de1bffaaf72887946496f`.

## Physical OTA

Firmware job: `9f2026b4f81617b7`

- status: `succeeded`
- phase: `verified`
- bytes: `327944 / 327944`
- rollback status: `available`
- device reconnected reporting `0.1.54-1.0.79`
- no crash/watchdog/error found for New after reconnect
- all four outlets were ON before OTA and remained ON after verification

## First real queue probe

Only the queue status operation was sent. No queue generation was committed and no outlet-changing event was scheduled.

Exact physical reply decoded by the server:

`code=4 generation=0 count=0 meta=0`

Server result:

`{"ok":true,"code":"ok","mac":"88D039394D73","generation":0,"count":0,"meta":0}`

This proves on real New that the 1.0.79 parser accepts the real server CRLF-framed status command, runtime queue initialization succeeds, the stock timer object can be created, and the ACK returns over the real socket path.

## Memory

Before OTA on 1.0.71:

- current free: `34728`
- minimum free: `30960`

After 1.0.79 status initialization:

- current free: `34416`
- minimum free: `30960`

Observed current-free delta: `312` bytes, consistent with the intentionally small runtime queue/timer allocation plus allocator overhead. This is observational evidence only; it is not a general heap-fragmentation proof.

## Stop gate

No real scheduled ON/OFF event has been armed yet. The next physical gate is a deliberately selected short event on one outlet, with the initial state captured and a rollback/recovery plan. That step changes a relay and therefore remains a separate approval gate.
