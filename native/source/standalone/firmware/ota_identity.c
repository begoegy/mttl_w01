#include "ota_identity.h"
#include "ota_hash.h"
#include <string.h>
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
uint32_t mttl_ota_identity(const mttl_ota_layout *l,const uint8_t *running,size_t n,uint32_t offset,
 int verified,mttl_ota_read_fn read,void *context){
 mttl_ota_plan plan;mttl_ota_hash hash;uint8_t wanted[32],digest[32],data[256];uint32_t found=0;unsigned slot,matches=0;
 if(!l||!running||!read||verified!=1||n<256||n>0x6a000||offset<24||offset>0x6a000-n||!mttl_ota_plan_make(l,l->first,44,&plan))return 0;
 mttl_ota_hash_init(&hash);mttl_ota_hash_update(&hash,running,n);mttl_ota_hash_finish(&hash,wanted);
 for(slot=0;slot<2;slot++){
  uint32_t base=slot?l->second:l->first,limit=slot?l->protected_start:l->second,body,pos=0;
  if(16+offset+n>limit-base)return 0;
  if(read(context,base,data,8)!=1)return 0;body=word(data);
  if(word(data+4)!=0x10006000||body<24||body>0x6a000||body+20>limit-base||offset+n>body)continue;
  mttl_ota_hash_init(&hash);
  while(pos<n){size_t take=n-pos;if(take>sizeof(data))take=sizeof(data);
   if(read(context,base+16+offset+pos,data,take)!=1){mttl_ota_hash_finish(&hash,digest);return 0;}
   mttl_ota_hash_update(&hash,data,take);pos+=(uint32_t)take;
  }
  mttl_ota_hash_finish(&hash,digest);
  if(!memcmp(wanted,digest,32)){found=base;matches++;}
 }
 return matches==1?found:0;
}
