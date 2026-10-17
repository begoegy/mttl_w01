#include "sfn_voltage_guard.h"
#include <string.h>

void sfn_voltage_guard_default(sfn_voltage_guard_config *c){
    if(!c)return;
    c->low_trip_dv=1800;
    c->high_trip_dv=2500;
    c->hysteresis_dv=100;
    c->trip_hold_ds=100;       /* 10 s: current physically unverified sample floor. */
    c->recovery_seconds=20;
    c->enabled=0;
    c->protected_mask=15;
}
int sfn_voltage_guard_valid(const sfn_voltage_guard_config *c){
    uint32_t lo,hi;
    if(!c||c->enabled>1)return 0;
    if(c->low_trip_dv<1000||c->low_trip_dv>2300)return 0;
    if(c->high_trip_dv<2100||c->high_trip_dv>3000)return 0;
    if(c->low_trip_dv+100u>c->high_trip_dv)return 0;
    if(c->hysteresis_dv<10||c->hysteresis_dv>200)return 0;
    lo=(uint32_t)c->low_trip_dv+c->hysteresis_dv;
    hi=(uint32_t)c->high_trip_dv-c->hysteresis_dv;
    if(lo>hi)return 0;
    /* Current native capability is deliberately honest about the ~10 s meter cadence. */
    if(c->trip_hold_ds<100||c->trip_hold_ds>300)return 0;
    if(c->recovery_seconds<10||c->recovery_seconds>3600)return 0;
    if(!c->protected_mask||c->protected_mask>15)return 0;
    return 1;
}
void sfn_voltage_guard_init(sfn_voltage_guard_state *s){if(s)memset(s,0,sizeof(*s));}
int sfn_voltage_guard_allows_on(const sfn_voltage_guard_state *s){return s&&!s->fault;}
uint8_t sfn_voltage_guard_blocked_mask(const sfn_voltage_guard_state *s,const sfn_voltage_guard_config *c){return s&&c&&s->fault?c->protected_mask:0;}
unsigned sfn_voltage_guard_step(sfn_voltage_guard_state *s,const sfn_voltage_guard_config *c,int valid,int32_t v,uint32_t at){
    unsigned reason=0;uint32_t need;
    if(!s||!sfn_voltage_guard_valid(c))return SFN_VOLTAGE_EVENT_NONE;
    if(!c->enabled){unsigned e=s->fault?SFN_VOLTAGE_EVENT_RECOVERED:0;sfn_voltage_guard_init(s);return e;}
    if(!valid||v<0){s->pending_active=0;s->pending_reason=0;s->recovery_active=0;return SFN_VOLTAGE_EVENT_NONE;}
    if(s->fault){
        int32_t low=(int32_t)c->low_trip_dv+c->hysteresis_dv;
        int32_t high=(int32_t)c->high_trip_dv-c->hysteresis_dv;
        if(v>=low&&v<=high){
            if(!s->recovery_active){s->recovery_active=1;s->recovery_since_ms=at;return SFN_VOLTAGE_EVENT_NONE;}
            need=(uint32_t)c->recovery_seconds*1000u;
            if((uint32_t)(at-s->recovery_since_ms)>=need){s->fault=0;s->pending_active=0;s->pending_reason=0;s->recovery_active=0;return SFN_VOLTAGE_EVENT_RECOVERED;}
        }else s->recovery_active=0;
        return SFN_VOLTAGE_EVENT_NONE;
    }
    if(v<c->low_trip_dv)reason=SFN_VOLTAGE_EVENT_TRIPPED_LOW;
    else if(v>c->high_trip_dv)reason=SFN_VOLTAGE_EVENT_TRIPPED_HIGH;
    if(!reason){s->pending_active=0;s->pending_reason=0;return SFN_VOLTAGE_EVENT_NONE;}
    if(!s->pending_active||s->pending_reason!=reason){s->pending_active=1;s->pending_reason=(uint8_t)reason;s->pending_since_ms=at;return SFN_VOLTAGE_EVENT_NONE;}
    need=(uint32_t)c->trip_hold_ds*100u;
    if((uint32_t)(at-s->pending_since_ms)>=need){s->fault=1;s->last_trip_reason=(uint8_t)reason;s->pending_active=0;s->pending_reason=0;s->recovery_active=0;return reason;}
    return SFN_VOLTAGE_EVENT_NONE;
}
