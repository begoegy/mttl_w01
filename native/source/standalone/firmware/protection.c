#include "protection.h"
void mttl_protection_default(mttl_protection_config *c){c->temperature_c=90;c->current_ma=16000;c->power_w=3000;c->enabled=7;}
int mttl_protection_valid(const mttl_protection_config *c){return c&&c->temperature_c>=50&&c->temperature_c<=90&&c->current_ma>=1000&&c->current_ma<=16000&&c->power_w>=100&&c->power_w<=3000&&c->enabled<=7;}
unsigned mttl_protection_temperature(mttl_protection_state *s,const mttl_protection_config *c,int valid,int32_t t){
 unsigned reason=(c->enabled&1)&&valid&&t>(int32_t)c->temperature_c*10?1:0;
 if(reason){s->reason=1;s->current_count=s->power_count=0;}return reason;
}
unsigned mttl_protection_round(mttl_protection_state *s,const mttl_protection_config *c,unsigned valid,int32_t t,int32_t i,int32_t p){
 unsigned reason=0;if(!s||!mttl_protection_valid(c))return 0;
 if(t<0)valid&=~1u;if(i<0)valid&=~2u;if(p<0)valid&=~4u;
 s->error_mask=(uint8_t)(c->enabled&~valid&7);
 if((c->enabled&valid&1)&&t>(int32_t)c->temperature_c*10)reason|=1;
 if((c->enabled&valid&2)&&i>c->current_ma){if(s->current_count<2)++s->current_count;}else s->current_count=0;
 if((c->enabled&valid&4)&&p>(int32_t)c->power_w*1000){if(s->power_count<2)++s->power_count;}else s->power_count=0;
 if(s->current_count==2)reason|=2;if(s->power_count==2)reason|=4;
 if(reason){s->reason=(uint8_t)reason;s->current_count=s->power_count=0;}
 return reason;
}
