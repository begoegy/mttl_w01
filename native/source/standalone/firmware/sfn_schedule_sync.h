#ifndef SFN_SCHEDULE_SYNC_H
#define SFN_SCHEDULE_SYNC_H
#include <stdint.h>
#include "scheduler.h"
#include "sfn_schedule_meta.h"
typedef struct {
    sfn_schedule_meta clock;
    uint32_t clock_epoch;
    uint32_t generation;
    mttl_schedule_config staged;
    uint8_t clock_valid;
    uint8_t active;
    uint8_t expected;
    uint8_t received;
    uint8_t seen_mask;
} sfn_schedule_sync;
void sfn_schedule_sync_init(sfn_schedule_sync *);
int sfn_schedule_sync_clock(sfn_schedule_sync *,uint32_t epoch,int offset_minutes,uint32_t transition1_epoch,int transition1_offset_minutes,uint32_t transition2_epoch,int transition2_offset_minutes);
int sfn_schedule_sync_begin(sfn_schedule_sync *,uint32_t generation,unsigned count);
int sfn_schedule_sync_item(sfn_schedule_sync *,unsigned slot,unsigned outlet,unsigned repeat,unsigned days,unsigned date_days,unsigned minute,unsigned on);
int sfn_schedule_sync_commit(sfn_schedule_sync *,uint32_t generation,mttl_schedule_config *schedule,sfn_schedule_meta *meta);
int16_t sfn_schedule_runtime_offset(const sfn_schedule_sync *,const sfn_schedule_meta *persisted,uint32_t epoch,int16_t fallback);
#endif
