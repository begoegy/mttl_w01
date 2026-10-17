#include "sdk_worker.h"
#include "sdk_wifi_adapter.h"
#include "sdk_web.h"
#include "sdk_dhcp.h"
#include "lifecycle.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "wifi_conf.h"
#include "device_app.h"
#ifndef SFN_NATIVE_UP_ONLY
#include "sdk_mqtt_socket.h"
#else
#include "sdk_up_socket.h"
#include "sfn_backend.h"
#include "sfn_native_build_config.h"
#endif
#include "sdk_settings_flash.h"
#include "sdk_ntp.h"
#include "sdk_meter.h"
#include "sdk_energy.h"
#include "sdk_memory.h"
#include "sdk_relay.h"
#include "sfn_voltage_guard.h"
#include "sfn_voltage_policy.h"
#include "sfn_schedule_sync.h"
#ifdef SFN_BOOT_GUARD
#include "sdk_boot_guard.h"
#endif
#include "sfn_child_lock.h"
#include "gpio_api.h"
#include <stdio.h>
#include <string.h>
#include "watchdog_health.h"
#ifdef MTTL_OTA_TEST
#include "ota_test_config.h"
#endif
extern void wdt_disable(void);
extern void wdt_enable(uint32_t);
extern void wdt_reset(void);
static volatile uint32_t watchdog_worker_at,watchdog_http_at,watchdog_relay_at;
static volatile uint8_t watchdog_worker_seen,watchdog_http_active;
static uint8_t watchdog_started,watchdog_expired;
void mttl_sdk_watchdog_http(int active){watchdog_http_at=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;watchdog_http_active=active!=0;}
static mttl_device_app device;
static mttl_schedule_state schedule_state;
static sfn_voltage_guard_state voltage_guard_state;
static sfn_schedule_sync schedule_sync;
static uint32_t voltage_guard_sample_at;
static uint8_t voltage_guard_sample_seen;
static uint8_t worker_started;
static uint8_t config_occupied,occupied;
static uint8_t energy_barrier_state; /* 0 idle, 1 requested, 2 saved, 3 failed */
/* Four waiting commands, in addition to the active relay operation. */
static uint8_t control_pending,control_head;
static struct {uint32_t at;uint8_t channel,on;} control_queue[4];
static SemaphoreHandle_t relay_wake;
static mttl_control relay_control;
static mttl_hardware_port relay_port;
static volatile uint8_t relay_busy,storage_busy;
static uint32_t relay_save_attempt_at;
static uint8_t relay_save_retry;
/* Arduino task owns HW feed; each monitored owner updates its own progress.
 * Once expired, never resume feeding merely because a stuck task briefly wakes. */
void mttl_sdk_watchdog_tick(void){
#if MTTL_DISABLE_WATCHDOG
 if(!watchdog_started){wdt_disable();watchdog_started=1;}return;
#else
 uint32_t now,worker_at,http_at,relay_at;uint8_t seen,http_active,relay_active;
 /* Snapshot owners and clock without higher-priority progress updates in
  * between. Never hold this critical section across watchdog SDK calls. */
 taskENTER_CRITICAL();
 worker_at=watchdog_worker_at;http_at=watchdog_http_at;relay_at=watchdog_relay_at;
 seen=watchdog_worker_seen;http_active=watchdog_http_active;relay_active=relay_busy;
 now=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;
 taskEXIT_CRITICAL();
 if(!seen)return;
 if(!watchdog_started){wdt_enable(8000);watchdog_started=1;}
 if(!mttl_watchdog_fresh(now,worker_at)||(http_active&&!mttl_watchdog_fresh(now,http_at))||(relay_active&&!mttl_watchdog_fresh(now,relay_at)))watchdog_expired=1;
 if(!watchdog_expired)wdt_reset();
#endif
}
static uint8_t relay_known,relay_on,relay_fault;
static uint8_t relay_desired,buttons_ready,provision_requested;
static gpio_t button_pins[5];
static mttl_button_input button_inputs[5];
/* Stock BSP command bit 16 drives PC3, separate from relay LED frames. */
static gpio_t setup_led;
static uint32_t setup_led_at;
static uint8_t setup_led_edges,setup_led_on;
static uint32_t relay_requested_at,relay_wait_ms,relay_run_ms,relay_sequence;
static int storage_begin(void){int ok=0;
#if MTTL_LEGACY_RELAY_TIMING
 taskENTER_CRITICAL();if(!relay_busy&&!storage_busy){storage_busy=1;ok=1;}
#else
 uint32_t now=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;taskENTER_CRITICAL();if(!relay_busy&&!control_pending&&!storage_busy&&(!relay_sequence||(uint32_t)(now-(relay_requested_at+relay_wait_ms+relay_run_ms))>=1500u)){storage_busy=1;ok=1;}
#endif
 taskEXIT_CRITICAL();return ok;}
static void storage_end(void){taskENTER_CRITICAL();storage_busy=0;taskEXIT_CRITICAL();}
/* Debounce relay-state persistence from the LAST completed command, not
 * from the first change. A queued/active command cancels the quiet window. */
