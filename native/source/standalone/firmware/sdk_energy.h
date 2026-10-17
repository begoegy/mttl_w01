#ifndef MTTL_SDK_ENERGY_H
#define MTTL_SDK_ENERGY_H
#include "settings.h"
#include "hardware_port.h"
void mttl_sdk_energy_init(const mttl_settings *,uint32_t now);
void mttl_sdk_energy_tick(mttl_settings *,mttl_hardware_samples *,uint32_t now);
int mttl_sdk_energy_flush(const mttl_settings *);
int mttl_sdk_energy_reset(mttl_settings *,mttl_hardware_samples *,uint32_t now);
int mttl_sdk_energy_relay_saved(unsigned known,unsigned on);
unsigned mttl_sdk_energy_restore_pending(void);
unsigned mttl_sdk_energy_saved_mask(void);
unsigned mttl_sdk_energy_loaded_mask(void);
uint32_t mttl_sdk_energy_save_count(void);
uint32_t mttl_sdk_energy_save_failures(void);
#endif
