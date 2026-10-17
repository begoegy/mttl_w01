#ifndef MTTL_NTP_CLIENT_H
#define MTTL_NTP_CLIENT_H
#include "ntp.h"
typedef struct {
    mttl_time_anchor time;
    uint64_t reference;
    uint32_t started,delay,generation;
    uint8_t token[8],waiting,online,attempts;
} mttl_ntp_client;
/* All calls on one worker. Tokens supplied by verified entropy adapter.
 * Adapter must filter UDP source and cancel/drain the old socket on link loss. */
void mttl_ntp_client_init(mttl_ntp_client *,uint64_t reference);
void mttl_ntp_link(mttl_ntp_client *,int online,uint32_t now);
int mttl_ntp_due(mttl_ntp_client *,uint32_t now);
int mttl_ntp_begin(mttl_ntp_client *,uint32_t now,const uint8_t token[8],uint8_t packet[48],uint32_t *generation);
int mttl_ntp_accept(mttl_ntp_client *,uint32_t now,uint32_t generation,const uint8_t *,size_t);
void mttl_ntp_send_failed(mttl_ntp_client *,uint32_t now,uint32_t generation);
#endif
