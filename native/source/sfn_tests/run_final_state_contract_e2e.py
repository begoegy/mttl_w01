#!/usr/bin/env python3
import asyncio
import json
import os
import sys

source = os.environ.get("SFN_SERVER_SOURCE")
if not source:
    raise SystemExit("SFN_SERVER_SOURCE is required")
sys.path.insert(0, source)

from sfn_power.device.hub import DeviceHub
from sfn_power.service.power_service import PowerService

MAC = "AABBCCDDE201"


async def wait_for(pred, timeout=6.0, step=0.02):
    end = asyncio.get_running_loop().time() + timeout
    while asyncio.get_running_loop().time() < end:
        if pred():
            return True
        await asyncio.sleep(step)
    return False


async def open_lab(binary, *host_args):
    service = PowerService()
    hub = DeviceHub(service, poll_seconds=5.0, diagnostics_every=100)
    service.attach(hub)
    servers = await hub.listen("127.0.0.1", [0])
    port = servers.servers[0].sockets[0].getsockname()[1]
    proc = await asyncio.create_subprocess_exec(
        binary, "127.0.0.1", str(port), *host_args,
        stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE,
    )
    assert await wait_for(lambda: service.is_online(MAC)), "connect timeout"
    session = hub.session(MAC)
    assert session and session.firmware == "2.0.13"
    return service, hub, servers, proc, session


async def close_lab(servers, proc):
    proc.terminate()
    try:
        out, err = await asyncio.wait_for(proc.communicate(), 3)
    except asyncio.TimeoutError:
        proc.kill()
        out, err = await proc.communicate()
    await servers.close()
    return out.decode(errors="replace").strip(), err.decode(errors="replace").strip()


async def delayed_all(binary, results):
    service, hub, servers, proc, session = await open_lab(binary, "--relay-ms", "125")
    try:
        start = asyncio.get_running_loop().time()
        assert await session.set_outlets((1, 2, 3, 4), True, settle_seconds=0, confirmation_timeout=3.0)
        elapsed = asyncio.get_running_loop().time() - start
        assert elapsed >= 0.40, elapsed
        assert session.last_outlet_report and all(r.on for r in session.last_outlet_report.readings)
        results["all_four_real_latency"] = round(elapsed, 3)
    finally:
        out, _ = await close_lab(servers, proc)
        results["all_four_host"] = out


async def relay_fault(binary, results):
    service, hub, servers, proc, session = await open_lab(binary, "--relay-ms", "100", "--fault-after", "1")
    try:
        ok = await session.set_outlets((1,), True, settle_seconds=0, confirmation_timeout=0.45)
        assert ok is False
        assert session.last_sent == "up:onoff:1:on"
        results["relay_fault_no_success_ack"] = True
    finally:
        out, _ = await close_lab(servers, proc)
        results["fault_host"] = out


async def voltage_guard_veto(binary, results):
    service, hub, servers, proc, session = await open_lab(binary, "--guard-fault-mask", "1")
    try:
        ok = await session.set_outlets((1,), True, settle_seconds=0, confirmation_timeout=0.35)
        assert ok is False
        results["voltage_guard_blocks_on_without_ack"] = True
    finally:
        out, _ = await close_lab(servers, proc)
        results["guard_host"] = out


async def reconnect_pending(binary, results):
    service, hub, servers, proc, first = await open_lab(binary, "--relay-ms", "500")
    try:
        task = asyncio.create_task(first.set_outlets((1,), True, settle_seconds=0, confirmation_timeout=0.9))
        assert await wait_for(lambda: first.last_sent == "up:onoff:1:on", 0.5)
        await asyncio.sleep(0.05)
        first.close()
        assert await wait_for(lambda: hub.session(MAC) is None, 1.0)
        assert await task is False
        assert await wait_for(lambda: hub.session(MAC) is not None, 4.0)
        second = hub.session(MAC)
        assert second is not None and second is not first
        await asyncio.sleep(0.15)
        assert not second.onoff_acks, second.onoff_acks
        assert await second.refresh(timeout=1.0)
        states = {r.channel: r.on for r in second.last_outlet_report.readings}
        assert states[1] is True
        results["reconnect_drops_stale_ack_keeps_relay_result"] = True
    finally:
        out, _ = await close_lab(servers, proc)
        results["reconnect_host"] = out


async def main():
    if len(sys.argv) != 2:
        raise SystemExit("host strip binary required")
    results = {}
    await delayed_all(sys.argv[1], results)
    await relay_fault(sys.argv[1], results)
    await voltage_guard_veto(sys.argv[1], results)
    await reconnect_pending(sys.argv[1], results)
    print("SFN_FINAL_STATE_CONTRACT_E2E=PASS")
    print(json.dumps(results, indent=2, sort_keys=True))


if __name__ == "__main__":
    asyncio.run(main())
