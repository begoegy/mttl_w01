#ifndef MTTL_OTA_STAGE_H
#define MTTL_OTA_STAGE_H
#include "ota_readback.h"
#include "ota_stream.h"
#include "ota_hash.h"
typedef struct {
 void *context;
 int (*erase)(void *,uint32_t);
 int (*write)(void *,uint32_t,const uint8_t *,size_t);
 mttl_ota_read_fn read;
} mttl_ota_stage_io;
enum {MTTL_STAGE_FAILED=0,MTTL_STAGE_RECEIVING=1,MTTL_STAGE_VERIFIED=2};
typedef struct {
 mttl_ota_layout layout;mttl_ota_plan plan;mttl_ota_stage_io io;
 mttl_ota_stream stream;mttl_ota_hash hash;
 uint32_t active,offset;uint8_t expected[32],state;
} mttl_ota_stage;
/* Caller must independently prove geometry and active slot. Integrity is not authentication.
 * Erases the inactive range immediately; never writes activation signatures or resets. */
int mttl_ota_stage_begin(mttl_ota_stage *,const mttl_ota_layout *,uint32_t,uint32_t,
                         const uint8_t [32],const mttl_ota_stage_io *);
int mttl_ota_stage_feed(mttl_ota_stage *,const uint8_t *,size_t);
int mttl_ota_stage_finish(mttl_ota_stage *);
void mttl_ota_stage_abort(mttl_ota_stage *);
#endif
