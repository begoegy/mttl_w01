#include "ota_guard.h"
#include <string.h>
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int mttl_ota_signature(const uint8_t *p){
    unsigned i;int empty=1;if(!p)return MTTL_OTA_INVALID;
    for(i=0;i<8;i++)if(p[i]!=255)empty=0;
    if(empty)return MTTL_OTA_EMPTY;
    if(!memcmp(p,"81958711",8))return MTTL_OTA_NEW;
    if(!memcmp(p,"01958711",8))return MTTL_OTA_OLD;
    return MTTL_OTA_INVALID;
}
int mttl_ota_plan_make(const mttl_ota_layout *l,uint32_t active,uint32_t bytes,mttl_ota_plan *out){
    uint32_t target,limit,rounded;if(!l||!out||bytes<=20)return 0;
    if(!l->first||((l->first|l->second|l->protected_start|l->flash_size)&4095u))return 0;
    if(!(l->first<l->second&&l->second<l->protected_start&&l->protected_start<=l->flash_size))return 0;
    if(active!=l->first&&active!=l->second)return 0;
    target=active==l->first?l->second:l->first;limit=target==l->first?l->second:l->protected_start;
    if(bytes>limit-target)return 0;
    if(bytes>UINT32_MAX-4095u)return 0;
    rounded=(bytes+4095u)&~4095u;if(rounded>limit-target)return 0;
    out->target=target;out->erase_end=target+rounded;out->capacity=limit-target;return 1;
}
int mttl_ota_header_validate(const uint8_t *p,size_t prefix,uint32_t n){
    static const uint8_t tail[20]={0x52,0x54,0x4b,0x57,0x69,0x6e,0,0xff,0,1,1,0,0x95,0x81,1,1,0,0,0,0};
    uint32_t body,entry;
    if(!p||prefix<40||n<44||n>0x70000)return 0;
    body=word(p);if(body<24||body>0x6a000||body+20u!=n||word(p+4)!=0x10006000)return 0;
    if(mttl_ota_signature(p+8)!=MTTL_OTA_EMPTY||memcmp(p+20,tail,20))return 0;
    entry=word(p+16);if(!(entry&1)||(entry&~1u)<0x10006018||(entry&~1u)>=0x10006000+body)return 0;
    return 1;
}
int mttl_ota_bin_validate(const uint8_t *p,size_t n){
    uint32_t sum=0;size_t i;
    if(n>0x70000||!mttl_ota_header_validate(p,n,(uint32_t)n))return 0;
    for(i=0;i<n-4;i++)sum+=p[i];return sum==word(p+n-4);
}
int mttl_ota_commit_init(mttl_ota_commit *c,const mttl_ota_layout *l,uint32_t active,uint32_t target,int verified){
    mttl_ota_plan p;if(!c)return 0;memset(c,0,sizeof(*c));c->failed=1;
    if(verified!=1||!mttl_ota_plan_make(l,active,44,&p)||target!=p.target)return 0;
    c->active=active;c->target=target;c->failed=0;return 1;
}
int mttl_ota_commit_action(const mttl_ota_commit *c,uint32_t *address,uint32_t *value){
    if(!c||!address||!value||c->failed||c->step>=3)return 0;
    *address=(c->step==2?c->active:c->target)+(c->step==1?12:8);
    *value=c->step==0?0x35393138u:c->step==1?0x31313738u:0x35393130u;return 1;
}
int mttl_ota_commit_ack(mttl_ota_commit *c,int ok,uint32_t readback){
    uint32_t a,v;if(!mttl_ota_commit_action(c,&a,&v))return 0;
    if(ok!=1||readback!=v){c->failed=1;return 0;}c->step++;return 1;
}
int mttl_ota_commit_done(const mttl_ota_commit *c){return c&&!c->failed&&c->step==3;}
