#!/usr/bin/env python3
import asyncio
import hashlib
import json
import pathlib
import sys

PYZ = pathlib.Path('/opt/sfn-power/sfn-power.pyz')
MAC = 'AABBCCDDE201'
BEFORE = hashlib.sha256(PYZ.read_bytes()).hexdigest()
sys.path.insert(0, str(PYZ))
from sfn_power.device.hub import DeviceHub
from sfn_power.service.power_service import PowerService

async def wait_for(pred, timeout=6.0, step=0.02):
    end = asyncio.get_running_loop().time() + timeout
    while asyncio.get_running_loop().time() < end:
        if pred():
            return True
        await asyncio.sleep(step)
    return False

async def main():
    if len(sys.argv) != 2:
        raise SystemExit('host strip binary required')
    service = PowerService()
    hub = DeviceHub(service, poll_seconds=5.0, diagnostics_every=100)
    service.attach(hub)
    servers = await hub.listen('127.0.0.1', [0])
    port = servers.servers[0].sockets[0].getsockname()[1]
    proc = await asyncio.create_subprocess_exec(
        sys.argv[1], '127.0.0.1', str(port), '--relay-ms', '125',
        stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE,
    )
    results = {}
    try:
        assert await wait_for(lambda: service.is_online(MAC)), 'connect timeout'
        start = asyncio.get_running_loop().time()
        result = await service.switch(MAC, 0, True)
        elapsed = asyncio.get_running_loop().time() - start
        assert result.confirmed and result.applied, result
        # Four physical operations at 125 ms each cannot be final at the server's
        # historical 0.3 s settle point. Success proves the firmware deferred the
        # requested report until the relay queue really finished.
        assert elapsed >= 0.45, elapsed
        results['all_on_confirmed_applied'] = True
        results['all_on_elapsed_seconds'] = round(elapsed, 3)

        result = await service.switch(MAC, 0, False)
        assert result.confirmed and result.applied, result
        results['all_off_confirmed_applied'] = True

        result = await service.switch(MAC, 1, True)
        assert result.confirmed and result.applied, result
        result = await service.switch(MAC, 1, False)
        assert result.confirmed and result.applied, result
        results['single_outlet_roundtrip'] = True
    finally:
        proc.terminate()
        try:
            out, err = await asyncio.wait_for(proc.communicate(), 3)
        except asyncio.TimeoutError:
            proc.kill()
            out, err = await proc.communicate()
        await servers.close()
    after = hashlib.sha256(PYZ.read_bytes()).hexdigest()
    assert BEFORE == after
    results['production_server_sha256'] = BEFORE
    results['production_server_unchanged'] = True
    results['host_summary'] = out.decode(errors='replace').strip()
    results['host_stderr'] = err.decode(errors='replace').strip()
    print('SFN_UNMODIFIED_PROD_SERVER_E2E=PASS')
    print(json.dumps(results, indent=2, sort_keys=True))

if __name__ == '__main__':
    asyncio.run(main())
