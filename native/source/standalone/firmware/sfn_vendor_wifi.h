#ifndef SFN_VENDOR_WIFI_H
#define SFN_VENDOR_WIFI_H
#include <stddef.h>
#include <stdint.h>
#include "core.h"
#define SFN_VENDOR_WIFI_RECORD_ADDRESS 0x000FA000u
#define SFN_VENDOR_WIFI_RECORD_SIZE 0xB4u
#define SFN_VENDOR_WIFI_PROFILE_COUNT 2u
#define SFN_VENDOR_WIFI_PROFILE_SIZE 0x58u
#define SFN_VENDOR_WIFI_MARKER 0x00005A5Au
typedef struct {
    char ssid[33];
    char password[65];
} sfn_vendor_wifi_profile;
typedef struct {
    sfn_vendor_wifi_profile profiles[SFN_VENDOR_WIFI_PROFILE_COUNT];
    uint8_t count;
    uint8_t next;
} sfn_vendor_wifi_import;
int sfn_vendor_wifi_parse(sfn_vendor_wifi_import *state,const uint8_t *record,size_t size);
int sfn_vendor_wifi_submit_next(sfn_vendor_wifi_import *state,mttl_network *network,uint32_t now);
int sfn_vendor_wifi_pending(const sfn_vendor_wifi_import *state);
void sfn_vendor_wifi_clear(sfn_vendor_wifi_import *state);
#endif
