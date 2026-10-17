#include "meter_probe.h"
#include <string.h>
static uint32_t be24(const uint8_t *p){return ((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];}
void mttl_meter_probe_init(mttl_meter_probe *p){if(p)memset(p,0,sizeof(*p));}
int mttl_meter_probe_tick(mttl_meter_probe *p,mttl_meter_read read,void *context,uint32_t now){
 uint8_t data[9];mttl_meter_raw *s;unsigned i;
 if(!p||!read)return 0;
 if(p->started&&(uint32_t)(now-p->polled_at)<250)return 0;
 p->started=1;p->polled_at=now;s=&p->channels[p->next];s->valid_mask=0;s->at=now;
 memset(data,0,sizeof(data));if(read(context,p->next,0x47,data,3)==1){s->temperature_raw=be24(data);s->valid_mask|=1;}
 memset(data,0,sizeof(data));if(read(context,p->next,0x09,data,9)==1){for(i=0;i<3;i++)s->measurement_raw[i]=be24(data+3*i);s->valid_mask|=2;}
 memset(data,0,sizeof(data));if(read(context,p->next,0x83,data,3)==1){s->energy_raw=be24(data);s->valid_mask|=4;}
 p->next=(p->next+1)%4;return 1;
}
int mttl_meter_probe_snapshot(const mttl_meter_probe *p,unsigned channel,uint32_t now,uint32_t stale_ms,mttl_meter_raw *out){
 if(!p||!out||channel>=4||!stale_ms||stale_ms>=0x80000000u)return 0;
 *out=p->channels[channel];if((uint32_t)(now-out->at)>stale_ms)out->valid_mask=0;return 1;
}
