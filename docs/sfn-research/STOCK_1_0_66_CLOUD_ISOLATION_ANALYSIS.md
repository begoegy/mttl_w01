# SFN research: stock 1.0.66 cloud-isolation analysis

Date: 2026-10-15
Status: static/read-only analysis only. No firmware bytes in a physical device were changed by this analysis.

## Image identity

Exact input: `work/firmware/1.0.66/comMTTL-W01_1.0.66.fwr`

- size: `327944` bytes
- SHA-256: `d780b578af69d52f3a05191a8e7d91a20e05085a912722327481cd5663682c04`
- model markers: `MTTL-W01`, `LGU+-TAP-HW002`
- firmware marker: `0.1.54-1.0.66`

All offsets below refer only to this exact pinned image.

## Direct controller path that must be preserved

The stock setup service accepts a technician-selected controller IPv4 with `up:ip:<IPv4>` and the tested firmware then dials the selected controller on TCP `10086`.

SFN Power has also physically proved remote `up:ota:<IPv4>` over the normal controller session: the canary fetched `/ota.bin`, received the full `327944`-byte pinned 1.0.66 package, rebooted and reconnected successfully. Therefore a neutral build does not need the manufacturer MEF channel in order to retain an SFN-managed recovery/update path.

## Vendor-network paths present in stock 1.0.66

Decoded strings and code references show a second, independent vendor cloud stack in the same image:

- `mef.onem2m.uplus.co.kr` — certificate / MEF enrollment / OTA version-check and download
- `brk2.onem2m.uplus.co.kr` — TLS MQTT / oneM2M
- `hdslog.lguplus.co.kr` — QMS diagnostic upload

Related decoded strings include:

- `LGtonlyoneM2MTask`
- `mqtt_connect start`
- `/oneM2M/req/...` and `/oneM2M/resp/...`
- `/mef/updateVersionCheck/firmware`
- `check_ota_ssl_task`
- `ota_task`
- `ota_ssl_task`
- `QMSlog_timer`
- HTTP POST construction for the QMS host

This matches the public local-backend project, whose stock-mode design redirects the original LG U+ destinations with router DNAT and whose Direct-local 1.0.68 patch replaces these hostnames and service ports.

## Static code-reference evidence

### MQTT / oneM2M

`brk2.onem2m.uplus.co.kr` is referenced from the decoded code near:

- file offset `0x18742`
- virtual address `0x1001e732`

The same connection block loads stock port `0x498f` = `18831` before calling the network/MQTT routine. This is executable code, not just an unused printable string.

### MEF

`mef.onem2m.uplus.co.kr` has decoded code references near:

- file offset `0x1cfe2`
- file offset `0x1d25e`
- file offset `0x1d362`

A connection block in this region uses TCP `443`. The stock image also contains the MEF firmware-version-check path and certificate paths.

### oneM2M task creation

A wrapper at virtual address `0x10023686` prepares an RTOS task with a `0x1400` stack and a PC-relative name that resolves to `LGtonlyoneM2MTask`, then calls the task-create routine at `0x100236b2`.

That wrapper is called from `0x100098f4`; the direct call to the wrapper is at `0x10009900`. The surrounding startup logic reaches this helper from two branches (`0x10009a04` and `0x10009b00`). This is strong static evidence that the legacy oneM2M/MQTT stack is intentionally started by normal firmware startup logic.

### QMS

The decoded image contains `hdslog.lguplus.co.kr` at the QMS diagnostic HTTP/TLS construction together with `POST`, `Host`, content headers, `/read_iot_wifi`, and success/failure logging. The public Direct-local patch replaces both QMS host occurrences and its service port.

## What is now proven

Stock 1.0.66 is **portable at the controller layer** because `up:ip` can point the normal TCP controller session to a technician-selected IPv4.

Stock 1.0.66 is **not suitable to label cloud-free/neutral as-is** because the exact image also contains active legacy LG U+ oneM2M/MEF/MQTT/QMS networking code and an OTA-check path.

A future vendor OTA path is especially relevant to SFN's anti-lock-in goal: a truly neutral service image must not leave a third party with an independent firmware-delivery channel.

## Preferred neutral-v1 design

Do not replace LG hardcoded endpoints with an SFN hardcoded endpoint. That would only replace one lock-in with another.

Preferred minimal patch from the pinned 1.0.66 base:

1. Keep setup AP and `up:ip`, `up:connect`, `up:reboot`.
2. Keep normal controller TCP `10086` and its relay/telemetry/RSSI protocol.
3. Keep the already-proven direct `up:ota` recovery/update mechanism.
4. Disable legacy manufacturer MEF / oneM2M MQTT / QMS networking and manufacturer OTA checks.
5. Do not change relay, metering, protection or Wi-Fi driver logic.
6. Give the neutral image its own distinct version identity only after the patch layout and rollback path are verified.

