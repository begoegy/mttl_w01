# SFN Neutral 1.0.148 local-cycle lab candidate

Date: 2026-10-15
Status: **LAB ONLY — NOT PHYSICALLY FLASHED, NOT PRODUCTION APPROVED**

## Purpose

Support the router-on-an-outlet case without depending on Internet after the OFF command. The controller hands the complete restart to the strip before the router is powered down.

Wire contract:

- request: `up:cycle:<channel>:<seconds>`
- channel: `1..4`
- delay: `5..900` seconds
- acknowledgement: `up:cycle:<channel>:armed:<seconds>`

The acknowledgement is written to the existing device TCP connection before the local task reaches the relay OFF operation. The task then waits on the strip itself and turns the same outlet ON without any server command.

## Exact source gate

Builder: `tools/sfn_build_neutral_cycle_lab_1_0_147.py`

Accepted source only:

- proven SFN Neutral 1.0.147 artifact
- size: 327944 bytes
- SHA-256: `1a3b66c4a43e63aa54615646b50f2c8507c57c3ce604be3f72211616f1379881`
- decoded checksum valid
- exact version marker `1.0.147`
- exactly four neutral `127.0.0.1` host replacements
- expected HW/protocol markers present

Any SHA/layout/marker mismatch is refused before output is written.

## Candidate output

Artifact is kept outside the repository at:

`/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.148-neutral-cycle-lab.fwr`

Static result:

- size: 327944 bytes
- SHA-256: `67686ef26ffc42da9c6e8be5c325d0c34a5b5b7014bb4d2810864ffa9c3c7765`
- decoded checksum: `01f86955`
- decoded changed bytes: 234
- reported application version: `1.0.148`
- four neutral host replacements preserved
- existing `up:ip`, `up:connect`, `up:ota`, and `up:reboot` markers preserved

The build is deterministic: two independent builds from the pinned source were byte-for-byte identical. A one-byte-mutated source was rejected by the SHA gate.

## Implementation

The stock manufacturing `test` command path is replaced with `cycle`; normal outlet control, telemetry and setup commands are not repurposed. The exact source image is pinned because executable addresses are source-build specific.

The cycle handler:

1. parses and bounds-checks outlet and delay;
2. creates a low-priority local FreeRTOS task;
3. only if task creation succeeds, formats and sends the `armed` acknowledgement;
4. the local task waits briefly so the ACK leaves the strip first;
5. requests relay OFF using the existing relay dispatcher;
6. waits `seconds * 1000` locally;
7. requests relay ON using the same dispatcher;
8. deletes itself.

The code uses the already-present stock task, delay, send and relay paths; it does not add a second network stack.

## Important limitation

This first candidate keeps the countdown in **device RAM**. That is enough for the target router-restart scenario because the strip itself remains powered while the router outlet is OFF. Loss of Internet does not cancel the countdown.

It does **not** claim that an in-progress cycle survives loss of power to the strip itself. Flash-persisted recovery must be a separate change after the basic local cycle is physically proven. Do not describe 1.0.148 as a persistent wall-clock scheduler.

## Physical canary gate before any production use

Use only an owned/authorized canary. Required evidence:

1. OTA transfer, reboot and exact `0.1.55-1.0.148` reconnect.
2. All four outlets and telemetry remain normal before testing cycle.
3. Cycle a non-critical test load first at 5 s and verify ACK precedes OFF.
4. Verify OFF duration and autonomous ON with the server connection deliberately unavailable.
5. Test 30 s on a router outlet and verify the strip reconnects after the router returns.
6. Verify invalid channels/durations do nothing.
7. Verify repeated/overlapping cycle commands are handled safely before exposing the UI broadly.
8. Cold power-cycle and confirm normal startup/outlet behavior.
9. Keep exact 1.0.147 rollback artifact ready.

No customer/fleet flash is authorized by this document.

## Executable payload emulation

`tools/sfn_cycle_payload_emulate.py` executes the injected Thumb handler/task with Unicorn while stubbing only the reviewed stock API calls. Current checks pass for:

- valid `cycle:2:30` -> task parameter + exact armed ACK;
- invalid channel/delay/missing argument -> no task and no ACK;
- task-create failure -> no ACK;
- task sequence -> 100 ms guard, relay OFF, 30 s local delay, relay ON, self-delete.

This strengthens pre-flash QA but is not a substitute for a physical canary.
