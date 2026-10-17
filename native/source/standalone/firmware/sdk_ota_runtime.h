#ifndef MTTL_SDK_OTA_RUNTIME_H
#define MTTL_SDK_OTA_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
/* Single HTTP owner. Only decoded Image2 BIN; no bootloader/factory writes. */
uint32_t mttl_sdk_ota_probe(void);
int mttl_sdk_ota_begin(uint32_t,const uint8_t [32]);
int mttl_sdk_ota_feed(const uint8_t *,size_t);
int mttl_sdk_ota_finish(void);
int mttl_sdk_ota_restore_version(void);
void mttl_sdk_ota_abort(void);
#endif
