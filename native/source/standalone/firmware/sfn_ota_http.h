#ifndef SFN_OTA_HTTP_H
#define SFN_OTA_HTTP_H
#include <stddef.h>
#include <stdint.h>
#include "ota_hash.h"
typedef struct {uint8_t key[16];uint32_t offset;} sfn_fwr_decoder;
typedef struct {mttl_ota_hash hash;uint8_t expected[32];uint32_t expected_bytes,received;uint8_t active,failed;} sfn_ota_integrity;
void sfn_fwr_decoder_init(sfn_fwr_decoder *);
int sfn_fwr_decode(sfn_fwr_decoder *,uint8_t *,size_t);
int sfn_ota_integrity_init(sfn_ota_integrity *,uint32_t expected_bytes,const uint8_t expected_sha256[32]);
int sfn_ota_integrity_feed(sfn_ota_integrity *,const uint8_t *,size_t);
int sfn_ota_integrity_finish(sfn_ota_integrity *);
/* 1 complete+valid, 0 incomplete, -1 invalid. */
int sfn_ota_http_header(const uint8_t *,size_t,uint32_t *content_length,size_t *body_offset);
#endif
