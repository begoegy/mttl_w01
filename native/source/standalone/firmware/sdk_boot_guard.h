#ifndef SFN_SDK_BOOT_GUARD_H
#define SFN_SDK_BOOT_GUARD_H
#include <stdint.h>
int sfn_sdk_boot_guard_start(uint32_t now_ms);
void sfn_sdk_boot_guard_tick(uint32_t now_ms,int sfn_online);
int sfn_sdk_boot_guard_available(void);
#endif
