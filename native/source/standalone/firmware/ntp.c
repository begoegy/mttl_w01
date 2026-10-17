#include "ntp.h"
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
int mttl_ntp_request(uint8_t packet[48],const uint8_t token[8]){
    unsigned i,nonzero=0;if(!packet||!token)return 0;for(i=0;i<8;i++)nonzero|=token[i];if(!nonzero)return 0;
    for(i=0;i<48;i++)packet[i]=0;packet[0]=0x23;for(i=0;i<8;i++)packet[40+i]=token[i];return 1;
}
int mttl_ntp_response(const uint8_t *p,size_t size,const uint8_t token[8],uint64_t reference,uint64_t *out){
    uint64_t ntp,ref,best=0,distance=~(uint64_t)0,candidate,d;uint32_t seconds,fraction;unsigned i,nonzero=0,version;int era,k;
    if(!p||!token||!out||size!=48||reference<1577836800ull||reference>4102444800ull)return 0;
    version=(p[0]>>3)&7;if((p[0]>>6)==3||(version!=3&&version!=4)||(p[0]&7)!=4||!p[1]||p[1]>15)return 0;
    for(i=0;i<8;i++){nonzero|=token[i];if(p[24+i]!=token[i])return 0;}if(!nonzero)return 0;
    if(!be32(p+32)&&!be32(p+36))return 0;
    seconds=be32(p+40);fraction=be32(p+44);if(!seconds&&!fraction)return 0;
    ref=reference+2208988800ull;era=(int)(ref>>32);
    for(k=era-1;k<=era+1;k++){if(k<0)continue;ntp=((uint64_t)(unsigned)k<<32)|seconds;
        if(ntp<2208988800ull)continue;candidate=ntp-2208988800ull;
        d=candidate>reference?candidate-reference:reference-candidate;
        if(d<distance){distance=d;best=candidate;}}
    /* Reject wildly wrong server time; caller's pivot must be trustworthy. */
    if(distance>315576000ull||best<1577836800ull||best>4102444800ull)return 0;
    *out=best*1000+(((uint64_t)fraction*1000)>>32);return 1;
}
void mttl_time_set(mttl_time_anchor *a,uint64_t ms,uint32_t tick){if(a){a->unix_ms=ms;a->tick=tick;a->valid=1;}}
int mttl_time_now(const mttl_time_anchor *a,uint32_t tick,uint64_t *out){
    uint32_t elapsed;if(!a||!a->valid||!out)return 0;elapsed=tick-a->tick;
    if(elapsed>=0x80000000u)return 0;*out=a->unix_ms+elapsed;return 1;
}
