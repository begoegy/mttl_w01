#ifndef MTTL_OTA_READBACK_H
#define MTTL_OTA_READBACK_H
#include "ota_guard.h"
/* Return exactly 1 only after filling the entire requested range. */
typedef int (*mttl_ota_read_fn)(void *,uint32_t,uint8_t *,size_t);
int mttl_ota_readback(const mttl_ota_layout *,uint32_t active,uint32_t target,uint32_t file_bytes,
                      const uint8_t expected_sha256[32],mttl_ota_read_fn,void *context);
#endif
