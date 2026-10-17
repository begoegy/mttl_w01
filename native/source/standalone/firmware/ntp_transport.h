#ifndef MTTL_NTP_TRANSPORT_H
#define MTTL_NTP_TRANSPORT_H
#include "ntp_client.h"
/* Single owner; callbacks never retain buffers. open/close return 1 on success.
 * recv returns 0 when idle, -1 on failure, or full datagram length (including
 * truncated length > capacity). IPv4 is host-order, port is host-order.
 * close failure retains ownership and blocks opening another socket.
 * entropy must provide fresh cryptographic bytes, not a clock/counter. */
typedef struct {
    void *context;
    int (*open)(void *,uint32_t,uint16_t);
    int (*close)(void *);
    int (*send)(void *,const uint8_t *,size_t);
    int (*recv)(void *,uint8_t *,size_t,uint32_t *,uint16_t *);
    int (*entropy)(void *,uint8_t *,size_t);
} mttl_ntp_io;
typedef struct {uint32_t server,generation;uint8_t opened;} mttl_ntp_transport;
int mttl_ntp_transport_init(mttl_ntp_transport *,uint32_t server);
/* Returns 1 only when a valid time is accepted, 0 otherwise. */
int mttl_ntp_transport_tick(mttl_ntp_transport *,mttl_ntp_client *,const mttl_ntp_io *,int online,uint32_t now);
#endif