The preferred implementation is to prevent the legacy cloud tasks from starting, rather than merely pointing their hostnames at a dead address and leaving permanent retry loops. The task-creation map below now identifies the minimal research patch points for the exact pinned image.

## Remaining static gate

Before any physical canary:

- build the lab candidate only from the exact pinned SHA;
- verify every expected instruction before changing it;
- recalculate and verify the existing image checksum;
- prove the encoded output differs only at the three approved task-create instructions plus the checksum field;
- re-run marker validation for setup, controller, relay, RSSI and direct OTA paths.

Only after those checks should a single physical lab canary be considered.

## Physical canary acceptance gate

A neutral candidate must pass all of the following before any customer intake use:

- cold boot with no vendor DNS/TCP/TLS destinations observed;
- connect only to the technician-configured controller for normal control;
- all four relays and physical buttons work;
- power/energy/voltage/temperature fields remain sane where supported;
- RSSI works;
- Wi-Fi reprovisioning still works;
- remote neutral -> approved recovery OTA works;
- recovery -> neutral OTA works;
- interrupted OTA does not brick the strip;
- repeated cold power cycles recover normally.

Until these gates pass, `1.0.66` remains a **candidate recovery baseline**, not SFN Neutral Firmware.

## Task-creation map completed

The firmware's RTOS task-create routine is at virtual address `0x10010068`. All direct call sites were enumerated before selecting any patch point.

Fixed-name system/device tasks that must remain untouched include `IDLE`, `Tmr Svc`, `tonly_meter_i2c_task`, `tonly_wifi_task_thread`, `tonly_bsp_tap_task`, and `LOGUART_TASK`. Two additional call sites are generic task-creation wrappers that receive the task function/name from callers, so they are also left untouched.

The vendor-network-specific creation paths are isolated to these exact sites in the pinned 1.0.66 image:

| Service path | Initializer / caller evidence | xTaskCreate BL file offset | Expected decoded bytes |
| --- | --- | ---: | --- |
| `check_ota_ssl_task` | startup calls initializer `0x10009a12` from `0x10009afa` | `0x3a3e` | `06 f0 1b fb` |
| `ota_task` + `ota_ssl_task` | startup calls `ota_task` initializer `0x1001f5fc` from `0x10009aa6`; `ota_ssl_task` initializer `0x1001f77c` is called from `0x100099fe`; both converge on the same task-create call | `0x197a4` | `f0 f7 68 fc` |
| `LGtonlyoneM2MTask` | startup calls wrapper `0x10023686` from `0x10009900` | `0x1d6c2` | `ec f7 d9 fc` |

Inside the OTA/QMS task state machine, `QMSlog_timer` is created by the `ota_task` entry and the QMS uploader is called from that same state machine. This supports disabling the task creation rather than patching the QMS hostname into a dead destination.

### Minimal lab patch candidate

For a research-only neutral-v1 candidate, replace only the three decoded 32-bit BL instructions above with two Thumb NOPs (`00 bf 00 bf`) at each site, then recalculate the image's existing additive checksum and re-encode the FWR container.

This approach intentionally leaves all host strings and dormant legacy code in the image. The security/neutrality claim therefore depends on proving those tasks cannot start through another path. Static call-site enumeration found no second direct task-create site for the named vendor tasks, but a physical packet capture is still mandatory before calling the result cloud-free.

No physical device is authorized by this document to receive that candidate.

## Offline lab candidate built and statically verified

The research builder is `tools/sfn_build_neutral_lab_1_0_66.py`. It refuses any source whose size, SHA-256, internal checksum, preserved protocol markers, or expected patch instruction bytes do not exactly match the pinned stock image. Building also requires the explicit `--ack-lab-only` flag.

Research output (not in the production catalog):

- path: `/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.69-neutral-lab.fwr`
- size: `327944` bytes
- SHA-256: `a1f7db34b32e171b529029276e8aa7488cf9307e5567da48c3d7af5bac871cb3`
- decoded additive checksum: `0x01f89f3b`

The lab image reports firmware `1.0.69` instead of `1.0.66`. This is a lab-only identity marker, not a production release-number commitment. The public Direct-local patch already demonstrates that the six-byte version field at `0x18e64` can be changed while preserving the image layout.

Decoded byte comparison against the pinned stock image found changes only at:

- `0x3a3e..0x3a41` — four bytes, `check_ota_ssl_task` task-create BL -> two NOPs
- `0x18e69` — one byte, final version digit `6` -> `9` (`1.0.66` -> `1.0.69`)
- `0x197a4..0x197a7` — four bytes, shared `ota_task` / `ota_ssl_task` task-create BL -> two NOPs
- `0x1d6c2..0x1d6c5` — four bytes, `LGtonlyoneM2MTask` task-create BL -> two NOPs
- checksum bytes `0x50104..0x50105` — the upper two checksum bytes are unchanged, so only two checksum bytes differ physically

