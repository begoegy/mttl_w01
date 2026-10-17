#include "sdk_energy.h"
#include "sdk_meter.h"
#include "sdk_settings_flash.h"
#include "meter_convert.h"
#include <string.h>
static uint32_t saved_raw[4],saved_wh[4],attempt_at;
static uint8_t saved_valid,restore_pending,loaded_valid;
static uint8_t saved_relay_known,saved_relay_on;
static uint32_t save_count,save_failures;
static void remember(const mttl_settings *s){memcpy(saved_raw,s->energy_raw,sizeof(saved_raw));memcpy(saved_wh,s->energy_wh,sizeof(saved_wh));saved_valid=s->energy_valid;saved_relay_known=s->relay_known;saved_relay_on=s->relay_on;}
void mttl_sdk_energy_init(const mttl_settings *s,uint32_t now){remember(s);attempt_at=now;restore_pending=loaded_valid=s->energy_valid;save_count=save_failures=0;}
unsigned mttl_sdk_energy_restore_pending(void){return restore_pending;}
unsigned mttl_sdk_energy_saved_mask(void){return saved_valid;}
unsigned mttl_sdk_energy_loaded_mask(void){return loaded_valid;}
uint32_t mttl_sdk_energy_save_count(void){return save_count;}
uint32_t mttl_sdk_energy_save_failures(void){return save_failures;}
int mttl_sdk_energy_relay_saved(unsigned known,unsigned on){return saved_relay_known==known&&saved_relay_on==on;}
int mttl_sdk_energy_flush(const mttl_settings *s){
 if(saved_valid==s->energy_valid&&saved_relay_known==s->relay_known&&saved_relay_on==s->relay_on&&!memcmp(saved_raw,s->energy_raw,sizeof(saved_raw))&&!memcmp(saved_wh,s->energy_wh,sizeof(saved_wh)))return 1;
 if(mttl_sdk_settings_save(s)!=1){if(save_failures!=0xffffffffu)++save_failures;return 0;}remember(s);if(save_count!=0xffffffffu)++save_count;return 1;
}
int mttl_sdk_energy_reset(mttl_settings *s,mttl_hardware_samples *samples,uint32_t now){
 uint32_t raw[4],old_raw[4],old_wh[4];unsigned mask,ch;uint8_t valid=s->energy_valid;
 mttl_sdk_meter_energy_snapshot(raw,&mask);
 if((mask&15)!=15)return 0;
 for(ch=0;ch<4;ch++)if(raw[ch]>0xffffffu||(uint32_t)(now-samples->current[ch].at)>MTTL_SENSOR_MAX_AGE_MS)return 0;
 memcpy(old_raw,s->energy_raw,sizeof(old_raw));memcpy(old_wh,s->energy_wh,sizeof(old_wh));
 memcpy(s->energy_raw,raw,sizeof(raw));memset(s->energy_wh,0,sizeof(s->energy_wh));s->energy_valid=15;
 if(!mttl_sdk_energy_flush(s)){memcpy(s->energy_raw,old_raw,sizeof(old_raw));memcpy(s->energy_wh,old_wh,sizeof(old_wh));s->energy_valid=valid;return 0;}
 restore_pending=0;attempt_at=now;
 for(ch=0;ch<4;ch++)mttl_hardware_sample(&samples->meter[ch],1,0,now);
 mttl_hardware_sample(&samples->total_meter,1,0,now);return 1;
}
void mttl_sdk_energy_tick(mttl_settings *s,mttl_hardware_samples *samples,uint32_t now){
#ifdef MTTL_SENSOR_TEST
 uint32_t raw[4];unsigned mask,ch;int all=1;int32_t total=0;
 mttl_sdk_meter_energy_snapshot(raw,&mask);
 for(ch=0;ch<4;ch++){
  mttl_meter_checkpoint prior,next;int ok;
  prior.raw=s->energy_raw[ch];prior.value=s->energy_wh[ch];prior.valid=(s->energy_valid>>ch)&1;
  ok=((mask>>ch)&1)&&(uint32_t)(now-samples->current[ch].at)<=MTTL_SENSOR_MAX_AGE_MS&&mttl_meter_checkpoint_step(&prior,1,raw[ch],&next);
  if(ok){s->energy_raw[ch]=next.raw;s->energy_wh[ch]=next.value;s->energy_valid|=1u<<ch;restore_pending&=~(1u<<ch);}
  /* A cumulative meter retains its persisted last good value on read failure. */
  mttl_hardware_sample(&samples->meter[ch],(s->energy_valid>>ch)&1,(int32_t)s->energy_wh[ch],now);
  if(!(s->energy_valid&(1u<<ch)))all=0;else if(s->energy_wh[ch]>0x7fffffffu-(uint32_t)total)all=0;else total+=(int32_t)s->energy_wh[ch];
 }
 mttl_hardware_sample(&samples->total_meter,all,total,now);
 if((uint32_t)(now-attempt_at)>=300000u){attempt_at=now;(void)mttl_sdk_energy_flush(s);}
#else
 (void)s;(void)samples;(void)now;
#endif
}
