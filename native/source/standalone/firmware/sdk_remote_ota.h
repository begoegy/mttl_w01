#ifndef SFN_SDK_REMOTE_OTA_H
#define SFN_SDK_REMOTE_OTA_H
#include <stdint.h>
/* backend_ipv4 must equal the backend compiled into this SFN Native build. */
int sfn_sdk_remote_ota_request(uint32_t backend_ipv4,uint32_t sequence,uint32_t bytes,const uint8_t sha256[32]);
void sfn_sdk_remote_ota_start_if_ready(int control_reply_drained);
int sfn_sdk_remote_ota_busy(void);
#endif
