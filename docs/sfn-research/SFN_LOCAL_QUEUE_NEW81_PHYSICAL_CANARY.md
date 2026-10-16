# SFN Local Queue 1.0.81 — physical New canary

Date: 2026-10-16
Device: `New`
MAC: `88D039394D73`
Firmware artifact: `comMTTL-W01_1.0.81-local-queue-r8fix-new-lab.fwr`
Firmware SHA256: `5b96489849e8808512a0e3f918013102ced8cb5610f939ae49da60cc2220ff0a`
Firmware research commit: `5b25182`
Server branch commit with 1.0.81 sync gate: `72d5e37`
Server diagnostic commit used for zero-load selection: `bffcd08`
Deployed server artifact SHA256: `f192de16ed6850804dd0ae7808099cc81f59cdc5f0f89e25febeb2d35210a9ca`

## OTA gate

Source firmware: `0.1.54-1.0.80`
Target firmware: `0.1.54-1.0.81`
Job: `cbaeb195369bf602`
Transfer: 327944 / 327944
Result: succeeded / verified
Registered automatic rollback before start: exact `0.1.54-1.0.80`, SHA256 `385192607c2b6f92221413bf6692087e205bb73258e3ec033fb8aaa48c0a8c86`

Post-boot:
- all four outlets ON
- voltage/RSSI normal
- queue status generation 0/count 0/meta 0
- heap current 34416 bytes
- heap minimum initially 30960 bytes

## Physical test 1 — safe ON event

Precondition: outlet 1 confirmed ON.
Queue generation 1:
- slot 0: +10 seconds, outlet 1 ON

Wire acknowledgements observed:
- Begin code 1
- Item code 2
- Commit code 3, generation 1/count 1/meta 0

After due time:
- status generation 1/count 1/meta 1
- low-byte terminal index = 1
- high-byte error = 0
- outlet 1 remained ON
- all four outlets remained ON
- heap current 34416/minimum 30960

The queue was then replaced by empty generation 2 and status returned count 0/meta 0.

## Physical test 2 — real relay OFF then ON

Three fresh power samples were taken before selecting an outlet. Outlets 1 and 4 were 0.00 W in all three samples. Outlet 4 was selected.

Queue generation 3:
- slot 0: +5 seconds, outlet 4 OFF
- slot 1: +10 seconds, outlet 4 ON

Midpoint after first event:
- generation 3/count 2/meta 1
- outlet state: 1 ON, 2 ON, 3 ON, 4 OFF

After second event:
- generation 3/count 2/meta 2
- outlet state: all four ON
- high-byte error remained 0
- heap current 34416
- heap minimum 30312

The queue was then replaced by empty generation 4.

## Physical test 3 — true server/network-loss execution

Purpose: prove the first relay event executes while the device cannot exchange any strip-protocol traffic with the SFN server.

Safety precondition:
- outlet 4 freshly ON
- outlet 4 power 0.00 W
- only one established direct strip-service connection existed from public IP `41.33.44.154` on port 10086
- current iptables configuration was backed up before isolation

Queue generation 5:
- slot 0: +5 seconds, outlet 4 OFF
- slot 1: +30 seconds, outlet 4 ON

Immediately after commit, New was isolated for 9 seconds in both directions only on the strip service:
- INPUT drop: source `41.33.44.154`, TCP destination port 10086
- OUTPUT drop: destination `41.33.44.154`, TCP source port 10086

No other device address or port was blocked.

Firewall backup:
`/srv/sfn-power/backups/SFN_NEW_OFFLINE_1792183514/iptables.before`

At approximately +18.22 seconds, after network restoration and before the +30 second ON event:
- first fresh report showed outlet 4 OFF
- other outlets remained ON
- status at +18.74 seconds was generation 5/count 2/meta 1

This is direct physical evidence that the +5 second OFF event executed while the server link was unavailable. The server could not have sent an ON/OFF command during the isolated interval.

At approximately +33.87 seconds:
- status generation 5/count 2/meta 2
- outlet 4 was ON again
- all outlets ON
- high-byte error 0
- heap current 34416/minimum 30312

The queue was then replaced by empty generation 6.

## Cleanup and final state

- both temporary firewall rules removed and verified absent
- `sfn-power.service` active
- no active firmware jobs
- New online on `0.1.54-1.0.81`
- all four outlets ON
- queue generation 6/count 0/meta 0
- heap current 34416/minimum 30312

## Gate result

`LOCAL_QUEUE_NETWORK_OFFLINE_PHYSICAL_PASS`

The following are now physically proved on New:
- staged Begin/Item/Commit with per-stage acknowledgements
- one-event local timer execution
- real OFF and ON relay transitions
- multi-event ordering
- execution while the SFN server/network path is unavailable
- queue cleanup/replacement
- stable telemetry and heap after the tests

Still NOT proved and not claimed:
- survival of a total strip power cut; the queue is RAM-only
- durable offline scheduling without internet after a full power loss
- fleet compatibility across other firmware families
- Child Lock or Voltage Guard in this firmware
