#ifndef MTTL_OTA_SESSION_H
#define MTTL_OTA_SESSION_H
#include "ota_stage.h"
/* Trusted platform policy must prove active slot, geometry and management
 * authorization independently. This callback is not a proof implementation. */
typedef int (*mttl_ota_admit_fn)(void *,const mttl_ota_layout *,uint32_t,
                                const mttl_ota_plan *,uint32_t,const uint8_t [32]);
enum {MTTL_SESSION_FAILED=0,MTTL_SESSION_HEADER=1,MTTL_SESSION_STREAM=2,MTTL_SESSION_VERIFIED=3};
typedef struct {
 mttl_ota_stage stage;mttl_ota_layout layout;mttl_ota_stage_io io;
 mttl_ota_admit_fn admit;void *policy;
 uint32_t active,bytes,received;uint8_t prefix[40],sha[32],state;
} mttl_ota_session;
/* No IO until a valid complete prefix and explicit policy acceptance. */
int mttl_ota_session_init(mttl_ota_session *,const mttl_ota_layout *,uint32_t,uint32_t,
                          const uint8_t [32],const mttl_ota_stage_io *,mttl_ota_admit_fn,void *);
int mttl_ota_session_feed(mttl_ota_session *,const uint8_t *,size_t);
int mttl_ota_session_finish(mttl_ota_session *);
void mttl_ota_session_abort(mttl_ota_session *);
#endif
