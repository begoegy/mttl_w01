#include "ota_stage.h"
#include <string.h>
void mttl_ota_stage_abort(mttl_ota_stage *s){
 uint8_t digest[32];if(!s)return;
 if(s->state==MTTL_STAGE_RECEIVING)mttl_ota_hash_finish(&s->hash,digest);
 s->state=MTTL_STAGE_FAILED;
}
int mttl_ota_stage_begin(mttl_ota_stage *s,const mttl_ota_layout *l,uint32_t active,
 uint32_t bytes,const uint8_t expected[32],const mttl_ota_stage_io *io){
 uint32_t address;if(!s)return 0;memset(s,0,sizeof(*s));
 if(!l||!expected||!io||!io->erase||!io->write||!io->read||
 !mttl_ota_plan_make(l,active,bytes,&s->plan)||!mttl_ota_stream_init(&s->stream,bytes))return 0;
 s->layout=*l;s->active=active;s->io=*io;memcpy(s->expected,expected,32);
 mttl_ota_hash_init(&s->hash);s->state=MTTL_STAGE_RECEIVING;
 for(address=s->plan.target;address<s->plan.erase_end;address+=4096)
  if(s->io.erase(s->io.context,address)!=1){mttl_ota_stage_abort(s);return 0;}
 return 1;
}
int mttl_ota_stage_feed(mttl_ota_stage *s,const uint8_t *data,size_t n){
 size_t pos=0,k;
 if(!s||s->state!=MTTL_STAGE_RECEIVING)return 0;
 if(!data||!n||n>s->stream.expected-s->offset){mttl_ota_stage_abort(s);return 0;}
 /* Never stage a boot-selectable signature, even from a malformed upload. */
 for(k=0;k<n;k++)if(s->offset+k>=8&&s->offset+k<16&&data[k]!=255){
  mttl_ota_stage_abort(s);return 0;
 }
 /* Validate/hash before this chunk is written. A bad final format leaves only an inactive image. */
 if(!mttl_ota_stream_feed(&s->stream,data,n)||!mttl_ota_hash_update(&s->hash,data,n)){
  mttl_ota_stage_abort(s);return 0;
 }
 while(pos<n){size_t take=n-pos;if(take>256)take=256;
  if(s->io.write(s->io.context,s->plan.target+s->offset,data+pos,take)!=1){
   mttl_ota_stage_abort(s);return 0;
  }
  s->offset+=(uint32_t)take;pos+=take;
 }
 return 1;
}
int mttl_ota_stage_finish(mttl_ota_stage *s){
 uint8_t digest[32];int ok;unsigned i,provided=0;
 if(!s||s->state!=MTTL_STAGE_RECEIVING)return 0;
 if(s->offset!=s->stream.expected||!mttl_ota_stream_finish(&s->stream)){
  mttl_ota_stage_abort(s);return 0;
 }
 ok=mttl_ota_hash_finish(&s->hash,digest);s->state=MTTL_STAGE_FAILED;
 if(!ok)return 0;
 for(i=0;i<32;i++)provided|=s->expected[i];
 /* An all-zero expected hash requests internal receive/readback verification. */
 if(!provided)memcpy(s->expected,digest,32);
 if(!ok||memcmp(digest,s->expected,32)||
 !mttl_ota_readback(&s->layout,s->active,s->plan.target,s->offset,s->expected,s->io.read,s->io.context))return 0;
 s->state=MTTL_STAGE_VERIFIED;return 1;
}
