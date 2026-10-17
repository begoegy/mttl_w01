#ifndef SFN_OTA_AUTH_H
#define SFN_OTA_AUTH_H
#include <stddef.h>
#include <stdint.h>
#define SFN_OTA_SIGNATURE_BYTES 64u
#define SFN_OTA_AUTH_MESSAGE_BYTES 56u
int sfn_ota_auth_message(uint32_t sequence,uint32_t bytes,const uint8_t fwr_sha256[32],uint8_t out[SFN_OTA_AUTH_MESSAGE_BYTES]);
int sfn_ota_auth_verify(uint32_t sequence,uint32_t bytes,const uint8_t fwr_sha256[32],const uint8_t *signature,size_t signature_bytes);
#endif
