#ifndef MTTL_OTA_IDENTITY_H
#define MTTL_OTA_IDENTITY_H
#include "ota_readback.h"
/* Read-only matching, not boot-slot proof. Caller must prove immutable running
 * bytes correspond to this Image2 body offset. Ambiguous/failed reads return 0. */
uint32_t mttl_ota_identity(const mttl_ota_layout *,const uint8_t *,size_t,uint32_t,
                          int range_verified,mttl_ota_read_fn,void *);
#endif
