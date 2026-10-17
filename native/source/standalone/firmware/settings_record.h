#ifndef MTTL_SETTINGS_RECORD_H
#define MTTL_SETTINGS_RECORD_H
#include <stddef.h>
#include <stdint.h>
/* Portable record format only. No flash addresses, erase or program calls. */
#define MTTL_SETTINGS_SLOT 2048
int mttl_settings_encode(uint8_t *slot,const uint8_t *body,size_t size,uint32_t sequence);
int mttl_settings_validate(const uint8_t *slot,uint32_t *sequence,size_t *size);
/* Restore device extensions to an in-memory v2/v3 envelope; physical body stays
 * v1 so older firmware can load Wi-Fi/MQTT. */
int mttl_settings_extract(const uint8_t *,uint8_t *,size_t,size_t *);
/* 0/1 chosen slot, -1 neither valid, -2 conflicting sequences. */
int mttl_settings_choose(const uint8_t *a,const uint8_t *b);
#endif
