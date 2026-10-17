#include "ota_session.h"
#include <string.h>
void mttl_ota_session_abort(mttl_ota_session *s){if(s){mttl_ota_stage_abort(&s->stage);s->state=MTTL_SESSION_FAILED;}}
int mttl_ota_session_init(mttl_ota_session *s,const mttl_ota_layout *l,uint32_t active,uint32_t bytes,
 const uint8_t sha[32],const mttl_ota_stage_io *io,mttl_ota_admit_fn admit,void *policy){
 mttl_ota_plan plan;if(!s)return 0;memset(s,0,sizeof(*s));
 if(!sha||!io||!io->erase||!io->write||!io->read||!admit||bytes<44||bytes>0x6a014||!mttl_ota_plan_make(l,active,bytes,&plan))return 0;
 s->layout=*l;s->active=active;s->bytes=bytes;s->io=*io;s->admit=admit;s->policy=policy;memcpy(s->sha,sha,32);s->state=MTTL_SESSION_HEADER;return 1;
}
int mttl_ota_session_feed(mttl_ota_session *s,const uint8_t *data,size_t n){
 size_t take;mttl_ota_plan plan;
 if(!s||(s->state!=MTTL_SESSION_HEADER&&s->state!=MTTL_SESSION_STREAM))return 0;
 if(!data||!n||n>s->bytes-s->received)goto fail;
 if(s->state==MTTL_SESSION_HEADER){
  take=40-s->received;if(take>n)take=n;memcpy(s->prefix+s->received,data,take);s->received+=(uint32_t)take;data+=take;n-=take;
  if(s->received<40)return 1;
  if(!mttl_ota_header_validate(s->prefix,40,s->bytes)||!mttl_ota_plan_make(&s->layout,s->active,s->bytes,&plan)||
   s->admit(s->policy,&s->layout,s->active,&plan,s->bytes,s->sha)!=1)goto fail;
  if(!mttl_ota_stage_begin(&s->stage,&s->layout,s->active,s->bytes,s->sha,&s->io)||
   !mttl_ota_stage_feed(&s->stage,s->prefix,40))goto fail;
  s->state=MTTL_SESSION_STREAM;
 }
 if(n){if(!mttl_ota_stage_feed(&s->stage,data,n))goto fail;s->received+=(uint32_t)n;}
 return 1;
fail:mttl_ota_session_abort(s);return 0;
}
int mttl_ota_session_finish(mttl_ota_session *s){
 if(s&&s->state==MTTL_SESSION_HEADER){mttl_ota_session_abort(s);return 0;}
 if(!s||s->state!=MTTL_SESSION_STREAM)return 0;
 if(s->received!=s->bytes||!mttl_ota_stage_finish(&s->stage)){mttl_ota_session_abort(s);return 0;}
 s->state=MTTL_SESSION_VERIFIED;return 1;
}
