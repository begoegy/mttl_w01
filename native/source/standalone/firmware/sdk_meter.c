#include "sdk_meter.h"
#include "meter_bus.h"
#include "meter_convert.h"
#include "gpio_api.h"
#include "wait_api.h"
#include <string.h>
static gpio_t pins[4];static const uint8_t selectors[4]={0x25,0x24,0x20,0x21};
static uint8_t started,initialized,reads,next;static uint32_t polled_at;
static mttl_meter_power_state power_state;static mttl_meter_power_input power_input;
static uint8_t energy_reads;static uint32_t energy_raw[4];
static uint8_t protection_event,temperature_reads,round_current,round_power,last_channel;
unsigned mttl_sdk_meter_protection(const mttl_hardware_samples *s,const mttl_protection_config *c,mttl_protection_state *p){
 unsigned reason=0,valid=0,i;int32_t current=0,power=0;uint8_t event=protection_event;protection_event=0;
 if(!event)return 0;
 p->error_mask=(uint8_t)(c->enabled&((p->error_mask&6)|(temperature_reads==15?0:1)));
 if(event&1)reason=mttl_protection_temperature(p,c,(temperature_reads>>last_channel)&1,s->temperature[last_channel].value);
 if(event&2){p->error_mask=(uint8_t)(c->enabled&((p->error_mask&1)|(round_current==15?0:2)|(round_power==15?0:4)));
 if(reason)return reason;
 if(round_current==15){valid|=2;for(i=0;i<4;i++)current+=s->current[i].value;}
  if(round_power==15){valid|=4;for(i=0;i<4;i++)power+=s->power[i].value;}
  /* Temperature was evaluated at each fresh channel read, not again here. */
  {mttl_protection_config cp=*c;unsigned error=p->error_mask;cp.enabled&=6;reason|=mttl_protection_round(p,&cp,valid,0,current,power);p->error_mask=(uint8_t)error;}
 }return reason;
}
static void (*service_network)(void);
void mttl_sdk_meter_set_service(void (*service)(void)){service_network=service;}
static gpio_t *pin(uint8_t selector){unsigned i;for(i=0;i<4;i++)if(selectors[i]==selector)return &pins[i];return 0;}
static int line(void *p,uint8_t selector,int release){gpio_t *g=pin(selector);(void)p;if(!g)return 0;if(release)gpio_dir(g,PIN_INPUT);else{gpio_write(g,0);gpio_dir(g,PIN_OUTPUT);}return 1;}
static int sense(void *p,uint8_t selector){gpio_t *g=pin(selector);(void)p;return g?gpio_read(g):-1;}
/* Trial 5 us, NOT a measured interpretation of OEM delay argument 5. */
static void delay(void *p){(void)p;wait_us(5);}
static mttl_meter_bus bus={0,line,sense,delay,3};
static uint32_t be24(const uint8_t *p){return ((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];}
unsigned mttl_sdk_meter_initialized(void){return initialized;}
unsigned mttl_sdk_meter_reads(void){return reads;}
void mttl_sdk_meter_energy_snapshot(uint32_t raw[4],unsigned *mask){memcpy(raw,energy_raw,sizeof(energy_raw));*mask=energy_reads;}
void mttl_sdk_meter_tick(mttl_hardware_samples *samples,uint32_t now){
#ifdef MTTL_SENSOR_TEST
 unsigned i,ch;uint8_t data[9];int32_t temperature;uint32_t raw_current;int ok;
 protection_event=0;if(!samples)return;
 if(!started){for(i=0;i<4;i++){gpio_init(&pins[i],(PinName)selectors[i]);gpio_mode(&pins[i],PullNone);gpio_dir(&pins[i],PIN_INPUT);}mttl_meter_power_init(&power_state);power_input.factor=20;started=1;polled_at=now;return;}
 if((uint32_t)(now-polled_at)<2500)return;polled_at=now;ch=next;next=(next+1)&3;
 if(ch==0)round_current=round_power=0;
 last_channel=(uint8_t)ch;
 if(!(initialized&(1u<<ch))){if(mttl_meter_initialize(ch,mttl_meter_bus_write,&bus))initialized|=1u<<ch;if(ch==3)protection_event=2;return;}
 reads&=~(1u<<ch);memset(data,0,sizeof(data));ok=mttl_meter_bus_read(&bus,ch,0x47,data,3)&&mttl_meter_temperature(be24(data),&temperature);
 mttl_hardware_sample(&samples->temperature[ch],ok,ok?temperature:0,now);
 temperature_reads&=~(1u<<ch);if(ok)temperature_reads|=1u<<ch;protection_event=1;
 if(service_network)service_network();
 memset(data,0,sizeof(data));ok=mttl_meter_bus_read(&bus,ch,9,data,9);
 /* Display/MQTT baseline correction in mA; OEM power conversion keeps raw current. */
 raw_current=ok?be24(data+3):0;
 mttl_hardware_sample(&samples->current[ch],ok,raw_current>19?(int32_t)(raw_current-19):0,now);
 /* A single phase voltage, use channel 1 rather than adding voltages. */
 if(ch==0)mttl_hardware_sample(&samples->voltage,ok,ok?(int32_t)(be24(data)/100):0,now);
 if(ok)reads|=1u<<ch;
 if(ok)round_current|=1u<<ch;
 if(ok){power_input.raw[ch]=be24(data+6);power_input.current[ch]=be24(data+3);power_input.read_mask|=1u<<ch;}
 {mttl_meter_power_result result;int32_t total;
  if(ok&&power_input.read_mask==15&&mttl_meter_measured_power_step(&power_state,&power_input,&result)&&(result.power_valid&(1u<<ch)))
   {mttl_hardware_sample(&samples->power[ch],1,(result.power[ch]/10)*10,now);round_power|=1u<<ch;}
  /* Failed reads keep the last successful power, including beyond UI expiry.
   * reads remains the actual bus-read mask, not a freshness claim. */
  else if(samples->power[ch].valid)samples->power[ch].at=now;
  if(mttl_current_sum(samples->power,now,MTTL_SENSOR_MAX_AGE_MS,&total))mttl_hardware_sample(&samples->total_power,1,total,now);
  else mttl_hardware_sample(&samples->total_power,0,0,now);
 }
 if(service_network)service_network();
 memset(data,0,sizeof(data));energy_reads&=~(1u<<ch);if(mttl_meter_bus_read(&bus,ch,0x83,data,3)){energy_raw[ch]=be24(data);energy_reads|=1u<<ch;}
 {int32_t total;if(mttl_current_sum(samples->current,now,MTTL_SENSOR_MAX_AGE_MS,&total))mttl_hardware_sample(&samples->total_current,1,total,now);else mttl_hardware_sample(&samples->total_current,0,0,now);}
 /* sdk_energy turns this raw counter snapshot into persisted cumulative energy. */
 if(service_network)service_network();
 if(ch==3)protection_event|=2;
#else
 (void)samples;(void)now;
#endif
}
