#!/usr/bin/env python3
import asyncio, json, pathlib, sys
PYZ='/opt/sfn-power/sfn-power.pyz'
sys.path.insert(0,PYZ)
from sfn_power.protocol import mttl_w01 as proto
from sfn_power.device.hub import DeviceHub
from sfn_power.service.power_service import PowerService
MAC='AABBCCDDE201'; FW='2.0.12'
async def wait_for(pred,timeout=6.0,step=.05):
    end=asyncio.get_running_loop().time()+timeout
    while asyncio.get_running_loop().time()<end:
        if pred(): return True
        await asyncio.sleep(step)
    return False
async def main():
    if len(sys.argv)!=2: raise SystemExit('host strip binary required')
    # Lab-only capability admission. Production process/files are not mutated.
    proto.CHILD_LOCK_CAPABLE_FIRMWARES=frozenset(set(proto.CHILD_LOCK_CAPABLE_FIRMWARES)|{FW})
    service=PowerService();hub=DeviceHub(service,poll_seconds=.2,diagnostics_every=2);service.attach(hub)
    servers=await hub.listen('127.0.0.1',[0]);server=servers.servers[0];port=server.sockets[0].getsockname()[1]
    proc=await asyncio.create_subprocess_exec(sys.argv[1],'127.0.0.1',str(port),stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
    results={}
    try:
        assert await wait_for(lambda:service.is_online(MAC)), 'boot/connect timeout'
        d=service.device(MAC);assert d and d['fw']==FW;results['bootinfo']=True
        assert await service.refresh_state(MAC);results['refresh']=True
        r=await service.switch(MAC,1,True);assert r.confirmed and r.applied;results['outlet_on']=True
        r=await service.switch(MAC,1,False);assert r.confirmed and r.applied;results['outlet_off']=True
        ack=await service.set_child_lock(MAC,31);assert ack.mask==31
        ack=await service.child_lock_status(MAC);assert ack.mask==31;results['child_lock']=31
        await hub.request_diagnostics(MAC)
        assert await wait_for(lambda:(service.device(MAC) or {}).get('voltage')==224.5 and (service.device(MAC) or {}).get('rssi')==-55)
        results['voltage_v']=service.device(MAC)['voltage'];results['rssi']=service.device(MAC)['rssi']
        # Force a real TCP disconnect. Native client must back off and reconnect with BootInfo.
        first=hub.session(MAC);assert first;first.close()
        assert await wait_for(lambda:not service.is_online(MAC),2.0), 'disconnect not observed'
        assert await wait_for(lambda:service.is_online(MAC),6.0), 'reconnect timeout'
        second=hub.session(MAC);assert second and second is not first;results['reconnect']=True
        r=await service.switch(MAC,2,True);assert r.confirmed and r.applied;results['post_reconnect_command']=True
        # OTA must not be acknowledged by this firmware profile, and link must survive.
        ota=await hub.request_ota(MAC,'192.0.2.10');assert ota is False;assert service.is_online(MAC);results['ota_acknowledged']=False;results['online_after_rejected_ota']=True
    finally:
        proc.terminate()
        try: out,err=await asyncio.wait_for(proc.communicate(),3)
        except asyncio.TimeoutError: proc.kill();out,err=await proc.communicate()
        await servers.close()
    text=out.decode(errors='replace').strip();results['host_summary']=text
    assert 'rejected=' in text and 'reconnects=' in text
    parts={k:int(v) for k,v in (item.split('=',1) for item in text.split() if '=' in item and item.split('=',1)[1].lstrip('-').isdigit())}
    assert parts.get('reconnects',0)>=2,parts
    assert parts.get('rejected',0)>=1,parts
    assert parts.get('io_failures',0)>=1,parts
    print('SFN_PROD_1_6_9_NATIVE_E2E=PASS')
    print(json.dumps(results,indent=2,sort_keys=True))
if __name__=='__main__': asyncio.run(main())
