#include "settings.h"
static size_t length(const char *s,size_t cap){size_t i;for(i=0;i<cap&&s[i];i++){}return i;}
static int utf8(const char *s,size_t cap,int required){
    size_t n=length(s,cap),i=0;uint32_t v,min;unsigned extra,j;
    if(n==cap||(!n&&required))return 0;
    while(i<n){unsigned char c=(unsigned char)s[i++];
        if(c<128){if(c<32||c==127)return 0;continue;}
        if(c>=0xc2&&c<=0xdf){extra=1;v=c&31;min=128;}
        else if(c>=0xe0&&c<=0xef){extra=2;v=c&15;min=2048;}
        else if(c>=0xf0&&c<=0xf4){extra=3;v=c&7;min=65536;}
        else return 0;
        if(i+extra>n)return 0;
        for(j=0;j<extra;j++){c=(unsigned char)s[i++];if((c&0xc0)!=0x80)return 0;v=(v<<6)|(c&63);}
        if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return 0;
    }return 1;
}
static int ascii(const char *s,size_t cap,int host){
    size_t n=length(s,cap),i;unsigned char c;if(n==cap)return 0;
    for(i=0;i<n;i++){c=(unsigned char)s[i];
        if(host){if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c==':'||c=='['||c==']'))return 0;}
        else if(c<33||c>126||c=='+'||c=='#')return 0;
    }return 1;
}
int mttl_settings_utf8(const char *s,size_t cap){return s&&cap&&utf8(s,cap,0);}
void mttl_settings_default(mttl_settings *s){
    size_t i;const char name[]="MTTL-W01";unsigned char *p=(unsigned char *)s;
    for(i=0;i<sizeof(*s);i++)p[i]=0;
    for(i=0;i<sizeof(name);i++)s->device[i]=name[i];
    for(i=0;i<4;i++){s->channel[i][0]='S';s->channel[i][1]='W';s->channel[i][2]=' ';s->channel[i][3]=(char)('1'+i);}
    s->port=1883;s->topic[0]='m';s->topic[1]='t';s->topic[2]='t';s->topic[3]='l';
    mttl_protection_default(&s->protection);
    mttl_schedule_default(&s->schedule);
    sfn_voltage_guard_default(&s->voltage_guard);s->child_lock_mask=0;
}
int mttl_settings_valid(const mttl_settings *s){
    size_t i,n,k;if(!s||s->language>1||s->mqtt_enabled>1||!s->port||s->energy_valid>15||!mttl_protection_valid(&s->protection)||!mttl_schedule_valid(&s->schedule)||!sfn_voltage_guard_valid(&s->voltage_guard)||!sfn_child_lock_valid(s->child_lock_mask))return 0;
    for(i=0;i<4;i++)if(s->energy_raw[i]>0xffffffu||s->energy_wh[i]>0x7fffffffu)return 0;
    if(!utf8(s->device,65,1))return 0;for(i=0;i<4;i++)if(!utf8(s->channel[i],65,1))return 0;
    /* SSID is an opaque byte string; scan roundtrip need not be valid UTF-8. */
    n=length(s->ssid,33);k=length(s->wifi_password,65);
    if(n==33||k==65||(!n&&k)||(k&&k<8))return 0;
    for(i=0;i<n;i++)if((unsigned char)s->ssid[i]<32||(unsigned char)s->ssid[i]==127)return 0;
    for(i=0;i<k;i++){unsigned char c=(unsigned char)s->wifi_password[i];if(c<32||c>126)return 0;
        if(k==64&&!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return 0;}
    if(!ascii(s->broker,254,1)||!ascii(s->topic,65,0)||!utf8(s->mqtt_user,65,0)||!utf8(s->mqtt_password,65,0))return 0;
    return !s->mqtt_enabled||(s->broker[0]&&s->topic[0]);
}
static int field(uint8_t *out,size_t cap,size_t *used,const char *s,size_t bound){
    size_t n=length(s,bound),i;if(n==bound||*used+1+n>cap)return 0;
    out[(*used)++]=(uint8_t)n;for(i=0;i<n;i++)out[(*used)++]=(uint8_t)s[i];return 1;
}
size_t mttl_settings_pack(const mttl_settings *s,uint8_t *out,size_t cap){
    size_t n=5,i,needed=85;if(!out||!mttl_settings_valid(s))return 0;
    needed+=length(s->device,65);for(i=0;i<4;i++)needed+=length(s->channel[i],65);
    needed+=length(s->ssid,33)+length(s->wifi_password,65)+length(s->broker,254)+length(s->mqtt_user,65)+length(s->mqtt_password,65)+length(s->topic,65);
    if(cap<needed)return 0; /* No partial credential output on short buffer. */
    out[0]=6;out[1]=s->language;out[2]=s->mqtt_enabled;out[3]=(uint8_t)s->port;out[4]=(uint8_t)(s->port>>8);
    if(!field(out,cap,&n,s->device,65))return 0;
    for(i=0;i<4;i++)if(!field(out,cap,&n,s->channel[i],65))return 0;
    if(!field(out,cap,&n,s->ssid,33)||!field(out,cap,&n,s->wifi_password,65)||!field(out,cap,&n,s->broker,254)||!field(out,cap,&n,s->mqtt_user,65)||!field(out,cap,&n,s->mqtt_password,65)||!field(out,cap,&n,s->topic,65))return 0;
    out[n++]=s->protection.enabled;out[n++]=(uint8_t)s->protection.temperature_c;out[n++]=(uint8_t)(s->protection.temperature_c>>8);
    out[n++]=(uint8_t)s->protection.current_ma;out[n++]=(uint8_t)(s->protection.current_ma>>8);
    out[n++]=(uint8_t)s->protection.power_w;out[n++]=(uint8_t)(s->protection.power_w>>8);mttl_schedule_pack(&s->schedule,out+n);n+=50;
    out[n++]=s->child_lock_mask;out[n++]=s->voltage_guard.enabled;out[n++]=s->voltage_guard.trip_samples;out[n++]=s->voltage_guard.recover_samples;
    out[n++]=(uint8_t)s->voltage_guard.low_trip_dv;out[n++]=(uint8_t)(s->voltage_guard.low_trip_dv>>8);
    out[n++]=(uint8_t)s->voltage_guard.high_trip_dv;out[n++]=(uint8_t)(s->voltage_guard.high_trip_dv>>8);
    out[n++]=(uint8_t)s->voltage_guard.recover_low_dv;out[n++]=(uint8_t)(s->voltage_guard.recover_low_dv>>8);
    out[n++]=(uint8_t)s->voltage_guard.recover_high_dv;out[n++]=(uint8_t)(s->voltage_guard.recover_high_dv>>8);return n;
}
static int take(const uint8_t *body,size_t size,size_t *used,char *out,size_t cap){
    size_t n,i;if(*used>=size)return 0;n=body[(*used)++];if(n>=cap||n>size-*used)return 0;
    for(i=0;i<n;i++){if(!body[*used])return 0;out[i]=(char)body[(*used)++];}out[n]=0;return 1;
}
int mttl_settings_unpack(const uint8_t *body,size_t size,mttl_settings *out){
    mttl_settings tmp;size_t n=5,i;int result=0;volatile uint8_t *wipe=(volatile uint8_t *)&tmp;if(!body||!out||size<5||size>MTTL_SETTINGS_BODY_MAX||(body[0]!=1&&body[0]!=4&&body[0]!=5&&body[0]!=6))return 0;
    mttl_settings_default(&tmp);tmp.language=body[1];tmp.mqtt_enabled=body[2];tmp.port=(uint16_t)body[3]|((uint16_t)body[4]<<8);
    if(!take(body,size,&n,tmp.device,65))goto done;for(i=0;i<4;i++)if(!take(body,size,&n,tmp.channel[i],65))goto done;
    if(!take(body,size,&n,tmp.ssid,33)||!take(body,size,&n,tmp.wifi_password,65)||!take(body,size,&n,tmp.broker,254)||!take(body,size,&n,tmp.mqtt_user,65)||!take(body,size,&n,tmp.mqtt_password,65)||!take(body,size,&n,tmp.topic,65))goto done;
    if(body[0]>=4){if(size-n!=(body[0]==6?69:(body[0]==5?57:7)))goto done;tmp.protection.enabled=body[n++];tmp.protection.temperature_c=(uint16_t)body[n]|((uint16_t)body[n+1]<<8);n+=2;tmp.protection.current_ma=(uint16_t)body[n]|((uint16_t)body[n+1]<<8);n+=2;tmp.protection.power_w=(uint16_t)body[n]|((uint16_t)body[n+1]<<8);n+=2;}
    if(body[0]>=5){if(!mttl_schedule_unpack(body+n,&tmp.schedule))goto done;n+=50;}
    if(body[0]==6){tmp.child_lock_mask=body[n++];tmp.voltage_guard.enabled=body[n++];tmp.voltage_guard.trip_samples=body[n++];tmp.voltage_guard.recover_samples=body[n++];
      tmp.voltage_guard.low_trip_dv=(uint16_t)body[n]|((uint16_t)body[n+1]<<8);n+=2;
      tmp.voltage_guard.high_trip_dv=(uint16_t)body[n]|((uint16_t)body[n+1]<<8);n+=2;
      tmp.voltage_guard.recover_low_dv=(uint16_t)body[n]|((uint16_t)body[n+1]<<8);n+=2;
      tmp.voltage_guard.recover_high_dv=(uint16_t)body[n]|((uint16_t)body[n+1]<<8);n+=2;}
    if(n!=size||!mttl_settings_valid(&tmp))goto done;
    *out=tmp;result=1;
done:for(i=0;i<sizeof(tmp);i++)wipe[i]=0;return result;
}
void mttl_settings_keep_energy(mttl_settings *to,const mttl_settings *from){
    unsigned i;for(i=0;i<4;i++){to->energy_raw[i]=from->energy_raw[i];to->energy_wh[i]=from->energy_wh[i];}to->energy_valid=from->energy_valid;
    to->relay_known=from->relay_known;to->relay_on=from->relay_on;
}
size_t mttl_settings_storage_pack(const mttl_settings *s,uint8_t *out,size_t cap){
    size_t n,i,j;if(!s||!out||cap<38||s->relay_known>15||(s->relay_on&~s->relay_known))return 0;
    n=mttl_settings_pack(s,out+3,cap-38);if(!n)return 0;
    out[0]=3;out[1]=(uint8_t)n;out[2]=(uint8_t)(n>>8);out[3+n]=s->energy_valid;
    for(i=0;i<8;i++){uint32_t v=i<4?s->energy_raw[i]:s->energy_wh[i-4];for(j=0;j<4;j++)out[4+n+4*i+j]=(uint8_t)(v>>(8*j));}
    out[n+36]=s->relay_known;out[n+37]=s->relay_on;return n+38;
}
int mttl_settings_storage_unpack(const uint8_t *body,size_t size,mttl_settings *out){
    mttl_settings tmp;size_t n,i,j;int ok=0;volatile uint8_t *wipe=(volatile uint8_t *)&tmp;
    if(!body||!out||!size)return 0;
    if(body[0]==1||body[0]==4||body[0]==5||body[0]==6)return mttl_settings_unpack(body,size,out);
    if((body[0]!=2&&body[0]!=3)||size<(body[0]==3?43:41)||size>MTTL_SETTINGS_BODY_MAX)return 0;
    n=(size_t)body[1]|((size_t)body[2]<<8);if(n!=size-(body[0]==3?38:36))return 0;
    if(!mttl_settings_unpack(body+3,n,&tmp))goto done;
    tmp.energy_valid=body[3+n];
    if(body[0]==3){tmp.relay_known=body[n+36];tmp.relay_on=body[n+37];if(tmp.relay_known>15||(tmp.relay_on&~tmp.relay_known))goto done;}
    for(i=0;i<8;i++){uint32_t v=0;for(j=0;j<4;j++)v|=(uint32_t)body[4+n+4*i+j]<<(8*j);if(i<4)tmp.energy_raw[i]=v;else tmp.energy_wh[i-4]=v;}
    if(!mttl_settings_valid(&tmp))goto done;*out=tmp;ok=1;
done:for(i=0;i<sizeof(tmp);i++)wipe[i]=0;return ok;
}
