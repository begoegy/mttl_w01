#ifndef MTTL_OTA_STREAM_H
#define MTTL_OTA_STREAM_H
#include <stdint.h>
#include <stddef.h>
typedef struct {uint32_t expected,received,sum;uint8_t prefix[40],tail[4],failed;} mttl_ota_stream;
int mttl_ota_stream_init(mttl_ota_stream *,uint32_t);
int mttl_ota_stream_feed(mttl_ota_stream *,const uint8_t *,size_t);
/* Format/additive checksum only. No SHA256/authentication or activation. */
int mttl_ota_stream_finish(mttl_ota_stream *);
#endif
