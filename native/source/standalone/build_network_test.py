"""Generate opt-in network bring-up candidate; no device access or activation."""
from pathlib import Path
import argparse,hashlib,json,secrets,subprocess,sys,os
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from standalone.image2_analysis import initialized_body,package_body
from standalone.ota import inspect_image

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--ota-test',action='store_true');parser.add_argument('--sensor-test',action='store_true');parser.add_argument('--relay-test',action='store_true');parser.add_argument('--no-watchdog',action='store_true');parser.add_argument('--legacy-relay-timing',action='store_true');options=parser.parse_args()
    if (options.no_watchdog or options.legacy_relay_timing) and not options.ota_test:parser.error('comparison flags require ota-test')
    if options.sensor_test and not options.ota_test:parser.error('sensor-test requires ota-test')
    if options.relay_test and not options.sensor_test:parser.error('relay-test requires sensor-test')
    ota=options.ota_test
    out=ROOT/('standalone/results/network-ota-test' if ota else 'standalone/results/network-test')
    out.mkdir(parents=True,exist_ok=True)
    if ota:
        sdk=out/'sdk';sdk.mkdir(parents=True,exist_ok=True)
        build_id=secrets.token_hex(8)
        import struct
        identity=json.loads((ROOT/'boot_identity.json').read_text())
        boot_size=identity['boot_size']
        boot_sha=bytes.fromhex(identity['boot_sha256'])
        assert 0<boot_size<0x9000-32 and len(boot_sha)==32
        (sdk/'ota_test_config.h').write_text('#define MTTL_OTA_BUILD_ID "'+build_id+'"\n#define MTTL_OTA_BOOT_BYTES '+str(boot_size)+'\n#define MTTL_OTA_BOOT_SHA {'+','.join(map(str,boot_sha))+'}\n',encoding='utf-8')
        with (sdk/'ota_test_config.h').open('a',encoding='utf-8') as config:
            config.write('#define MTTL_DISABLE_WATCHDOG '+str(int(options.no_watchdog))+'\n#define MTTL_LEGACY_RELAY_TIMING '+str(int(options.legacy_relay_timing))+'\n')
    subprocess.run([sys.executable,str(ROOT/'standalone/build_assets.py'),*(['--ota-test'] if ota else [])],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'analysis/build_minimal_network.py'),'--standalone','--network-test',*(['--ota-test'] if ota else []),*(['--sensor-test'] if options.sensor_test else []),*(['--relay-test'] if options.relay_test else [])],cwd=ROOT,check=True)
    sdk=out/'sdk';elf=sdk/'offline-only.axf'
    tool=Path(os.environ['MTTL_GCC_DIR'])/'bin'
    def run(name,*args):return subprocess.check_output([str(tool/(name+'.exe')),*map(str,args)],text=True)
    syms={p[2]:int(p[0],16) for line in run('arm-none-eabi-nm',elf).splitlines() if len(p:=line.split())==3}
    assert syms['__HeapLimit']<=syms['__mttl_msp_limit']
    assert syms['__mttl_msp_top']-syms['__mttl_msp_limit']==4096
    body,sections=initialized_body(elf.read_bytes())
    assert (b'NETWORK_OTA_TEST_RAM_SETTINGS_' if ota else b'NETWORK_TEST_NO_OTA_RAM_SETTINGS\0') in body
    assert '2.0.12' in inspect_image(package_body(body)[0]).version_candidates
    # Inspect actual setup machine code: a direct BL must start the worker.
    import re
    dis=run('arm-none-eabi-objdump','-d',elf)
    setup=re.search(r'<(?:setup|_Z5setupv)>:\n(.*?)(?=\n[0-9a-f]+ <)',dis,re.S).group(1)
    assert re.search(r'\bbl\b[^\n]*<mttl_sdk_worker_start>',setup)
    (out/'setup-disassembly.txt').write_text(setup,encoding='utf-8')
    # Execute the real setup on ARM; replace only worker startup (no SDK/RTOS).
    sys.path.insert(0,str(ROOT/'.tools/pydeps'))
    from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
    from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_R0
    import struct
    for result in (0,1):
        u=Uc(UC_ARCH_ARM,UC_MODE_THUMB);u.mem_map(0x10000000,0x100000)
        u.mem_write(0x10006000,body)
        calls=[];stop=0x100f0000
        def hook(uc,address,size,context):
            if address==syms['mttl_sdk_worker_start']:
                calls.append(address);uc.reg_write(UC_ARM_REG_R0,result);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
            elif address==stop:uc.emu_stop()
        u.hook_add(UC_HOOK_CODE,hook,begin=syms['mttl_sdk_worker_start'],end=syms['mttl_sdk_worker_start'])
        u.hook_add(UC_HOOK_CODE,hook,begin=stop,end=stop)
        u.reg_write(UC_ARM_REG_SP,0x100ee000);u.reg_write(UC_ARM_REG_LR,stop|1)
        u.emu_start(syms.get('setup',syms.get('_Z5setupv'))|1,stop,count=10000)
        assert u.reg_read(UC_ARM_REG_PC)==stop and len(calls)==1
        assert struct.unpack('<I',u.mem_read(syms['mttl_network_start_result'],4))[0]==result
    objcopy=out/'independent-body.bin'
    args=['-O','binary']
    for s in sections:args+=['-j',s['section']]
    subprocess.run([str(tool/'arm-none-eabi-objcopy.exe'),*args,str(elf),str(objcopy)],check=True)
    assert objcopy.read_bytes()==body
    raw,fwr=package_body(body)
    assert raw[0x28:0x48]==b'LGU+-TAP-HW002'.ljust(32,b'\0')
    assert syms['mttl_native_ota_hardware']==0x10006018
    files=[]
    for ext,data in (('bin',raw),('fwr',fwr)):
        path=out/(('mttl-w01-2.0.12-network-OTA-test.' if ota else 'mttl-w01-2.0.12-network-test-NO-OTA.')+ext)
        path.write_bytes(data)
        assert inspect_image(path.read_bytes()).decoded==raw
        files.append({'path':str(path),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    report={'version':'2.0.12','profile':'NETWORK_OTA_TEST_RAM_SETTINGS' if ota else 'NETWORK_TEST_NO_OTA_RAM_SETTINGS',
            'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'files':files,
            'network_start_verified_in_setup_machine_code':True,
            'setup_arm_mock_success_failure_cases':2,
            'objcopy_byte_match':True,'bin_fwr_roundtrip':True,
            'native_ota_hardware_field_verified':True,
            'msp_reserve_bytes':4096,'static_gap_to_msp_bytes':syms['__mttl_msp_limit']-syms['__HeapLimit'],
            'link_warning_count':sum(': warning:' in line for line in (sdk/'build.log').read_text().splitlines()),
            'boot_signature':'FFFFFFFFFFFFFFFF','device_written':False,
            'target_boot_verified':False,'validation_profile':'Portable format/roundtrip and setup emulator; original decoder fixture not included','production_ready':False,
            'relay_enabled':options.relay_test,'settings_persistent':False,'ota_write_enabled':ota,
            'settings_flash_enabled':True,'settings_sectors':['0xee000','0xef000'],
            'initial_rtos_heap_bytes':(39 if options.sensor_test else 48)*1024 if ota else 100*1024,
            'sensor_gpio_enabled':options.sensor_test,
            'ota_runtime_validated':False,
            'warning':'Experimental bring-up candidate, not target-boot validated. Recovery debugger required. No direct full-flash writing; no bootloader/factory replacement.'}
    report['build_id']=build_id if ota else None
    report['watchdog_enabled']=not options.no_watchdog
    report['legacy_relay_timing']=options.legacy_relay_timing
    (out/'manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    if ota:
        import shutil
        shutil.copyfile(out/'mttl-w01-2.0.12-network-OTA-test.fwr',out/('comMTTL-W01_2.0.12-build-'+build_id+'.fwr'))
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
