"""Compile/link experiment only. Does not create OTA files or access a device."""
from pathlib import Path
import re, subprocess, json, argparse, os
parser=argparse.ArgumentParser()
parser.add_argument('--reserve-bytes',type=int,default=0)
parser.add_argument('--standalone',action='store_true',help='Build the 2.0.0 offline baseline in standalone/results/sdk')
parser.add_argument('--network-test',action='store_true',help='Opt-in network-start test, separate output; no device access')
parser.add_argument('--ota-test',action='store_true',help='Opt-in self-OTA test profile; separate output')
parser.add_argument('--sensor-test',action='store_true',help='Experimental sensor GPIO profile, requires OTA test')
parser.add_argument('--relay-test',action='store_true',help='Experimental output profile; requires sensor-test')
args=parser.parse_args()
if args.ota_test and not args.network_test: parser.error('ota-test requires network-test')
if args.sensor_test and not args.ota_test: parser.error('sensor-test requires ota-test')
if args.relay_test and not args.sensor_test: parser.error('relay-test requires sensor-test')
if args.network_test and not args.standalone: parser.error('network-test requires standalone')
if args.reserve_bytes < 0: parser.error('reserve-bytes must not be negative')
ROOT=Path(__file__).resolve().parents[1]
hw=ROOT/'work/reference/ameba-arduino-1/Arduino_package/hardware'
out=ROOT/('analysis/ghidra/reports/minimal-network-build' if not args.reserve_bytes else f'analysis/ghidra/reports/minimal-network-budget-{args.reserve_bytes}')
if args.standalone:
 out=ROOT/'standalone/results/sdk'
if args.network_test:
 out=ROOT/'standalone/results/network-test/sdk'
if args.ota_test:
 out=ROOT/'standalone/results/network-ota-test/sdk'
out.mkdir(exist_ok=True,parents=True)
tool=Path(os.environ['MTTL_GCC_DIR'])/'bin'
plat=(hw/'platform.txt').read_text()
line=next(x for x in plat.splitlines() if x.startswith('compiler.ameba.c.include='))
incs=['-I'+x.replace('{ameba.sdkpath}',str(hw/'system')).replace('{ameba.project}','realtek_ameba1_va0_example') for x in re.findall(r'"-I([^"]+)"',line)]
incs = ['-I'+str(hw/x) for x in ['cores/arduino','variants/rtl8710','libraries/WiFi/src','libraries/MQTTClient/src','system/component/common/network/httpd']] + incs
common=['-Os','-g','-mcpu=cortex-m3','-mthumb','-fno-short-enums','-fno-common','-ffunction-sections','-fdata-sections','-DHTTPD_USE_TLS=-1','-DCONFIG_PLATFORM_8195A','-DARDUINO_SDK','-DARDUINO_AMEBA','-DARDUINO=10600','-DARDUINO_ARCH_AMEBA','-DBOARD_RTL8710','-DF_CPU=83333333L',*incs]
common += ['-DMTTL_HEAP_CEILING=0x1006F000',f'-DMTTL_RESERVE_BYTES={args.reserve_bytes}']
if args.standalone: common += ['-I'+str(out),'-DMTTL_PRESERVE_IMAGE1_RAM=1']
if args.network_test:
 common += ['-DMTTL_NETWORK_TEST=1','-I'+str(ROOT/'standalone/results/sdk')]
