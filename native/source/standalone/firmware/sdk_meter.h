#ifndef MTTL_SDK_METER_H
#define MTTL_SDK_METER_H
#include "hardware_port.h"
#include "protection.h"
/* Experimental, no output-bus or relay access. GPIO activation opt-in. */
void mttl_sdk_meter_tick(mttl_hardware_samples *,uint32_t now);
/* Same worker owner; called only BETWEEN complete bus transactions. */
void mttl_sdk_meter_set_service(void (*service)(void));
unsigned mttl_sdk_meter_initialized(void);
unsigned mttl_sdk_meter_reads(void);
void mttl_sdk_meter_energy_snapshot(uint32_t raw[4],unsigned *mask);
/* Consumes only events from the preceding tick; cached values never count. */
unsigned mttl_sdk_meter_protection(const mttl_hardware_samples *,const mttl_protection_config *,mttl_protection_state *);
#endif
