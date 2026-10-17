#include "sfn_voltage_guard.h"
#include <string.h>

void sfn_voltage_guard_default(sfn_voltage_guard_config *c){
    if(!c)return;
    c->low_trip_dv=1800;
    c->high_trip_dv=2500;
    c->recover_low_dv=1900;
    c->recover_high_dv=2450;
    c->enabled=0;
    c->trip_samples=2;
    c->recover_samples=3;
}

int sfn_voltage_guard_valid(const sfn_voltage_guard_config *c){
    if(!c)return 0;
    if(c->enabled>1)return 0;
    if(c->low_trip_dv<1000||c->low_trip_dv>2300)return 0;
    if(c->high_trip_dv<1800||c->high_trip_dv>3000)return 0;
    if(c->low_trip_dv>=c->high_trip_dv)return 0;
    if(c->recover_low_dv<=c->low_trip_dv||c->recover_low_dv>c->recover_high_dv)return 0;
    if(c->recover_high_dv>=c->high_trip_dv)return 0;
    if(c->trip_samples<1||c->trip_samples>10)return 0;
    if(c->recover_samples<1||c->recover_samples>30)return 0;
    return 1;
}

void sfn_voltage_guard_init(sfn_voltage_guard_state *s){if(s)memset(s,0,sizeof(*s));}

int sfn_voltage_guard_allows_on(const sfn_voltage_guard_state *s){return s&&!s->fault;}

unsigned sfn_voltage_guard_step(sfn_voltage_guard_state *s,const sfn_voltage_guard_config *c,int valid,int32_t v){
    unsigned reason=0;
    if(!s||!sfn_voltage_guard_valid(c))return SFN_VOLTAGE_EVENT_NONE;
    if(!c->enabled){
        unsigned event=s->fault?SFN_VOLTAGE_EVENT_RECOVERED:SFN_VOLTAGE_EVENT_NONE;
        sfn_voltage_guard_init(s);
        return event;
    }
    if(!valid||v<0){
        s->trip_count=0;s->pending_reason=0;s->recover_count=0;
        return SFN_VOLTAGE_EVENT_NONE;
    }
    if(s->fault){
        if(v>=c->recover_low_dv&&v<=c->recover_high_dv){
            if(s->recover_count<c->recover_samples)++s->recover_count;
            if(s->recover_count>=c->recover_samples){
                s->fault=0;s->trip_count=0;s->recover_count=0;s->pending_reason=0;
                return SFN_VOLTAGE_EVENT_RECOVERED;
            }
        }else s->recover_count=0;
        return SFN_VOLTAGE_EVENT_NONE;
    }
    if(v<c->low_trip_dv)reason=SFN_VOLTAGE_EVENT_TRIPPED_LOW;
    else if(v>c->high_trip_dv)reason=SFN_VOLTAGE_EVENT_TRIPPED_HIGH;
    if(!reason){s->trip_count=0;s->pending_reason=0;return SFN_VOLTAGE_EVENT_NONE;}
    if(s->pending_reason!=reason){s->pending_reason=(uint8_t)reason;s->trip_count=1;}
    else if(s->trip_count<c->trip_samples)++s->trip_count;
    if(s->trip_count>=c->trip_samples){
        s->fault=1;s->last_trip_reason=(uint8_t)reason;s->trip_count=0;s->recover_count=0;s->pending_reason=0;
        return reason;
    }
    return SFN_VOLTAGE_EVENT_NONE;
}