if args.ota_test: common += ['-DMTTL_OTA_TEST=1']
if args.sensor_test: common += ['-DMTTL_SENSOR_TEST=1']
if args.relay_test: common += ['-DMTTL_RELAY_TEST=1']
sources=[ROOT/'analysis/minimal_network_link.cpp',ROOT/'analysis/bounded_sbrk.c',ROOT/'analysis/http_only_adapter.c',ROOT/'analysis/newlib_scheduler_guard.c',hw/'variants/rtl8710/variant.cpp']
if args.standalone:
 sources[0]=ROOT/'standalone/firmware/sdk_baseline.cpp'
 sources.append(hw/'system/component/os/freertos/freertos_v8.1.2/Source/portable/MemMang/heap_5.c')
 if args.ota_test:
  sources[-1]=ROOT/'standalone/firmware/sdk_test_heap.c'
  sources.append(ROOT/'standalone/firmware/sdk_ota_runtime.c')
 sources.append(ROOT/'standalone/firmware/core.c')
 sources.append(ROOT/'standalone/firmware/ota_guard.c')
 sources.append(ROOT/'standalone/firmware/ota_stream.c')
 sources.append(ROOT/'standalone/firmware/ota_hash.c')
 sources.append(ROOT/'standalone/firmware/ota_readback.c')
 sources.append(ROOT/'standalone/firmware/ota_stage.c')
 sources.append(ROOT/'standalone/firmware/ota_identity.c')
 sources.append(ROOT/'standalone/firmware/ota_session.c')
 sources.append(ROOT/'standalone/firmware/http_framing.c')
 common += ['-I'+str(ROOT/'standalone/firmware')]
 sources.append(ROOT/'standalone/firmware/sdk_ota_flash.c')
 sources.append(ROOT/'standalone/firmware/sdk_wifi_adapter.c')
 sources.append(ROOT/'standalone/firmware/lifecycle.c')
 sources.append(ROOT/'standalone/firmware/sdk_web.c')
 sources.append(ROOT/'standalone/firmware/sdk_wifi_no_fast_connect.c')
 sources.append(ROOT/'standalone/firmware/sdk_dhcp.c')
 sources.append(ROOT/'standalone/firmware/sdk_httpd_core.c')
 sources.append(ROOT/'standalone/firmware/sdk_worker.c')
 for name in ('meter_probe','meter_bus','meter_init','meter_convert','sdk_meter','sdk_energy','sdk_memory','sdk_relay','sdk_sse'):
  sources.append(ROOT/'standalone/firmware'/(name+'.c'))
 sources.append(ROOT/'standalone/firmware/wifi_form.c')
 sources.append(ROOT/'standalone/firmware/sdk_http_io.c')
 sources.append(ROOT/'standalone/firmware/sdk_httpd_msg.c')
 sources.append(ROOT/'standalone/firmware/settings_record.c')
 sources.append(ROOT/'standalone/firmware/settings.c')
 sources.append(ROOT/'standalone/firmware/protection.c')
 sources.append(ROOT/'standalone/firmware/scheduler.c')
 sources.append(ROOT/'standalone/firmware/settings_store.c')
 sources.append(ROOT/'standalone/firmware/sdk_settings_flash.c')
 sources.append(ROOT/'standalone/firmware/config_store.c')
 sources.append(ROOT/'standalone/firmware/ntp.c')
 sources.append(ROOT/'standalone/firmware/ntp_client.c')
 sources.append(ROOT/'standalone/firmware/ntp_transport.c')
 sources.append(ROOT/'standalone/firmware/sdk_ntp.c')
 sources.append(ROOT/'standalone/firmware/mqtt_session.c')
 sources.append(ROOT/'standalone/firmware/mqtt_command.c')
 sources.append(ROOT/'standalone/firmware/mqtt_wire.c')
 for name in ('ha_discovery','mqtt_engine','hardware_port','device_app','sdk_mqtt_socket'):
  sources.append(ROOT/'standalone/firmware'/ (name+'.c'))
for folder in ['cores/arduino','libraries/WiFi/src','libraries/MQTTClient/src','system/component/common/network/httpd']:
 sources += sorted(p for p in (hw/folder).glob('*') if p.suffix in ['.c','.cpp'] and p.name != 'httpd_tls.c' and not (args.standalone and p.name in ('httpd_core.c','httpd_util.c','httpd_msg.c')))
logs=[]; objects=[]
def run(cmd):
 r=subprocess.run([str(x) for x in cmd],capture_output=True,text=True)
 logs.append('$ '+' '.join(map(str,cmd))+'\n'+r.stdout+r.stderr)
 (out/'build.log').write_text('\n'.join(logs),encoding='utf-8')
 return r
for i,src in enumerate(sources):
 obj=out/f'{i:02d}-{src.stem}.o'
 cpp=src.suffix=='.cpp'
 extra=['-fstack-usage'] + (['-std=gnu++11','-fno-exceptions','-fno-rtti'] if cpp else ['-std=gnu99'])
 r=run([tool/('arm-none-eabi-g++.exe' if cpp else 'arm-none-eabi-gcc.exe'),*common,*extra,'-c',src,'-o',obj])
 if r.returncode:
  print('COMPILE FAILED',src.name,r.stderr[-4500:]);raise SystemExit(1)
 objects.append(obj)
 print('compiled',src.name,flush=True)
variant=hw/'variants/rtl8710'
libs=[variant/p for p in re.findall(r'"\{build.variant.path\}/([^"]+)"',next(x for x in plat.splitlines() if x.startswith('compiler.ameba.ar.list=')))]
elf=out/'offline-only.axf'
linker=variant/'linker_scripts/gcc/rtl8710-symbol-v03-img2_arduino_arduino.ld'
if args.standalone:
 original=linker.read_text()
 needle='KEEP(*(.infra.ram.start*))'
 assert original.count(needle)==1
 linker=out/'native-ota-image2.ld'
 linker.write_text(original.replace(needle,'KEEP(*(.mttl.native.hardware))\n    '+needle),encoding='utf-8')
cmd=[tool/'arm-none-eabi-g++.exe','-mcpu=cortex-m3','-mthumb','--specs=nano.specs','-L'+str(variant/'linker_scripts/gcc'),'-T'+str(linker),'-Wl,--gc-sections','-Wl,--entry=Reset_Handler','-Wl,--undefined=InfraStart','-Wl,-wrap,rom_psk_CalcGTK','-Wl,-wrap,rom_psk_CalcPTK','-Wl,-Map='+str(out/'application.map'),'-o',elf,'-Wl,--start-group',*objects,*libs,'-lstdc++','-lsupc++','-lm','-lc','-lgcc','-lnosys','-Wl,--end-group']
cmd += ['-T'+str(ROOT/'analysis/memory_bounds.ld'),'-lnosys']
cmd += [f'-Wl,--wrap,{name}' for name in ['_malloc_r','_free_r','_realloc_r','_calloc_r']]
r=run(cmd)
print('LINK EXIT',r.returncode,r.stderr[-6000:])
if not r.returncode:
 r=run([tool/'arm-none-eabi-size.exe','-A',elf]);print(r.stdout)
 (out/'sections.txt').write_text(r.stdout,encoding='utf-8')
raise SystemExit(r.returncode)
