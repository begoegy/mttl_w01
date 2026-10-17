#include "sfn_vendor_wifi.h"
#include <string.h>
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int ssid_byte(uint8_t c){return c>=32u&&c!=127u;}
static int password_byte(uint8_t c){return c>=32u&&c<=126u;}
static int profile_parse(sfn_vendor_wifi_profile *out,const uint8_t *p){
    unsigned i;uint32_t password_length;uint8_t ssid_length;
    if(!out||!p)return 0;
    ssid_length=p[0];
    password_length=le32(p+0x50);
    if(!ssid_length||ssid_length>32u||password_length>32u)return 0;
    if(p[1u+ssid_length]!=0)return 0;
    for(i=0;i<ssid_length;i++)if(!ssid_byte(p[1u+i])||p[1u+i]==0)return 0;
    if(password_length){
        if(password_length<8u||p[0x2cu+password_length]!=0)return 0;
        for(i=0;i<password_length;i++)if(!password_byte(p[0x2cu+i])||p[0x2cu+i]==0)return 0;
    }else if(p[0x2c]!=0)return 0;
    memset(out,0,sizeof(*out));memcpy(out->ssid,p+1,ssid_length);if(password_length)memcpy(out->password,p+0x2c,password_length);return 1;
}
void sfn_vendor_wifi_clear(sfn_vendor_wifi_import *state){volatile uint8_t *p;size_t n;if(!state)return;p=(volatile uint8_t *)state;n=sizeof(*state);while(n--)*p++=0;}
int sfn_vendor_wifi_parse(sfn_vendor_wifi_import *state,const uint8_t *record,size_t size){
    unsigned slot;sfn_vendor_wifi_profile candidate;if(!state)return 0;sfn_vendor_wifi_clear(state);
    if(!record||size!=SFN_VENDOR_WIFI_RECORD_SIZE||le32(record)==0xffffffffu||le32(record+0xb0)!=SFN_VENDOR_WIFI_MARKER)return 0;
    for(slot=0;slot<SFN_VENDOR_WIFI_PROFILE_COUNT;slot++){
        if(!profile_parse(&candidate,record+slot*SFN_VENDOR_WIFI_PROFILE_SIZE))continue;
        if(state->count&&strcmp(candidate.ssid,state->profiles[0].ssid)==0&&strcmp(candidate.password,state->profiles[0].password)==0){memset(&candidate,0,sizeof(candidate));continue;}
        state->profiles[state->count++]=candidate;memset(&candidate,0,sizeof(candidate));
    }
    return state->count;
}
int sfn_vendor_wifi_pending(const sfn_vendor_wifi_import *state){return state&&state->next<state->count;}
int sfn_vendor_wifi_submit_next(sfn_vendor_wifi_import *state,mttl_network *network,uint32_t now){
    sfn_vendor_wifi_profile *p;size_t a=0,b=0;int ok;if(!sfn_vendor_wifi_pending(state)||!network)return 0;p=&state->profiles[state->next];
    while(a<32u&&p->ssid[a])a++;
    while(b<64u&&p->password[b])b++;
    ok=mttl_wifi_submit(network,(const uint8_t *)p->ssid,a,(const uint8_t *)p->password,b,now);
    if(ok){volatile uint8_t *q=(volatile uint8_t *)p;size_t n=sizeof(*p);while(n--)*q++=0;state->next++;}
    return ok;
}
