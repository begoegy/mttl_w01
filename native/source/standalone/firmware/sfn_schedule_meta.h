#ifndef SFN_SCHEDULE_META_H
#define SFN_SCHEDULE_META_H
#include <stdint.h>
typedef struct {
    uint32_t generation;
    uint32_t transition1_epoch;
    uint32_t transition2_epoch;
    int16_t offset_minutes;
    int16_t transition1_offset_minutes;
    int16_t transition2_offset_minutes;
    uint8_t count;
    uint8_t valid;
} sfn_schedule_meta;
void sfn_schedule_meta_default(sfn_schedule_meta *);
int sfn_schedule_meta_valid(const sfn_schedule_meta *);
int16_t sfn_schedule_offset_at(const sfn_schedule_meta *,uint32_t epoch);
#endif
