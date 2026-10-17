#ifndef MTTL_SCHEDULER_H
#define MTTL_SCHEDULER_H
#include <stdint.h>
#define MTTL_SCHEDULE_COUNT 8
#define MTTL_SCHEDULE_BYTES 50
/* mode: 0 empty, 1 daily, 2 weekly (Mon bit0), 3 day-of-month, 4 last day.
 * High bit of mode enables the rule. Fixed-offset local time, no DST. */
typedef struct {uint16_t minute;uint8_t mode,day,channel,on;} mttl_schedule_rule;
typedef struct {int16_t timezone; mttl_schedule_rule rules[8];} mttl_schedule_config;
typedef struct {int32_t high_minute;uint8_t primed,pending,on;} mttl_schedule_state;
void mttl_schedule_default(mttl_schedule_config *);
int mttl_schedule_valid(const mttl_schedule_config *);
void mttl_schedule_pack(const mttl_schedule_config *,uint8_t out[50]);
int mttl_schedule_unpack(const uint8_t in[50],mttl_schedule_config *);
/* Returns affected channel mask and desired ON mask. Caller submits through
 * normal relay owner and clears pending after atomic queue acceptance. Pending
 * work can retry only inside its original minute. Later rule wins conflicts.
 * No replay after
 * reboot, invalid clock, clock jumps or backward correction. */
unsigned mttl_schedule_tick(mttl_schedule_state *,const mttl_schedule_config *,int valid,uint64_t unix_ms,unsigned *on);
#endif