The three patch locations disassemble as exactly `nop; nop` in the candidate. Direct calls to the RTOS task-create routine fall from 11 to 8; the remaining fixed-name tasks are normal RTOS/device tasks (`IDLE`, timer service, metering, Wi-Fi, BSP and log UART) plus two generic wrappers, and were not modified.

The setup/controller markers `up:ip:ip_ok`, `up:connect:connect_ok`, `up:ota:ota_ok`, `up:reboot_ok`, `wifirssi`, `up:query:%d`, `MTTL-W01`, and `LGU+-TAP-HW002` remain byte-for-byte present. The stock version marker is intentionally replaced with the lab identity `1.0.69`.

The vendor task entry pointers were also checked in the pinned stock image. Each of the four relevant task entry pointers (`check_ota_ssl_task`, `ota_task`, `ota_ssl_task`, `LGtonlyoneM2MTask`) has one literal pointer occurrence used by its mapped initializer. This further reduces the chance of an undiscovered second direct creation path, but it is still not a substitute for a physical network capture.

Focused builder tests pass: exact stock input produces only the approved diff set, and a one-byte-mutated source is refused. The unrelated legacy HA MQTT test cannot currently import because the research environment lacks the optional `paho` dependency; no environment packages were installed or changed for this firmware research.

**Status remains LAB ONLY. No device has received this candidate.**

## Physical canary result — 2026-10-15

The first remote runtime attempt to install the restricted neutral lab image on canary `88D039394D73` did **not** complete. Source was confirmed `0.1.54-1.0.66`; target was lab identity `0.1.54-1.0.69`, SHA-256 `a1f7db34b32e171b529029276e8aa7488cf9307e5567da48c3d7af5bac871cb3`.

SFN Power job `970784d5266a77c3` passed preflight, paused normal polling, armed the HTTP lease, received `up:ota:ota_ok`, and began transfer. The device reset the HTTP connection after the backend attempted `68096 / 327944` bytes. Caddy independently logged `connection reset by peer` from the canary public IP. The transfer never completed and the device did not reboot into the lab image.

Post-failure evidence is safe: the canary stayed online on stock `0.1.54-1.0.66`; four outlet states were still reported, voltage was `194.7 V`, and RSSI was `-51 dBm`. No second OTA attempt was made.

This invalidates any assumption that the already-proven remote runtime path `1.0.38 -> stock 1.0.66` automatically proves `stock 1.0.66 -> custom neutral image`. Public Direct-local documentation uses either the stock MEF OTA flow on a redirected lab network or the setup-AP OTA tool (`192.168.1.1:30300`) to install the patched custom firmware from stock `1.0.66`.

**Gate:** do not re-enable the neutral lab package in production until one of those `1.0.66` installation paths is reproduced on the physical canary with packet/transfer evidence. The lab binary remains research-only.

## Remote-path follow-up — neutral V2

The failed `1.0.69` transfer does not prove that `up:ota` rejects custom firmware. The public MatterBridge OTA tool explicitly supports installing custom firmware and uses the same device command `up:ota:<IPv4>`, waits for `up:ota:ota_ok`, then serves `/ota.bin` with `Content-Length`, `Connection: close`, 512-byte writes and 5 ms pacing. The important environmental difference is that the reference tool is normally used directly on the strip's setup LAN, while the failed SFN attempt fetched through the public edge VPS, Caddy, WireGuard and the production backend.

To remove that transport difference, production OTA delivery was moved to the main SFN Power VPS public IPv4 `169.58.193.85`: the strip now fetches `http://169.58.193.85/ota.bin`, main-host Caddy immediately proxies to the local protected OTA backend with `flush_interval -1`. The old edge/VPN path remains unused for new jobs. No firmware job was started while making this routing change; the canary remained connected on stock `0.1.54-1.0.66`.

A second LAB-ONLY image was also built without NOPing any RTOS task creation. `1.0.70` V2 follows the already-public Direct-local patching pattern but uses loopback (`127.0.0.1`) for the four known LG U+ service destinations instead of replacing task startup code. The normal controller address remains provisioned independently through `up:ip`.

V2 artifact (research only, not production catalog):
- file: `/srv/sfn-power-research/lab-artifacts/comMTTL-W01_1.0.70-neutral-lab-v2.fwr`
- size: `327944`
- SHA-256: `0cbf05da978ef3985439e9a1d40e71a8bcd5dea7988a800217615873783472a2`
- decoded checksum: `0x01f88a8b`
- focused builder tests: `2/2` pass

The three task-create instructions changed by V1 are byte-for-byte stock in V2. No physical device has received V2 yet.
