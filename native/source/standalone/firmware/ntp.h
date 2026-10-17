#ifndef MTTL_NTP_H
#define MTTL_NTP_H
#include <stddef.h>
#include <stdint.h>
/* Portable SNTP subset, no socket/DNS/NTP task. Token must be unpredictable
 * in the eventual adapter; verify UDP peer there. This is not authenticated NTP. */
int mttl_ntp_request(uint8_t packet[48],const uint8_t token[8]);
int mttl_ntp_response(const uint8_t *,size_t,const uint8_t token[8],uint64_t reference_unix_seconds,uint64_t *unix_ms);
typedef struct { uint64_t unix_ms;uint32_t tick;uint8_t valid; } mttl_time_anchor;
void mttl_time_set(mttl_time_anchor *,uint64_t unix_ms,uint32_t tick);
int mttl_time_now(const mttl_time_anchor *,uint32_t tick,uint64_t *unix_ms);
#endif
