#ifndef MTTL_SDK_NTP_H
#define MTTL_SDK_NTP_H
#include "ntp_client.h"
void mttl_sdk_ntp_tick(mttl_ntp_client *,int online,uint32_t now);
#endif
