#ifndef MTTL_PROTECTION_H
#define MTTL_PROTECTION_H
#include <stdint.h>
typedef struct {uint16_t temperature_c,current_ma,power_w;uint8_t enabled;} mttl_protection_config;
typedef struct {uint8_t current_count,power_count,error_mask,reason;} mttl_protection_state;
void mttl_protection_default(mttl_protection_config *);
int mttl_protection_valid(const mttl_protection_config *);
unsigned mttl_protection_temperature(mttl_protection_state *,const mttl_protection_config *,int valid,int32_t temperature_dc);
/* Called once per completed NEW measurement round, never from cached UI data.
 * valid bits: temperature=1, corrected total current=2, total power=4.
 * Temperature caller supplies maximum fresh channel temperature in deci-C.
 * Current is corrected mA; power is the existing aggregate in mW.
 * Returns reason bits requesting ALL OFF, not a lock. No output side effects. */
unsigned mttl_protection_round(mttl_protection_state *,const mttl_protection_config *,unsigned valid,int32_t temperature_dc,int32_t current_ma,int32_t power_mw);
#endif
