#ifndef MTTL_WATCHDOG_HEALTH_H
#define MTTL_WATCHDOG_HEALTH_H
#include <stdint.h>
/* Wrap-safe progress policy. Link/server connectivity is deliberately absent. */
/* A progress timestamp may be slightly newer than a sampled clock. Treat
 * that as fresh, not as an unsigned ~49-day stall. Intervals stay <2^31 ms. */
static inline int mttl_watchdog_fresh(uint32_t now,uint32_t last){return (int32_t)(now-last)<=30000;}
#endif
