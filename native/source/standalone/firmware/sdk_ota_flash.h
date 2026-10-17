#ifndef MTTL_SDK_OTA_FLASH_H
#define MTTL_SDK_OTA_FLASH_H
#include "ota_guard.h"
typedef struct {mttl_ota_plan plan;uint8_t verified;} mttl_sdk_ota_flash;
/* init deliberately does not authorize hardware IO. No runtime caller yet. */
void mttl_sdk_ota_flash_init(mttl_sdk_ota_flash *);
int mttl_sdk_ota_flash_read(mttl_sdk_ota_flash *,uint32_t,uint8_t *,size_t);
int mttl_sdk_ota_flash_write(mttl_sdk_ota_flash *,uint32_t,const uint8_t *,size_t);
int mttl_sdk_ota_flash_erase(mttl_sdk_ota_flash *,uint32_t);
#endif
