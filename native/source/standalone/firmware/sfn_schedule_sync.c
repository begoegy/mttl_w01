#include "sfn_schedule_sync.h"
#include <string.h>
#define SFN_EPOCH_MIN 1577836800u
#define SFN_EPOCH_MAX 4102444800u
static int offset_ok(int x){return x>=-840&&x<=840;}
void sfn_schedule_sync_init(sfn_schedule_sync *s){if(s)memset(s,0,sizeof(*s));}
int sfn_schedule_sync_clock(sfn_schedule_sync *s,uint32_t epoch,int offset,uint32_t t1,int o1,uint32_t t2,int o2){
    sfn_schedule_meta m;
    if(!s||epoch<SFN_EPOCH_MIN||epoch>SFN_EPOCH_MAX||!offset_ok(offset)||!offset_ok(o1)||!offset_ok(o2))return 0;
    if(t1&&(t1<=epoch||t1>SFN_EPOCH_MAX))return 0;
    if(t2&&(t2<=(t1?t1:epoch)||t2>SFN_EPOCH_MAX))return 0;
    sfn_schedule_meta_default(&m);m.valid=1;m.offset_minutes=(int16_t)offset;m.transition1_epoch=t1;m.transition1_offset_minutes=(int16_t)o1;m.transition2_epoch=t2;m.transition2_offset_minutes=(int16_t)o2;
    if(!sfn_schedule_meta_valid(&m))return 0;
    s->clock=m;s->clock_epoch=epoch;s->clock_valid=1;s->active=0;return 1;
}
int sfn_schedule_sync_begin(sfn_schedule_sync *s,uint32_t generation,unsigned count){
    if(!s||!s->clock_valid||generation>0x7fffffffu||count>8)return 0;
    mttl_schedule_default(&s->staged);s->staged.timezone=s->clock.offset_minutes;s->generation=generation;s->expected=(uint8_t)count;s->received=0;s->seen_mask=0;s->active=1;return 1;
}
int sfn_schedule_sync_item(sfn_schedule_sync *s,unsigned slot,unsigned outlet,unsigned repeat,unsigned days,unsigned date_days,unsigned minute,unsigned on){
    mttl_schedule_rule *r;
    if(!s||!s->active||slot>=8||slot>=s->expected||(s->seen_mask&(1u<<slot))||outlet>4||minute>=1440||on>1)return 0;
    /* Native gate intentionally supports recurring Daily/Weekly only. Once needs a durable UTC date representation first. */
    if(repeat==1){if(days||date_days!=0xffffu)return 0;}
    else if(repeat==2){if(!days||days>0x7fu||date_days!=0xffffu)return 0;}
    else return 0;
    r=&s->staged.rules[slot];r->minute=(uint16_t)minute;r->mode=(uint8_t)(0x80u|repeat);r->day=(uint8_t)(repeat==2?days:0);r->channel=(uint8_t)(outlet?outlet-1:4);r->on=(uint8_t)on;
    if(!mttl_schedule_valid(&s->staged)){memset(r,0,sizeof(*r));return 0;}
    s->seen_mask=(uint8_t)(s->seen_mask|(1u<<slot));++s->received;return 1;
}
int sfn_schedule_sync_commit(sfn_schedule_sync *s,uint32_t generation,mttl_schedule_config *schedule,sfn_schedule_meta *meta){
    sfn_schedule_meta m;
    if(!s||!schedule||!meta||!s->active||generation!=s->generation||s->received!=s->expected)return 0;
    if(s->expected&&s->seen_mask!=(uint8_t)((1u<<s->expected)-1u))return 0;
    if(!mttl_schedule_valid(&s->staged))return 0;
    m=s->clock;m.generation=generation;m.count=s->expected;m.valid=1;
    if(!sfn_schedule_meta_valid(&m))return 0;
    *schedule=s->staged;*meta=m;s->active=0;return 1;
}
int16_t sfn_schedule_runtime_offset(const sfn_schedule_sync *s,const sfn_schedule_meta *persisted,uint32_t epoch,int16_t fallback){
    if(s&&s->clock_valid)return sfn_schedule_offset_at(&s->clock,epoch);
    if(persisted&&persisted->valid&&sfn_schedule_meta_valid(persisted))return sfn_schedule_offset_at(persisted,epoch);
    return fallback;
}
