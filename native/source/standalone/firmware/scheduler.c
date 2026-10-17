#include "scheduler.h"
void mttl_schedule_default(mttl_schedule_config *c){unsigned i;c->timezone=540;for(i=0;i<8;i++){c->rules[i].minute=0;c->rules[i].mode=c->rules[i].day=c->rules[i].channel=c->rules[i].on=0;}}
int mttl_schedule_valid(const mttl_schedule_config *c){unsigned i;if(!c||c->timezone< -720||c->timezone>840||c->timezone%15)return 0;
 for(i=0;i<8;i++){const mttl_schedule_rule *r=&c->rules[i];unsigned mode=r->mode&127;
  if(mode>4||r->minute>=1440||r->channel>4||r->on>1)return 0;
  if(!mode){if(r->mode||r->minute||r->day||r->channel||r->on)return 0;}
  else if(mode==2){if(!r->day||r->day>127)return 0;}
  else if(mode==3){if(!r->day||r->day>31)return 0;}
  else if(r->day)return 0;
 }return 1;
}
void mttl_schedule_pack(const mttl_schedule_config *c,uint8_t out[50]){unsigned i;out[0]=(uint8_t)c->timezone;out[1]=(uint8_t)((uint16_t)c->timezone>>8);for(i=0;i<8;i++){const mttl_schedule_rule *r=&c->rules[i];unsigned p=2+6*i;out[p]=(uint8_t)r->minute;out[p+1]=(uint8_t)(r->minute>>8);out[p+2]=r->mode;out[p+3]=r->day;out[p+4]=r->channel;out[p+5]=r->on;}}
int mttl_schedule_unpack(const uint8_t in[50],mttl_schedule_config *c){unsigned i;c->timezone=(int16_t)((uint16_t)in[0]|((uint16_t)in[1]<<8));for(i=0;i<8;i++){mttl_schedule_rule *r=&c->rules[i];unsigned p=2+6*i;r->minute=(uint16_t)in[p]|((uint16_t)in[p+1]<<8);r->mode=in[p+2];r->day=in[p+3];r->channel=in[p+4];r->on=in[p+5];}return mttl_schedule_valid(c);}
static unsigned month_days(unsigned year,unsigned month){static const uint8_t days[12]={31,28,31,30,31,30,31,31,30,31,30,31};return days[month-1]+(month==2&&year%4==0&&(year%100!=0||year%400==0));}
unsigned mttl_schedule_tick(mttl_schedule_state *s,const mttl_schedule_config *c,int valid,uint64_t ms,unsigned *on){
 int32_t minute;uint32_t days,left;unsigned year=1970,month=1,day,weekday,i,mask=0,value=0;*on=0;
 if(!valid||ms<1577836800000ULL||ms>4102444800000ULL){s->pending=0;return 0;}
 minute=(int32_t)(ms/60000)+c->timezone;
 if(!s->primed){s->primed=1;s->pending=0;s->high_minute=minute;return 0;}
 if(minute<s->high_minute){s->pending=0;return 0;}
 if(minute==s->high_minute){*on=s->on;return s->pending;}
 s->pending=0;
 {int contiguous=minute==s->high_minute+1;s->high_minute=minute;if(!contiguous)return 0;}
 days=(uint32_t)(minute/1440);weekday=(days+3)%7;left=days;
 while(left>=(uint32_t)(365+(year%4==0&&(year%100!=0||year%400==0)))){left-=365+(year%4==0&&(year%100!=0||year%400==0));++year;}
 while(left>=month_days(year,month)){left-=month_days(year,month);++month;}day=left+1;
 for(i=0;i<8;i++){const mttl_schedule_rule *r=&c->rules[i];unsigned mode=r->mode&127,bits;
  if(!(r->mode&128)||r->minute!=(unsigned)(minute%1440))continue;
  if(mode==2&&!(r->day&(1u<<weekday)))continue;
  if(mode==3&&r->day!=day)continue;
  if(mode==4&&day!=month_days(year,month))continue;
  if(!mode)continue;bits=r->channel==4?15:1u<<r->channel;mask|=bits;value=(value&~bits)|(r->on?bits:0);
 }s->pending=(uint8_t)mask;s->on=(uint8_t)value;*on=value;return mask;
}
