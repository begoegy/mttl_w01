#include "ntp_client.h"
#include "deadline.h"
static void retry(mttl_ntp_client *c,uint32_t now){unsigned i;c->waiting=0;c->started=now;c->delay=c->attempts<3?5000:60000;for(i=0;i<8;i++)c->token[i]=0;}
void mttl_ntp_client_init(mttl_ntp_client *c,uint64_t reference){unsigned i;uint8_t *p=(uint8_t *)c;for(i=0;i<sizeof(*c);i++)p[i]=0;c->reference=reference;}
void mttl_ntp_link(mttl_ntp_client *c,int online,uint32_t now){
    unsigned i;if(c->online==(online!=0))return;c->online=online!=0;c->waiting=0;++c->generation;c->attempts=0;c->started=now;c->delay=0;for(i=0;i<8;i++)c->token[i]=0;
}
int mttl_ntp_due(mttl_ntp_client *c,uint32_t now){
    if(!c->online)return 0;
    if(c->waiting){if(!mttl_remaining(c->started,now,3000))retry(c,now);return 0;}
    return !mttl_remaining(c->started,now,c->delay);
}
int mttl_ntp_begin(mttl_ntp_client *c,uint32_t now,const uint8_t token[8],uint8_t packet[48],uint32_t *generation){
    unsigned i;if(!generation||!mttl_ntp_due(c,now)||!mttl_ntp_request(packet,token))return 0;
    for(i=0;i<8;i++)c->token[i]=token[i];c->waiting=1;c->started=now;++c->generation;if(c->attempts<3)++c->attempts;*generation=c->generation;return 1;
}
int mttl_ntp_accept(mttl_ntp_client *c,uint32_t now,uint32_t generation,const uint8_t *packet,size_t size){
    uint64_t ms,reference=c->reference;unsigned i;
    if(!c->online||!c->waiting||generation!=c->generation)return 0;
    if(!mttl_remaining(c->started,now,3000)){retry(c,now);return 0;}
    if(mttl_time_now(&c->time,now,&ms))reference=ms/1000;
    if(!mttl_ntp_response(packet,size,c->token,reference,&ms))return 0;
    mttl_time_set(&c->time,ms,now);c->reference=ms/1000;c->waiting=0;c->attempts=0;c->started=now;c->delay=3600000;
    for(i=0;i<8;i++)c->token[i]=0;return 1;
}
void mttl_ntp_send_failed(mttl_ntp_client *c,uint32_t now,uint32_t generation){if(c->waiting&&generation==c->generation)retry(c,now);}
