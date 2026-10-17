#include "sfn_voltage_policy.h"
int sfn_voltage_policy_decode(uint32_t flags,uint32_t thresholds,uint32_t timing,uint32_t recovery,sfn_voltage_guard_config *out){
    sfn_voltage_guard_config c;uint32_t known=1u|2u|(15u<<8);
    if(!out||flags&~known||(flags&2u)||recovery>65535u)return 0;
    sfn_voltage_guard_default(&c);
    c.enabled=(uint8_t)(flags&1u);c.protected_mask=(uint8_t)((flags>>8)&15u);
    c.low_trip_dv=(uint16_t)(thresholds&0xffffu);c.high_trip_dv=(uint16_t)(thresholds>>16);
    c.hysteresis_dv=(uint16_t)(timing&0xffffu);c.trip_hold_ds=(uint16_t)(timing>>16);
    c.recovery_seconds=(uint16_t)recovery;
    if(!sfn_voltage_guard_valid(&c))return 0;
    *out=c;
    return 1;
}
