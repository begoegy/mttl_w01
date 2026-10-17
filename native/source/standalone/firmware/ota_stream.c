#include "ota_stream.h"
#include "ota_guard.h"
#include <string.h>
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int mttl_ota_stream_init(mttl_ota_stream *s,uint32_t size){if(!s)return 0;memset(s,0,sizeof(*s));s->expected=size;s->failed=size<44||size>0x70000;return !s->failed;}
int mttl_ota_stream_feed(mttl_ota_stream *s,const uint8_t *data,size_t n){
    size_t i;if(!s||s->failed)return 0;if(!data||!n||n>s->expected-s->received){s->failed=1;return 0;}
    for(i=0;i<n;i++){uint32_t at=s->received;if(at<40)s->prefix[at]=data[i];if(at>=4)s->sum+=s->tail[at%4];s->tail[at%4]=data[i];s->received++;}return 1;
}
int mttl_ota_stream_finish(mttl_ota_stream *s){
    static const uint8_t magic[20]={0x52,0x54,0x4b,0x57,0x69,0x6e,0,0xff,0,1,1,0,0x95,0x81,1,1,0,0,0,0};
    uint8_t last[4];uint32_t body,entry;unsigned i;if(!s||s->failed)return 0;
    if(s->received!=s->expected)goto fail;body=word(s->prefix);entry=word(s->prefix+16);
    if(body<24||body>0x6a000||body+20!=s->expected||word(s->prefix+4)!=0x10006000)goto fail;
    if(mttl_ota_signature(s->prefix+8)!=MTTL_OTA_EMPTY||memcmp(s->prefix+20,magic,20))goto fail;
    if(!(entry&1)||(entry&~1u)<0x10006018||(entry&~1u)>=0x10006000+body)goto fail;
    for(i=0;i<4;i++)last[i]=s->tail[(s->received+i)%4];if(s->sum!=word(last))goto fail;return 1;
fail:s->failed=1;return 0;
}
