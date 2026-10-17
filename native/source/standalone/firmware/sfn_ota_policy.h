#ifndef SFN_OTA_POLICY_H
#define SFN_OTA_POLICY_H
#include <stddef.h>
#include <stdint.h>
int sfn_ota_sequence_allowed(uint32_t current_sequence,uint32_t target_sequence);
size_t sfn_ota_profile_marker(char *out,size_t cap,uint32_t sequence);
#endif