static __attribute__((noinline)) int relay_save_ready(uint32_t now){int ready;taskENTER_CRITICAL();ready=!relay_busy&&!control_pending&&(uint32_t)(now-(relay_requested_at+relay_wait_ms+relay_run_ms))>=10000u;taskEXIT_CRITICAL();return ready;}
static void relay_task(void *unused){mttl_output out={&relay_port,mttl_hardware_begin,mttl_hardware_end};(void)unused;
 for(;;){unsigned ch,on;uint32_t start;
  xSemaphoreTake(relay_wake,portMAX_DELAY);
  for(;;){
  taskENTER_CRITICAL();
  if(!control_pending){taskEXIT_CRITICAL();break;}
  ch=control_queue[control_head].channel;on=control_queue[control_head].on;
  relay_requested_at=control_queue[control_head].at;control_head=(control_head+1)&3;--control_pending;
  taskEXIT_CRITICAL();
  while(storage_busy)vTaskDelay(1); /* Never energize coils across NOR operations. */
  start=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;
  if(mttl_request(&relay_control,ch,on))do{
   mttl_control_tick(&relay_control,&out,(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS);
   watchdog_relay_at=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;
   taskENTER_CRITICAL();relay_known=relay_control.known_mask;relay_on=relay_control.on_mask;relay_fault=relay_control.fault;taskEXIT_CRITICAL();
   /* Blocking pulse adapter has already waited and released the coil.
    * Finalize immediately when due; do not add a task sleep between channels. */
   if(relay_control.active&&(int32_t)((uint32_t)xTaskGetTickCount()*portTICK_RATE_MS-relay_control.deadline)>=0)continue;
   if(relay_control.active||relay_control.count)vTaskDelay(1);
  }while(!relay_control.fault&&(relay_control.active||relay_control.count));
  taskENTER_CRITICAL();relay_known=relay_control.known_mask;relay_on=relay_control.on_mask;relay_fault=relay_control.fault;
  relay_wait_ms=start-relay_requested_at;relay_run_ms=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS-start;++relay_sequence;
  if(relay_fault)control_pending=0;
  relay_busy=control_pending!=0;taskEXIT_CRITICAL();
  }
 }
}
void mttl_sdk_control_timing(uint32_t out[4]){taskENTER_CRITICAL();out[0]=relay_sequence;out[1]=relay_wait_ms;out[2]=relay_run_ms;out[3]=relay_busy;taskEXIT_CRITICAL();}
uint32_t mttl_sdk_relay_snapshot(void){uint32_t value;taskENTER_CRITICAL();value=relay_known|(relay_on<<4)|(relay_fault<<8);taskEXIT_CRITICAL();return value;}
int mttl_sdk_energy_barrier(void){unsigned i;int result=0;
 taskENTER_CRITICAL();if(!worker_started||energy_barrier_state||config_occupied||occupied||relay_busy||control_pending||provision_requested){taskEXIT_CRITICAL();return 0;}energy_barrier_state=1;taskEXIT_CRITICAL();
 for(i=0;i<100;i++){uint8_t state;taskENTER_CRITICAL();state=energy_barrier_state;taskEXIT_CRITICAL();if(state>=2){result=state==2;break;}vTaskDelay(20/portTICK_RATE_MS+1);}
 if(!result)mttl_sdk_energy_release();return result;
}
void mttl_sdk_energy_release(void){taskENTER_CRITICAL();energy_barrier_state=0;taskEXIT_CRITICAL();}
#ifndef SFN_NATIVE_UP_ONLY
static mttl_sdk_mqtt_socket mqtt_socket;
static mttl_mqtt_io mqtt_io;
#define SFN_MQTT_IO (&mqtt_io)
#else
static sfn_sdk_up_socket up_socket;
static sfn_up_io up_io;
static uint8_t up_physical_pending;
#define SFN_MQTT_IO ((const mttl_mqtt_io *)0)
#endif
static char device_json[1536],identity[13];
static size_t device_size;
static mttl_settings pending_config;
static uint8_t config_ready;
static uint8_t config_keep_password;
static uint8_t ready,ap_ready,stop_phase;
static mttl_worker_status published;
static uint32_t rssi_polled_at;
static int cached_rssi;
static uint8_t cached_rssi_valid;
static uint32_t clock_ms(void *context);
static int control_enqueue(unsigned channel,unsigned on,int local);
static void protection_off_mask(uint8_t mask);
#ifdef SFN_NATIVE_UP_ONLY
extern const char mttl_firmware_version[];
static void sfn_backend_text(uint32_t ip,char out[16]){snprintf(out,16,"%lu.%lu.%lu.%lu",(unsigned long)(ip>>24),(unsigned long)((ip>>16)&255u),(unsigned long)((ip>>8)&255u),(unsigned long)(ip&255u));}
static size_t sfn_up_handle(void *context,const sfn_up_command *command,char *out,size_t cap){
 mttl_device_app *d=context;uint32_t now=clock_ms(0);unsigned i;int32_t value;uint8_t known,on;
 if(!d||!command||!out)return 0;
 switch(command->type){
  case SFN_UP_CMD_GETINFO_ALL:{sfn_up_outlet x[4];taskENTER_CRITICAL();known=relay_known;on=relay_on;taskEXIT_CRITICAL();
   for(i=0;i<4;i++){int32_t power=0,temp=0,energy=(int32_t)d->settings.energy_wh[i];
    (void)mttl_sample_read(&d->samples.power[i],now,MTTL_SENSOR_MAX_AGE_MS,&power);
    if(mttl_sample_read(&d->samples.temperature[i],now,MTTL_SENSOR_MAX_AGE_MS,&temp))temp/=10;else temp=0;
    (void)mttl_sample_read(&d->samples.meter[i],now,MTTL_SENSOR_MAX_AGE_MS,&energy);
    x[i].channel=(uint8_t)(i+1);x[i].on=(known&(1u<<i))&&((on&(1u<<i))!=0);x[i].runtime=(int32_t)(now/1000u);x[i].state=(known&(1u<<i))?1:0;x[i].power_mw=power;x[i].energy_wh=energy<0?0:(uint32_t)energy;x[i].temperature_c=temp;x[i].overload="0";x[i].overheat="0";}
   return sfn_up_encode_getinfo(out,cap,x);}
  case SFN_UP_CMD_ONOFF:
   if(control_enqueue(command->channel-1,command->on,0))return sfn_up_encode_onoff_ack(out,cap,command->channel,command->on);return 0;
  case SFN_UP_CMD_VOLTAGE:
   if(mttl_sample_read(&d->samples.voltage,now,MTTL_SENSOR_MAX_AGE_MS,&value)&&value>=0)return sfn_up_encode_voltage(out,cap,1,value*100);return 0;
  case SFN_UP_CMD_RSSI:
   if(cached_rssi_valid)return sfn_up_encode_rssi(out,cap,cached_rssi);return 0;
  case SFN_UP_CMD_CHILDLOCK_GET:return sfn_up_encode_childlock(out,cap,d->settings.child_lock_mask);
  case SFN_UP_CMD_CHILDLOCK_SET:{unsigned old=d->settings.child_lock_mask;if(command->mask!=old&&!config_occupied&&!occupied&&!energy_barrier_state){mttl_settings candidate=d->settings;
    candidate.child_lock_mask=command->mask;if(storage_begin()){int saved=mttl_sdk_settings_save(&candidate);storage_end();if(saved==1){taskENTER_CRITICAL();d->settings=candidate;++d->revision;taskEXIT_CRITICAL();old=command->mask;}}}
   return sfn_up_encode_childlock(out,cap,old);}
  case SFN_UP_CMD_VGUARD_SET:{sfn_voltage_guard_config policy;mttl_settings candidate;int saved;
   if(!sfn_voltage_policy_decode(command->flags,command->thresholds,command->timing,command->recovery_seconds,&policy))return 0;
   if(!memcmp(&policy,&d->settings.voltage_guard,sizeof(policy)))return sfn_up_encode_vguard_ack(out,cap,command->revision);
   if(config_occupied||occupied||energy_barrier_state)return 0;candidate=d->settings;candidate.voltage_guard=policy;
   if(!storage_begin())return 0;saved=mttl_sdk_settings_save(&candidate);storage_end();if(saved!=1)return 0;
   taskENTER_CRITICAL();d->settings=candidate;++d->revision;sfn_voltage_guard_init(&voltage_guard_state);voltage_guard_sample_seen=0;voltage_guard_sample_at=0;taskEXIT_CRITICAL();
   return sfn_up_encode_vguard_ack(out,cap,command->revision);}
  case SFN_UP_CMD_CLOCK_SYNC:
   if(!sfn_schedule_sync_clock(&schedule_sync,command->epoch,command->offset_minutes,command->transition1_epoch,command->transition1_offset_minutes,command->transition2_epoch,command->transition2_offset_minutes))return 0;
   mttl_time_set(&d->ntp.time,(uint64_t)command->epoch*1000u,now);schedule_state.primed=0;schedule_state.pending=0;return sfn_up_encode_clock_ack(out,cap,command->epoch);
  case SFN_UP_CMD_SCHEDULE_BEGIN:
   (void)sfn_schedule_sync_begin(&schedule_sync,command->generation,command->count);return 0;
  case SFN_UP_CMD_SCHEDULE_ITEM:
   (void)sfn_schedule_sync_item(&schedule_sync,command->slot,command->outlet,command->repeat,command->days,command->date_days,command->minute,command->on);return 0;
  case SFN_UP_CMD_SCHEDULE_COMMIT:{mttl_schedule_config schedule;sfn_schedule_meta meta;mttl_settings candidate;int saved;
   if(!sfn_schedule_sync_commit(&schedule_sync,command->generation,&schedule,&meta))return 0;if(config_occupied||occupied||energy_barrier_state)return 0;
   candidate=d->settings;candidate.schedule=schedule;candidate.sfn_schedule=meta;if(!storage_begin())return 0;saved=mttl_sdk_settings_save(&candidate);storage_end();if(saved!=1)return 0;
   taskENTER_CRITICAL();d->settings=candidate;++d->revision;schedule_state.primed=0;schedule_state.pending=0;taskEXIT_CRITICAL();return sfn_up_encode_schedule_commit_ack(out,cap,meta.generation,meta.count);}
  case SFN_UP_CMD_SCHEDULE_STATUS:{uint64_t current;int clock_valid=mttl_time_now(&d->ntp.time,now,&current);(void)current;return sfn_up_encode_schedule_status(out,cap,d->settings.sfn_schedule.generation,d->settings.sfn_schedule.count,clock_valid,d->settings.sfn_schedule.valid);}
  default:return 0;
 }
}
static void sfn_up_disconnect(void){if(device.up.state!=SFN_UP_LINK_OFF)sfn_up_client_reset(&device.up,&up_io);}
static void sfn_up_service(int online){char host[16];uint32_t ip=device.settings.sfn_backend_ipv4;if(!online||!ip||!identity[0]){sfn_up_disconnect();return;}sfn_backend_text(ip,host);(void)sfn_up_client_step(&device.up,&up_io,1,host,device.settings.sfn_backend_port,clock_ms(0),"lgutap",identity,mttl_firmware_version,sfn_up_handle,&device);}
static void sfn_up_physical_events(void){unsigned i;char frame[48];size_t n;if(relay_busy||control_pending||!up_physical_pending)return;for(i=0;i<5;i++)if(up_physical_pending&(1u<<i)){int state=i?((relay_on&(1u<<(i-1)))!=0):(relay_on==15);n=sfn_up_encode_event(frame,sizeof(frame),i,state);if(n&&sfn_up_client_publish(&device.up,frame,n))up_physical_pending=(uint8_t)(up_physical_pending&~(1u<<i));break;}}
#endif
static int control_enqueue(unsigned channel,unsigned on,int local){int ok=0;if(channel>4||on>1)return 0;taskENTER_CRITICAL();if(worker_started&&relay_wake&&(local||published.state==MTTL_NET_ONLINE)&&!energy_barrier_state&&!occupied&&!config_occupied&&control_pending<4&&device.hardware.verified&&!relay_fault&&!(on&&(channel==4?sfn_voltage_guard_blocked_mask(&voltage_guard_state,&device.settings.voltage_guard)!=0:(sfn_voltage_guard_blocked_mask(&voltage_guard_state,&device.settings.voltage_guard)&(1u<<channel))))){unsigned tail=(control_head+control_pending)&3;control_queue[tail].channel=channel;control_queue[tail].on=on;control_queue[tail].at=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;++control_pending;
/* Publish the new progress timestamp atomically with busy, before waking
 * the relay task or allowing watchdog evaluation of this command. */
watchdog_relay_at=control_queue[tail].at;relay_busy=1;if(channel==4)relay_desired=on?15:0;else if(on)relay_desired|=1u<<channel;else relay_desired&=~(1u<<channel);ok=1;}taskEXIT_CRITICAL();if(ok)xSemaphoreGive(relay_wake);return ok;}
int mttl_sdk_control_request(unsigned channel,unsigned on){return control_enqueue(channel,on,0);}
/* Replace waiting ON commands with a priority ALL OFF; an active coil pulse
 * still finishes/release safely. No persistent lock or automatic re-enable. */
static void protection_off(void){taskENTER_CRITICAL();if(relay_wake&&device.hardware.verified&&!relay_fault){control_head=0;control_pending=1;control_queue[0].channel=4;control_queue[0].on=0;control_queue[0].at=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;watchdog_relay_at=control_queue[0].at;relay_busy=1;relay_desired=0;}taskEXIT_CRITICAL();if(relay_wake)xSemaphoreGive(relay_wake);}
static void protection_off_mask(uint8_t mask){
 unsigned i;uint32_t now;int wake=0;if(!(mask&15u))return;if((mask&15u)==15u){protection_off();return;}
 now=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;taskENTER_CRITICAL();
 if(relay_wake&&device.hardware.verified&&!relay_fault){
  control_head=0;control_pending=0;
  for(i=0;i<4;i++)if(mask&(1u<<i)){unsigned tail=(control_head+control_pending)&3u;control_queue[tail].channel=(uint8_t)i;control_queue[tail].on=0;control_queue[tail].at=now;++control_pending;}
  if(control_pending){watchdog_relay_at=now;relay_busy=1;relay_desired=(uint8_t)(relay_desired&~mask);wake=1;}
 }
 taskEXIT_CRITICAL();if(wake)xSemaphoreGive(relay_wake);
}
void mttl_sdk_button_event(unsigned index,int event){unsigned on,lock_mask;if(index>4||!event)return;
 taskENTER_CRITICAL();lock_mask=device.settings.child_lock_mask;taskEXIT_CRITICAL();
 if(!sfn_child_lock_event_allowed(lock_mask,index,(unsigned)event))return;
 if(index==0&&event==2){taskENTER_CRITICAL();if(!energy_barrier_state){provision_requested=1;setup_led_edges=6;setup_led_at=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS-200u;}taskEXIT_CRITICAL();return;}
 taskENTER_CRITICAL();on=index?!(relay_desired&(1u<<(index-1))):!relay_desired;taskEXIT_CRITICAL();
 if(control_enqueue(index?index-1:4,on,1)){
#ifdef SFN_NATIVE_UP_ONLY
  taskENTER_CRITICAL();up_physical_pending=(uint8_t)(up_physical_pending|(1u<<index));taskEXIT_CRITICAL();
#endif
 }
}
/* Called from the existing Arduino loop, independently of network/flash work. */
void mttl_sdk_buttons_poll(void){unsigned i;uint32_t now;if(!buttons_ready)return;now=(uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;
 for(i=0;i<5;i++){int raw=gpio_read(&button_pins[i]);int event=i?mttl_button_input_poll(&button_inputs[i],raw==0||raw==1,raw==0,now):mttl_main_button_input_poll(&button_inputs[i],raw==0||raw==1,raw==0,now);if(event)mttl_sdk_button_event(i,event);}
 /* Existing 10 ms loop owns output; no sleeps, task, or relay bus writes. */
 {unsigned state;taskENTER_CRITICAL();state=published.state;taskEXIT_CRITICAL();
  if(setup_led_edges){if((uint32_t)(now-setup_led_at)>=200u){setup_led_at=now;--setup_led_edges;setup_led_on=setup_led_edges&1;gpio_write(&setup_led,setup_led_on);}}
  else if(state==MTTL_NET_AP||state==MTTL_NET_JOINING||state==MTTL_NET_DHCP){if((uint32_t)(now-setup_led_at)>=500u){setup_led_at=now;setup_led_on=!setup_led_on;gpio_write(&setup_led,setup_led_on);}}
  else if(setup_led_on){setup_led_on=0;gpio_write(&setup_led,0);}
 }
}
static __attribute__((noinline)) size_t config_snapshot(char *out,size_t cap){mttl_settings copy;int persistent;size_t n;taskENTER_CRITICAL();copy=device.settings;persistent=device.persistent;taskEXIT_CRITICAL();n=mttl_settings_json(&copy,persistent,out,cap);{volatile uint8_t *p=(volatile uint8_t *)&copy;size_t i;for(i=0;i<sizeof(copy);i++)p[i]=0;}return n;}
size_t mttl_sdk_device_json(char *out,size_t cap,int settings){size_t n;if(settings)return config_snapshot(out,cap);taskENTER_CRITICAL();n=device_size;if(n&&n<cap){memcpy(out,device_json,n+1);}else n=0;taskEXIT_CRITICAL();return n;}
int mttl_sdk_settings_persistent(void){return device.persistent!=0;}
int mttl_sdk_config_reserve(const uint8_t *body,size_t n,int keep_password){int ok=0;mttl_settings candidate;if(!mttl_settings_unpack(body,n,&candidate))return 0;taskENTER_CRITICAL();if((ap_ready||published.state==MTTL_NET_ONLINE)&&!occupied&&!config_occupied&&!energy_barrier_state){pending_config=candidate;config_keep_password=keep_password!=0;config_occupied=1;ok=1;}taskEXIT_CRITICAL();{volatile uint8_t *p=(volatile uint8_t *)&candidate;size_t i;for(i=0;i<sizeof(candidate);i++)p[i]=0;}return ok;}
void mttl_sdk_config_release(int accepted){taskENTER_CRITICAL();if(config_occupied){if(accepted)config_ready=1;else{memset(&pending_config,0,sizeof(pending_config));config_occupied=0;}}taskEXIT_CRITICAL();}
static mttl_lifecycle life;
static uint8_t scan_requested;
static uint8_t manual_setup;
static uint32_t wifi_retry_at;
static __attribute__((noinline)) int wifi_retry_allowed(unsigned flags,unsigned state,unsigned generation,uint32_t elapsed){return !(flags&1)&&(flags&2)&&state==MTTL_NET_AP&&(generation==1||elapsed>=15000u);}
static int wifi_retry_ready(uint32_t now){unsigned flags=(manual_setup?1:0)|(device.persistent&&device.settings.ssid[0]?2:0)|(life.web_started?4:0);return wifi_retry_allowed(flags,life.network.state,life.network.generation,(uint32_t)(now-wifi_retry_at))&&!occupied&&!config_occupied&&!energy_barrier_state&&!scan_requested&&!mttl_sdk_scan_pending();}
static char requested_ssid[33],requested_password[65];
static uint8_t ap_phase;
static uint8_t scan_failed;
static uint32_t scan_sequence;
int mttl_sdk_scan_request(void){int ok=0;if(mttl_sdk_scan_pending())return 0;taskENTER_CRITICAL();if((ap_ready||published.state==MTTL_NET_ONLINE)&&!occupied&&!config_occupied&&!energy_barrier_state&&!scan_requested){scan_requested=1;scan_failed=0;ok=1;}taskEXIT_CRITICAL();return ok;}
int mttl_sdk_scan_queued(void){int queued;taskENTER_CRITICAL();queued=scan_requested;taskEXIT_CRITICAL();return queued;}
int mttl_sdk_scan_failed(void){int failed;taskENTER_CRITICAL();failed=scan_failed;taskEXIT_CRITICAL();return failed;}
void mttl_sdk_status_snapshot(mttl_worker_status *out){taskENTER_CRITICAL();*out=published;taskEXIT_CRITICAL();}
static void wipe(char *p,unsigned size){volatile char *q=p;while(size--)*q++=0;}
static unsigned length(const char *p,unsigned max){unsigned n=0;while(n<=max&&p[n])++n;return n;}
static uint32_t clock_ms(void *context){(void)context;return (uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;}
/* Scratch exists only during serialization, on the existing 8 KiB worker
 * stack. noinline keeps it out of flash-save/configuration call chains. */
static __attribute__((noinline)) void publish_snapshots(uint32_t now){
 char buffer[2048];size_t n=mttl_device_status(&device,now,buffer,sizeof(buffer));
 mttl_sdk_memory_sample();
 taskENTER_CRITICAL();device_size=n<sizeof(device_json)?n:0;if(device_size)memcpy(device_json,buffer,n+1);taskEXIT_CRITICAL();
}
int mttl_sdk_wifi_reserve(const char *ssid,const char *password){
    unsigned a,b,i;if(!ssid||!password)return 0;a=length(ssid,32);b=length(password,64);if(!a||a>32||b>64)return 0;
    taskENTER_CRITICAL();if((!ap_ready&&published.state!=MTTL_NET_ONLINE)||occupied||config_occupied||energy_barrier_state){taskEXIT_CRITICAL();return 0;}occupied=1;
    for(i=0;i<=a;i++)requested_ssid[i]=ssid[i];for(i=0;i<=b;i++)requested_password[i]=password[i];
    taskEXIT_CRITICAL();return 1;
}
void mttl_sdk_wifi_release(int accepted){taskENTER_CRITICAL();if(occupied){if(accepted)ready=1;else{wipe(requested_ssid,33);wipe(requested_password,65);occupied=0;}}taskEXIT_CRITICAL();}
static int stop(void *p){
    (void)p;taskENTER_CRITICAL();ap_ready=0;taskEXIT_CRITICAL();
#ifdef SFN_NATIVE_UP_ONLY
    sfn_up_disconnect();
#endif
    if(mttl_sdk_scan_pending())return 0;
    mttl_device_tick(&device,identity,0,clock_ms(0),SFN_MQTT_IO);
#ifndef SFN_NATIVE_UP_ONLY
    if(device.mqtt.session.state!=MTTL_MQ_OFF)return 0;
#endif
    mttl_sdk_web_stop();if(!mttl_sdk_web_stopped())return 0;
    if(stop_phase==0){if(mttl_sdk_dhcp_busy()||!mttl_sdk_dhcp_stop())return 0;stop_phase=1;return 0;}
    if(mttl_sdk_dhcp_busy())return 0;
    if(!mttl_sdk_stop_radio())return 0;
    stop_phase=0;ap_phase=0;return 1;
}
static int ap(void *p){
    (void)p;
    if(!ap_phase){
        if(!mttl_sdk_start_ap("MTTL-W01-SETUP"))return 0;
        if(!mttl_sdk_ap_address())return 0;
        ap_phase=1;return -1;
    }
    return mttl_sdk_dhcp_busy()?-1:1;
}
static int join(void *p,const char *s,const char *key){(void)p;return mttl_sdk_join(s,key);}
static int associated(void *p){(void)p;return mttl_sdk_associated();}
static int dhcp(void *p){(void)p;return mttl_sdk_dhcp_start();}
static uint32_t ip(void *p){(void)p;return mttl_sdk_associated()?mttl_sdk_dhcp_poll():0;}
static int web(void *p){(void)p;return mttl_sdk_web_start();}
static int commit(void *p,const char *s,const char *key){unsigned i;mttl_settings candidate=device.settings;int changed;(void)p;
 changed=strcmp(candidate.ssid,s)||strcmp(candidate.wifi_password,key);
 memcpy(candidate.ssid,s,33);memcpy(candidate.wifi_password,key,65);
#ifdef SFN_NATIVE_UP_ONLY
 changed=sfn_backend_pin(&candidate,SFN_NATIVE_BACKEND_IPV4,SFN_NATIVE_BACKEND_PORT)||changed;
#endif
 if(changed||!device.persistent){int saved;if(!storage_begin()){wipe((char *)&candidate,sizeof(candidate));return 0;}saved=mttl_sdk_settings_save(&candidate);storage_end();if(saved!=1){wipe((char *)&candidate,sizeof(candidate));return 0;}}
 taskENTER_CRITICAL();device.settings=candidate;device.persistent=1;taskEXIT_CRITICAL();manual_setup=0;wipe((char *)&candidate,sizeof(candidate));
 (void)i;return 1;}
static const mttl_services services={0,stop,ap,join,associated,dhcp,ip,web,clock_ms,commit};
/* No additional task/socket owner. Service control between independent sensor
 * transactions, never within a bus frame or a NOR operation. */
static void meter_network_service(void){
 if(energy_barrier_state||life.network.state!=MTTL_NET_ONLINE)return;
 taskENTER_CRITICAL();device.control.known_mask=relay_known;device.control.on_mask=relay_on;device.control.fault=relay_fault;taskEXIT_CRITICAL();
 mttl_device_tick(&device,identity,1,clock_ms(0),SFN_MQTT_IO);
 mttl_device_tick(&device,identity,1,clock_ms(0),SFN_MQTT_IO);
#ifdef SFN_NATIVE_UP_ONLY
 sfn_up_service(1);sfn_up_service(1);sfn_up_physical_events();
#endif
 publish_snapshots(clock_ms(0));
}
static void worker(void *p){
    char mac[32];unsigned k,j=0;(void)p;
    mttl_sdk_memory_init();
    mttl_sdk_network_init();
#ifdef SFN_BOOT_GUARD
    (void)sfn_sdk_boot_guard_start(clock_ms(0));
#endif
    mttl_device_init(&device,1767225600ULL);
    sfn_schedule_sync_init(&schedule_sync);
#ifndef SFN_NATIVE_UP_ONLY
    mttl_sdk_mqtt_socket_init(&mqtt_socket);mttl_sdk_mqtt_socket_io(&mqtt_socket,&mqtt_io);
#else
    sfn_sdk_up_socket_init(&up_socket);sfn_sdk_up_socket_io(&up_socket,&up_io);
#endif
    mttl_sdk_relay_init(&device.hardware);
    {mttl_settings loaded;int result=mttl_sdk_settings_load(&loaded);if(result>=0){device.settings=loaded;device.persistent=result==1;}wipe((char *)&loaded,sizeof(loaded));}
    if(device.hardware.verified){device.control.known_mask=device.settings.relay_known;device.control.on_mask=device.settings.relay_on;}
    /* Restore indicators without pulsing latching relay coils. */
    if(device.hardware.verified&&!mttl_hardware_indicators(&device.hardware,device.control.known_mask,device.control.on_mask))device.hardware.verified=0;
    relay_control=device.control;relay_port=device.hardware;relay_known=relay_control.known_mask;relay_on=relay_control.on_mask;relay_desired=relay_on;
    relay_wake=xSemaphoreCreateBinary();
    if(relay_wake&&xTaskCreate(relay_task,"mttl_relay",384,0,tskIDLE_PRIORITY+2,0)==pdPASS)device.relay_request=mttl_sdk_control_request;
    else{if(relay_wake)vSemaphoreDelete(relay_wake);relay_wake=0;device.hardware.verified=0;}
    if(device.hardware.verified){static const uint8_t pins[5]={3,5,0x22,2,1};gpio_init(&setup_led,(PinName)0x23);gpio_mode(&setup_led,PullNone);gpio_write(&setup_led,0);gpio_dir(&setup_led,PIN_OUTPUT);for(k=0;k<5;k++){gpio_init(&button_pins[k],(PinName)pins[k]);gpio_mode(&button_pins[k],PullUp);gpio_dir(&button_pins[k],PIN_INPUT);mttl_button_input_init(&button_inputs[k],30);}buttons_ready=1;}
    mttl_sdk_energy_init(&device.settings,clock_ms(0));
    mttl_sdk_meter_set_service(meter_network_service);
    memset(mac,0,sizeof(mac));if(wifi_get_mac_address(mac)==RTW_SUCCESS){for(k=0;mac[k]&&j<12;k++){char c=mac[k];if(c>='A'&&c<='F')c=(char)(c+32);if((c>='0'&&c<='9')||(c>='a'&&c<='f'))identity[j++]=c;}if(j!=12)identity[0]=0;}
    mttl_lifecycle_init(&life);mttl_provision(&life.network,clock_ms(0));
    for(;;){
        int scan_now=0,config_now;
        watchdog_worker_at=clock_ms(0);watchdog_worker_seen=1;
        taskENTER_CRITICAL();device.control.known_mask=relay_known;device.control.on_mask=relay_on;device.control.fault=relay_fault;taskEXIT_CRITICAL();
#ifndef SFN_NATIVE_UP_ONLY
        /* Same MQTT socket owner, but bypass slow sensor/storage work while
         * relay completion is pending. Second step drains the state packet
         * queued by the first instead of leaving it until the next full loop. */
        if(!device.control.fault&&!energy_barrier_state&&life.network.state==MTTL_NET_ONLINE&&device.mqtt.session.state==MTTL_MQ_ONLINE&&
           (relay_busy||mttl_device_mqtt_pending(&device))){
            mttl_device_tick(&device,identity,1,clock_ms(0),&mqtt_io);
            mttl_device_tick(&device,identity,1,clock_ms(0),&mqtt_io);
            publish_snapshots(clock_ms(0));
            vTaskDelay(1);continue;
        }
#endif
        {int provision=0;taskENTER_CRITICAL();if(provision_requested&&!relay_busy&&!energy_barrier_state&&!occupied&&!config_occupied){provision=1;}taskEXIT_CRITICAL();
         if(provision&&storage_begin()){int reset;
          if(!device.control.fault){device.settings.relay_known=device.control.known_mask;device.settings.relay_on=device.control.on_mask;}
          reset=mttl_sdk_energy_reset(&device.settings,&device.samples,clock_ms(0));storage_end();
          if(reset){taskENTER_CRITICAL();provision_requested=0;taskEXIT_CRITICAL();manual_setup=1;mttl_provision(&life.network,clock_ms(0));}
         }}
        {uint8_t state;taskENTER_CRITICAL();state=energy_barrier_state;taskEXIT_CRITICAL();
         if(state){if(state==1){int saved;if(!device.control.fault){device.settings.relay_known=device.control.known_mask;device.settings.relay_on=device.control.on_mask;}saved=mttl_sdk_energy_flush(&device.settings);taskENTER_CRITICAL();if(energy_barrier_state==1)energy_barrier_state=saved?2:3;taskEXIT_CRITICAL();}vTaskDelay(20/portTICK_RATE_MS+1);continue;}}
        taskENTER_CRITICAL();config_now=config_ready;taskEXIT_CRITICAL();
        if(config_now&&storage_begin()){
            memcpy(pending_config.ssid,device.settings.ssid,33);memcpy(pending_config.wifi_password,device.settings.wifi_password,65);
            if(config_keep_password)memcpy(pending_config.mqtt_password,device.settings.mqtt_password,65);
            mttl_settings_keep_energy(&pending_config,&device.settings);
            /* Flash operations never run with scheduler/interrupts disabled. */
            if(mttl_settings_valid(&pending_config)&&mttl_sdk_settings_save(&pending_config)==1){
              int voltage_changed=memcmp(&device.settings.voltage_guard,&pending_config.voltage_guard,sizeof(pending_config.voltage_guard))!=0;
              if(memcmp(&device.settings.schedule,&pending_config.schedule,sizeof(pending_config.schedule)))schedule_state.primed=0;
              taskENTER_CRITICAL();device.settings=pending_config;++device.revision;device.persistent=1;
              if(voltage_changed){sfn_voltage_guard_init(&voltage_guard_state);voltage_guard_sample_seen=0;voltage_guard_sample_at=0;}
              taskEXIT_CRITICAL();device.protection.current_count=device.protection.power_count=0;}
            storage_end();
            taskENTER_CRITICAL();wipe((char *)&pending_config,sizeof(pending_config));config_ready=config_occupied=0;taskEXIT_CRITICAL();
        }
        taskENTER_CRITICAL();
        if(ready){manual_setup=1;if(mttl_wifi_submit(&life.network,(const uint8_t *)requested_ssid,length(requested_ssid,32),(const uint8_t *)requested_password,length(requested_password,64),clock_ms(0))){life.join_started=life.dhcp_started=0;}
            wipe(requested_ssid,33);wipe(requested_password,65);ready=occupied=0;ap_ready=0;}
        if(scan_requested){scan_now=1;scan_requested=0;}
        taskEXIT_CRITICAL();
        if(scan_now){int failed=(life.network.state!=MTTL_NET_AP&&life.network.state!=MTTL_NET_ONLINE)||!mttl_sdk_scan_start(++scan_sequence);taskENTER_CRITICAL();scan_failed=failed;taskEXIT_CRITICAL();}
        mttl_lifecycle_step(&life,&services);
#ifdef SFN_BOOT_GUARD
        sfn_sdk_boot_guard_tick(clock_ms(0),device.persistent&&device.settings.sfn_backend_ipv4&&device.up.state==SFN_UP_LINK_ONLINE);
#endif
        /* AP state after stop is enough: saved-network retry must not depend
         * on successful setup AP/HTTP startup. Explicit setup stays manual. */
        if(wifi_retry_ready(clock_ms(0))){
            wifi_retry_at=clock_ms(0);life.join_started=life.dhcp_started=0;
            mttl_wifi_submit(&life.network,(const uint8_t *)device.settings.ssid,length(device.settings.ssid,32),(const uint8_t *)device.settings.wifi_password,length(device.settings.wifi_password,64),clock_ms(0));
        }
        /* Radio initialization may be required before the SDK can read its MAC. */
        if(!identity[0]&&(life.network.state==MTTL_NET_AP||life.network.state==MTTL_NET_ONLINE)){memset(mac,0,sizeof(mac));j=0;if(wifi_get_mac_address(mac)==RTW_SUCCESS){for(k=0;mac[k]&&j<12;k++){char c=mac[k];if(c>='A'&&c<='F')c=(char)(c+32);if((c>='0'&&c<='9')||(c>='a'&&c<='f'))identity[j++]=c;}if(j!=12)identity[0]=0;}}
        mttl_device_tick(&device,identity,life.network.state==MTTL_NET_ONLINE,clock_ms(0),SFN_MQTT_IO);
#ifdef SFN_NATIVE_UP_ONLY
        sfn_up_service(life.network.state==MTTL_NET_ONLINE);sfn_up_physical_events();
#endif
        /* Pulse adapter has already released coils; save only a completed queue. */
        if(!device.control.fault&&!mttl_sdk_energy_relay_saved(device.control.known_mask,device.control.on_mask)&&relay_save_ready(clock_ms(0))&&(!relay_save_retry||(uint32_t)(clock_ms(0)-relay_save_attempt_at)>=5000u)&&storage_begin()){
            device.settings.relay_known=device.control.known_mask;device.settings.relay_on=device.control.on_mask;
            /* Publish completed command before the potentially slow NOR save.
             * This is runtime state confirmation, not flash-save confirmation. */
            publish_snapshots(clock_ms(0));
            relay_save_attempt_at=clock_ms(0);
            relay_save_retry=!mttl_sdk_energy_flush(&device.settings);
            storage_end();
        }
        mttl_sdk_ntp_tick(&device.ntp,life.network.state==MTTL_NET_ONLINE,clock_ms(0));
        {uint64_t time;unsigned on,mask,i;int valid=mttl_time_now(&device.ntp.time,clock_ms(0),&time);mttl_schedule_config active_schedule=device.settings.schedule;
#ifdef SFN_NATIVE_UP_ONLY
         if(valid)active_schedule.timezone=sfn_schedule_runtime_offset(&schedule_sync,&device.settings.sfn_schedule,(uint32_t)(time/1000u),active_schedule.timezone);
#endif
         mask=mttl_schedule_tick(&schedule_state,&active_schedule,valid,valid?time:0,&on);
         /* At most four commands, admitted atomically; never partly apply a
          * schedule batch when manual work has filled the bounded queue. */
         if(mask){unsigned count=0;for(i=0;i<4;i++)if(mask&(1u<<i))++count;
          taskENTER_CRITICAL();
          if(relay_wake&&device.hardware.verified&&!relay_fault&&!energy_barrier_state&&!occupied&&!config_occupied&&!provision_requested&&control_pending+count<=4&&!(on&mask&sfn_voltage_guard_blocked_mask(&voltage_guard_state,&device.settings.voltage_guard))){
           for(i=0;i<4;i++)if(mask&(1u<<i))(void)control_enqueue(i,!!(on&(1u<<i)),1);
           schedule_state.pending=0;
          }
          taskEXIT_CRITICAL();
         }}
        mttl_sdk_meter_tick(&device.samples,clock_ms(0));
        {unsigned voltage_event=0;uint32_t sample_at=device.samples.voltage.at;
         if(!voltage_guard_sample_seen||sample_at!=voltage_guard_sample_at){
          voltage_guard_sample_seen=1;voltage_guard_sample_at=sample_at;
          taskENTER_CRITICAL();voltage_event=sfn_voltage_guard_step(&voltage_guard_state,&device.settings.voltage_guard,device.samples.voltage.valid,device.samples.voltage.value,sample_at);taskEXIT_CRITICAL();
          if(voltage_event&(SFN_VOLTAGE_EVENT_TRIPPED_LOW|SFN_VOLTAGE_EVENT_TRIPPED_HIGH))protection_off_mask(device.settings.voltage_guard.protected_mask);
         }}
        if(mttl_sdk_meter_protection(&device.samples,&device.settings.protection,&device.protection))protection_off();
        if(storage_begin()){mttl_sdk_energy_tick(&device.settings,&device.samples,clock_ms(0));storage_end();}
        publish_snapshots(clock_ms(0));
        {
            mttl_worker_status current;unsigned i;int rssi=0;
            memset(&current,0,sizeof(current));
            current.hardware_ready=device.hardware.verified&&!device.control.fault;
            current.meter_initialized=mttl_sdk_meter_initialized();current.meter_reads=mttl_sdk_meter_reads();
            current.energy_restore_pending=(uint8_t)mttl_sdk_energy_restore_pending();
            current.energy_saved_mask=(uint8_t)mttl_sdk_energy_saved_mask();current.energy_loaded_mask=(uint8_t)mttl_sdk_energy_loaded_mask();current.energy_save_count=mttl_sdk_energy_save_count();current.energy_save_failures=mttl_sdk_energy_save_failures();
            current.heap_free_bytes=(uint32_t)xPortGetFreeHeapSize();current.heap_min_free_bytes=(uint32_t)xPortGetMinimumEverFreeHeapSize();current.worker_observed_stack_descent_bytes=mttl_sdk_memory_stack_descent();
            {unsigned mask;mttl_sdk_meter_energy_snapshot(current.energy_raw,&mask);current.energy_reads=(uint8_t)mask;}
            current.state=life.network.state;current.ip=life.network.state==MTTL_NET_AP?0xc0a80401u:life.network.ip;current.uptime_ms=clock_ms(0);
            if(life.network.state!=MTTL_NET_ONLINE)cached_rssi_valid=0;
            else if(!rssi_polled_at||(uint32_t)(current.uptime_ms-rssi_polled_at)>=2000){
                rssi_polled_at=current.uptime_ms;cached_rssi_valid=wifi_get_rssi(&rssi)==RTW_SUCCESS;cached_rssi=rssi;
            }
            current.rssi_valid=cached_rssi_valid;current.rssi=cached_rssi;
            for(i=0;i<33;i++)current.ssid[i]=life.network.state==MTTL_NET_ONLINE?device.settings.ssid[i]:0;
            if(identity[0])snprintf(current.mac,sizeof(current.mac),"%.2s:%.2s:%.2s:%.2s:%.2s:%.2s",identity,identity+2,identity+4,identity+6,identity+8,identity+10);
            current.time_valid=mttl_time_now(&device.ntp.time,current.uptime_ms,&current.unix_ms);
            if(current.time_valid&&life.network.state==MTTL_NET_ONLINE){uint32_t elapsed=current.uptime_ms-life.network.connected_at;if(elapsed<0x80000000u&&current.unix_ms>=elapsed)current.connected_unix_ms=current.unix_ms-elapsed;}
            taskENTER_CRITICAL();published=current;taskEXIT_CRITICAL();
        }
        taskENTER_CRITICAL();ap_ready=life.network.state==MTTL_NET_AP&&life.web_started;taskEXIT_CRITICAL();
        vTaskDelay(20/portTICK_RATE_MS+1);
    }
}
int mttl_sdk_worker_start(void){if(worker_started)return 1;if(xTaskCreate(worker,"mttl_network",1536,0,tskIDLE_PRIORITY+1,0)!=pdPASS)return 0;worker_started=1;return 1;}
