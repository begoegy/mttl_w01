#!/usr/bin/env python3
import asyncio, json, os, pathlib, sys
PYZ='/opt/sfn-power/sfn-power.pyz'
SERVER_SOURCE=os.environ.get('SFN_SERVER_SOURCE')
sys.path.insert(0,SERVER_SOURCE or PYZ)
from sfn_power.protocol import mttl_w01 as proto
from sfn_power.device.hub import DeviceHub
from sfn_power.service.power_service import PowerService
MAC='AABBCCDDE201'; FW='2.0.13'
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
    proto.LOCAL_VOLTAGE_GUARD_CAPABLE_FIRMWARES=frozenset(set(proto.LOCAL_VOLTAGE_GUARD_CAPABLE_FIRMWARES)|{FW})
    proto.OFFLINE_SCHEDULE_CAPABLE_FIRMWARES=frozenset(set(proto.OFFLINE_SCHEDULE_CAPABLE_FIRMWARES)|{FW})
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
        r=await service.switch(MAC,0,True);assert r.confirmed and r.applied;results['all_on']=True
        r=await service.switch(MAC,0,False);assert r.confirmed and r.applied;results['all_off']=True
        a,b=await asyncio.gather(service.switch(MAC,1,True),service.switch(MAC,2,True));assert a.confirmed and a.applied and b.confirmed and b.applied;results['rapid_serialized_commands']=True
        ack=await service.set_child_lock(MAC,31);assert ack.mask==31
        ack=await service.child_lock_status(MAC);assert ack.mask==31;results['child_lock']=31
        await hub.request_diagnostics(MAC)
        assert await wait_for(lambda:(service.device(MAC) or {}).get('voltage')==224.5 and (service.device(MAC) or {}).get('rssi')==-55)
        results['voltage_v']=service.device(MAC)['voltage'];results['rssi']=service.device(MAC)['rssi']
        good=proto.LocalVoltageGuardPolicy(revision=17,enabled=True,low_v=180.0,high_v=250.0,hysteresis_v=10.0,trip_hold_seconds=10.0,recovery_seconds=20,auto_restore=False,protected_mask=5)
        assert await service.configure_voltage_guard(MAC,good);results['vguard_revision']=17;results['vguard_mask']=5
        # Production protocol can express faster/auto-restore policies; native 2.0.13 must fail closed until physically proven.
        fast=proto.LocalVoltageGuardPolicy(revision=18,enabled=True,low_v=180.0,high_v=250.0,hysteresis_v=10.0,trip_hold_seconds=2.0,recovery_seconds=20,auto_restore=False,protected_mask=5)
        assert not await hub.session(MAC).configure_voltage_guard(fast,timeout=.35);results['fast_vguard_rejected']=True
        auto=proto.LocalVoltageGuardPolicy(revision=19,enabled=True,low_v=180.0,high_v=250.0,hysteresis_v=10.0,trip_hold_seconds=10.0,recovery_seconds=20,auto_restore=True,protected_mask=5)
        assert not await hub.session(MAC).configure_voltage_guard(auto,timeout=.35);results['auto_restore_rejected']=True
        assert service.is_online(MAC)
        clock=proto.LocalScheduleClock(epoch=1760700000,offset_minutes=180,transition1_epoch=1770000000,transition1_offset_minutes=120,transition2_epoch=1780000000,transition2_offset_minutes=180)
        session=hub.session(MAC);assert session and await session.sync_clock(clock,timeout=1.0);results['clock_sync']=True
        items=[proto.LocalScheduleItem(slot=0,outlet=1,repeat=1,days=0,date_days=proto.LOCAL_SCHEDULE_NONE,minute=480,on=True),proto.LocalScheduleItem(slot=1,outlet=0,repeat=2,days=0x41,date_days=proto.LOCAL_SCHEDULE_NONE,minute=1320,on=False)]
        assert await session.sync_local_schedules(42,items,timeout=1.0);status=await session.request_local_schedule_status(timeout=1.0);assert status and status.generation==42 and status.count==2 and status.clock_valid and status.storage_valid;results['schedule_generation']=42;results['schedule_count']=2
        # Native gate rejects Once until a durable UTC-date representation exists; old committed table must survive.
        once=[proto.LocalScheduleItem(slot=0,outlet=1,repeat=0,days=0,date_days=20000,minute=500,on=True)]
        assert not await session.sync_local_schedules(43,once,timeout=.35);status=await session.request_local_schedule_status(timeout=1.0);assert status and status.generation==42 and status.count==2;results['once_rejected']=True
        # Native capacity is intentionally 8 even though the legacy server protocol can carry 40.
        nine=[proto.LocalScheduleItem(slot=i,outlet=1,repeat=1,days=0,date_days=proto.LOCAL_SCHEDULE_NONE,minute=100+i,on=True) for i in range(9)]
        assert not await session.sync_local_schedules(44,nine,timeout=.35);status=await session.request_local_schedule_status(timeout=1.0);assert status and status.generation==42;results['over_capacity_rejected']=True
        # Force a real TCP disconnect. Native client must back off and reconnect with BootInfo.
        first=hub.session(MAC);assert first;first.close()
        assert await wait_for(lambda:not service.is_online(MAC),2.0), 'disconnect not observed'
        assert await wait_for(lambda:service.is_online(MAC),6.0), 'reconnect timeout'
        second=hub.session(MAC);assert second and second is not first;results['reconnect']=True
        status=await second.request_local_schedule_status(timeout=1.0);assert status and status.generation==42 and status.count==2 and status.storage_valid;results['schedule_survived_reconnect']=True
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
    assert parts.get('vguard_revision')==17 and parts.get('vguard_mask')==5,parts
    assert parts.get('schedule_generation')==42 and parts.get('schedule_count')==2,parts
    print('SFN_PROD_1_6_9_NATIVE_E2E=PASS')
    print(json.dumps(results,indent=2,sort_keys=True))
if __name__=='__main__': asyncio.run(main())
