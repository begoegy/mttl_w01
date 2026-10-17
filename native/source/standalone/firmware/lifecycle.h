#ifndef MTTL_LIFECYCLE_H
#define MTTL_LIFECYCLE_H
#include "core.h"
/* Single network worker; callbacks must have bounded execution. A successful
 * stop means ALL tasks/sockets/scan contexts are quiescent, not just radio off.
 * join starts work; completion/IP are polled later. No HTTP-handler calls. */
typedef struct {
    void *context;
    int (*stop)(void *);
    int (*ap)(void *); /* 1 fully ready, 0 failed, -1 setup pending */
    int (*join)(void *, const char *, const char *);
    int (*associated)(void *); /* 1 joined, 0 pending, -1 failed */
    int (*dhcp)(void *);
    uint32_t (*ip)(void *);
    int (*web)(void *);
    uint32_t (*clock)(void *); /* refreshed after operations */
    mttl_commit_fn commit;
} mttl_services;
typedef struct {
    mttl_network network;
    uint32_t applied_generation;
    uint32_t waiting_since;
    uint8_t waiting;
    uint8_t ap_started, join_started, dhcp_started, web_started;
} mttl_lifecycle;
void mttl_lifecycle_init(mttl_lifecycle *life);
void mttl_lifecycle_step(mttl_lifecycle *life, const mttl_services *services);
#endif
