#ifndef MTTL_OTA_HASH_H
#define MTTL_OTA_HASH_H
#include <stdint.h>
#include <stddef.h>
typedef union {uint32_t align;uint8_t opaque[128];} mttl_ota_hash;
void mttl_ota_hash_init(mttl_ota_hash *);
int mttl_ota_hash_update(mttl_ota_hash *,const uint8_t *,size_t);
int mttl_ota_hash_finish(mttl_ota_hash *,uint8_t digest[32]);
int mttl_ota_hash_parse(const char *,uint8_t digest[32]);
#endif
