#include "sfn_up_protocol.h"
#include <stdio.h>
#include <string.h>

static int clean(const char *line,size_t size,char out[129]){
    size_t n=0,i;if(!line||!out||!size||size>128)return 0;
    while(n<size&&line[n]&&line[n]!='\r'&&line[n]!='\n'){unsigned char c=(unsigned char)line[n];if(c<32||c>126)return 0;out[n]=line[n];++n;}
    for(i=n;i<size;i++){unsigned char c=(unsigned char)line[i];if(c&&c!='\r'&&c!='\n'&&c!=' ')return 0;}
    while(n&&out[n-1]==' ')--n;
    out[n]=0;
    return n>0;
}
static int exact(const char *a,const char *b){return strcmp(a,b)==0;}
static int dec_u32(const char *p,const char **end,uint32_t *value){
    uint64_t v=0;unsigned n=0;if(!p||!*p)return 0;
    while(*p>='0'&&*p<='9'){if(n++>9)return 0;v=v*10u+(unsigned)(*p-'0');if(v>0xffffffffULL)return 0;++p;}
    if(!n)return 0;
    *value=(uint32_t)v;
    if(end)*end=p;
    return 1;
}
static int parse_vguard(const char *p,sfn_up_command *out){
    uint32_t v[5];unsigned i;const char *e=p;
    for(i=0;i<5;i++){if(!dec_u32(e,&e,&v[i]))return 0;if(i<4){if(*e!=':')return 0;++e;}else if(*e)return 0;}
    if(!v[0]||v[0]>0x7fffffffu)return 0;
    out->type=SFN_UP_CMD_VGUARD_SET;out->revision=v[0];out->flags=v[1];out->thresholds=v[2];out->timing=v[3];out->recovery_seconds=v[4];return 1;
}
static int dec_i32(const char *p,const char **end,int32_t *value){
    int neg=0;uint32_t mag;const char *e;if(!p)return 0;if(*p=='-'){neg=1;++p;}
    if(!dec_u32(p,&e,&mag)||mag>(neg?2147483648u:2147483647u))return 0;
    *value=neg?(mag==2147483648u?(-2147483647-1):-(int32_t)mag):(int32_t)mag;if(end)*end=e;return 1;
}
static int next_u32(const char **p,uint32_t *v,int last){const char *e;if(!dec_u32(*p,&e,v))return 0;if(last){if(*e)return 0;}else{if(*e!=':')return 0;e++;}*p=e;return 1;}
static int next_i32(const char **p,int32_t *v,int last){const char *e;if(!dec_i32(*p,&e,v))return 0;if(last){if(*e)return 0;}else{if(*e!=':')return 0;e++;}*p=e;return 1;}
static int parse_clock(const char *p,sfn_up_command *o){uint32_t epoch,t1,t2;int32_t off,o1,o2;if(!next_u32(&p,&epoch,0)||!next_i32(&p,&off,0)||!next_u32(&p,&t1,0)||!next_i32(&p,&o1,0)||!next_u32(&p,&t2,0)||!next_i32(&p,&o2,1))return 0;if(off< -32768||off>32767||o1< -32768||o1>32767||o2< -32768||o2>32767)return 0;o->type=SFN_UP_CMD_CLOCK_SYNC;o->epoch=epoch;o->offset_minutes=(int16_t)off;o->transition1_epoch=t1;o->transition1_offset_minutes=(int16_t)o1;o->transition2_epoch=t2;o->transition2_offset_minutes=(int16_t)o2;return 1;}
static int parse_begin(const char *p,sfn_up_command *o){uint32_t g,c;if(!next_u32(&p,&g,0)||!next_u32(&p,&c,1)||c>255)return 0;o->type=SFN_UP_CMD_SCHEDULE_BEGIN;o->generation=g;o->count=(uint8_t)c;return 1;}
static int parse_item(const char *p,sfn_up_command *o){uint32_t v[7];unsigned i;for(i=0;i<7;i++)if(!next_u32(&p,&v[i],i==6))return 0;if(v[0]>255||v[1]>255||v[2]>255||v[3]>255||v[4]>65535||v[5]>65535||v[6]>1)return 0;o->type=SFN_UP_CMD_SCHEDULE_ITEM;o->slot=(uint8_t)v[0];o->outlet=(uint8_t)v[1];o->repeat=(uint8_t)v[2];o->days=(uint8_t)v[3];o->date_days=(uint16_t)v[4];o->minute=(uint16_t)v[5];o->on=(uint8_t)v[6];return 1;}
static int parse_commit(const char *p,sfn_up_command *o){uint32_t g;if(!next_u32(&p,&g,1))return 0;o->type=SFN_UP_CMD_SCHEDULE_COMMIT;o->generation=g;return 1;}
static int dec_mask(const char *p,unsigned *value){
    unsigned v=0,n=0;if(!p||!*p)return 0;while(*p){if(*p<'0'||*p>'9'||n++>2)return 0;v=v*10u+(unsigned)(*p-'0');++p;}if(v>31)return 0;*value=v;return 1;
}
int sfn_up_parse_command(const char *line,size_t size,sfn_up_command *out){
    char b[129];unsigned mask;if(!out)return 0;memset(out,0,sizeof(*out));if(!clean(line,size,b))return 0;
    if(exact(b,"up:getinfo:all")){out->type=SFN_UP_CMD_GETINFO_ALL;return 1;}
    if(exact(b,"up:power_report:1:vol")){out->type=SFN_UP_CMD_VOLTAGE;out->channel=1;return 1;}
    if(exact(b,"up:query:wifirssi")){out->type=SFN_UP_CMD_RSSI;return 1;}
    if(exact(b,"up:childlock")){out->type=SFN_UP_CMD_CHILDLOCK_GET;return 1;}
    if(!strncmp(b,"up:vguard:",10)&&parse_vguard(b+10,out))return 1;
    if(!strncmp(b,"up:sfn:90:",10)&&parse_clock(b+10,out))return 1;
    if(!strncmp(b,"up:sfn:91:",10)&&parse_begin(b+10,out))return 1;
    if(!strncmp(b,"up:sfn:92:",10)&&parse_item(b+10,out))return 1;
    if(!strncmp(b,"up:sfn:93:",10)&&parse_commit(b+10,out))return 1;
    if(exact(b,"up:sfn:94")){out->type=SFN_UP_CMD_SCHEDULE_STATUS;return 1;}
    if(exact(b,"up:sfn:96")){out->type=SFN_UP_CMD_GUARD_STATUS;return 1;}
    if(!strncmp(b,"up:ota:",7)){
        const char *p=b+7,*colon=strchr(p,':'),*colon2;uint32_t sequence,bytes;unsigned i;
        if(!colon)return 0;
        {char nbuf[12];size_t nn=(size_t)(colon-p);const char *end;if(!nn||nn>=sizeof(nbuf))return 0;memcpy(nbuf,p,nn);nbuf[nn]=0;if(!dec_u32(nbuf,&end,&sequence)||*end||!sequence)return 0;}
        p=colon+1;colon2=strchr(p,':');if(!colon2)return 0;
        {char nbuf[12];size_t nn=(size_t)(colon2-p);const char *end;if(!nn||nn>=sizeof(nbuf))return 0;memcpy(nbuf,p,nn);nbuf[nn]=0;if(!dec_u32(nbuf,&end,&bytes)||*end||bytes<44u||bytes>0x6a014u)return 0;}
        p=colon2+1;if(strlen(p)!=64)return 0;
        for(i=0;i<32;i++){unsigned hi,lo;char x=p[i*2],y=p[i*2+1];hi=(x>='0'&&x<='9')?(unsigned)(x-'0'):(x>='a'&&x<='f')?(unsigned)(x-'a'+10):(x>='A'&&x<='F')?(unsigned)(x-'A'+10):99;lo=(y>='0'&&y<='9')?(unsigned)(y-'0'):(y>='a'&&y<='f')?(unsigned)(y-'a'+10):(y>='A'&&y<='F')?(unsigned)(y-'A'+10):99;if(hi>15||lo>15)return 0;out->ota_sha256[i]=(uint8_t)((hi<<4)|lo);}
        out->ota_sequence=sequence;out->ota_bytes=bytes;out->type=SFN_UP_CMD_OTA;return 1;
    }
    if(!strncmp(b,"up:childlock:",13)&&dec_mask(b+13,&mask)){out->type=SFN_UP_CMD_CHILDLOCK_SET;out->mask=(uint8_t)mask;return 1;}
    if(!strncmp(b,"up:onoff:",9)&&b[9]>='1'&&b[9]<='4'&&b[10]==':'){
        const char *v=b+11;if(exact(v,"on")||exact(v,"off")){out->type=SFN_UP_CMD_ONOFF;out->channel=(uint8_t)(b[9]-'0');out->on=(uint8_t)exact(v,"on");return 1;}
    }
    return 0;
}
static size_t finish(char *out,size_t cap,int n){if(!out||n<0||(size_t)n>=cap)return 0;return (size_t)n;}
static int mac_ok(const char *m){unsigned i;if(!m)return 0;for(i=0;i<12;i++){char c=m[i];if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')||(c>='a'&&c<='f')))return 0;}return m[12]==0;}
static int atom_ok(const char *p){if(!p||!*p)return 0;while(*p){unsigned char c=(unsigned char)*p++;if(c<33||c>126||c==';'||c=='\r'||c=='\n')return 0;}return 1;}
size_t sfn_up_encode_bootinfo(char *out,size_t cap,const char *model,const char mac[13],const char *fw){int n;if(!atom_ok(model)||!mac_ok(mac)||!atom_ok(fw))return 0;n=snprintf(out,cap,"up:bootinfo:%s;%s;%s;%s;connect\r\n",model,mac,mac,fw);return finish(out,cap,n);}
size_t sfn_up_encode_onoff_ack(char *out,size_t cap,unsigned ch,int on){int n;if(ch<1||ch>4)return 0;n=snprintf(out,cap,"up:onoff:%u:%s\r\n",ch,on?"on":"off");return finish(out,cap,n);}
size_t sfn_up_encode_event(char *out,size_t cap,unsigned ch,int on){int n;if(ch>4)return 0;n=snprintf(out,cap,"up:event:onoff:%u:%s\r\n",ch,on?"on":"off");return finish(out,cap,n);}
size_t sfn_up_encode_voltage(char *out,size_t cap,unsigned ch,int32_t mv){int n;if(ch<1||ch>5||mv<0)return 0;n=snprintf(out,cap,"up:power_report:%u:%ld\r\n",ch,(long)mv);return finish(out,cap,n);}
size_t sfn_up_encode_rssi(char *out,size_t cap,int rssi){int n;if(rssi>0||rssi< -127)return 0;n=snprintf(out,cap,"up:query:%d\r\n",rssi);return finish(out,cap,n);}
size_t sfn_up_encode_childlock(char *out,size_t cap,unsigned mask){int n;if(mask>31)return 0;n=snprintf(out,cap,"up:cl:%u\r\n",mask);return finish(out,cap,n);}
size_t sfn_up_encode_vguard_ack(char *out,size_t cap,uint32_t revision){int n;if(!revision||revision>0x7fffffffu)return 0;n=snprintf(out,cap,"up:vguard:%lu\r\n",(unsigned long)revision);return finish(out,cap,n);}
size_t sfn_up_encode_clock_ack(char *out,size_t cap,uint32_t epoch){int n;n=snprintf(out,cap,"up:sfn:90:%lu:0\r\n",(unsigned long)epoch);return finish(out,cap,n);}
size_t sfn_up_encode_schedule_commit_ack(char *out,size_t cap,uint32_t generation,unsigned count){int n;if(generation>0x7fffffffu||count>8)return 0;n=snprintf(out,cap,"up:sfn:93:%lu:%u\r\n",(unsigned long)generation,count);return finish(out,cap,n);}
size_t sfn_up_encode_schedule_status(char *out,size_t cap,uint32_t generation,unsigned count,int clock_valid,int storage_valid){int n;unsigned meta;if(count>8)return 0;meta=count|(clock_valid?0x100u:0u)|(storage_valid?0x200u:0u);n=snprintf(out,cap,"up:sfn:94:%lu:%u\r\n",(unsigned long)generation,meta);return finish(out,cap,n);}
size_t sfn_up_encode_guard_status(char *out,size_t cap,int available,int trusted){if(trusted&&!available)return 0;return finish(out,cap,snprintf(out,cap,"up:sfn:96:%u:%u\r\n",available?1u:0u,trusted?1u:0u));}
size_t sfn_up_encode_ota_ack(char *out,size_t cap){return finish(out,cap,snprintf(out,cap,"up:ota:ota_ok\r\n"));}
size_t sfn_up_encode_getinfo(char *out,size_t cap,const sfn_up_outlet x[4]){
    size_t used=0;unsigned i;uint32_t energy=0;int64_t power=0;int n;if(!out||!x||cap<16)return 0;
    n=snprintf(out,cap,"up:getinfo:");if(n<0||(size_t)n>=cap)return 0;used=(size_t)n;
    for(i=0;i<4;i++){
        const char *ol=x[i].overload?x[i].overload:"0",*oh=x[i].overheat?x[i].overheat:"0";
        if(x[i].channel!=i+1||strchr(ol,';')||strchr(ol,':')||strchr(oh,';')||strchr(oh,':'))return 0;
        n=snprintf(out+used,cap-used,"%u:%ld;%s;%ld;%s;%s;%ld;%08lX;00000000;00000000;0;00;%ld:",i+1,(long)x[i].runtime,x[i].on?"on":"off",(long)x[i].state,ol,oh,(long)x[i].power_mw,(unsigned long)x[i].energy_wh,(long)x[i].temperature_c);
        if(n<0||(size_t)n>=cap-used)return 0;
        used+=(size_t)n;
        energy+=x[i].energy_wh;
        power+=x[i].power_mw;
    }
    if(power>2147483647LL)power=2147483647LL;
    if(power< -2147483647LL)power=-2147483647LL;
    n=snprintf(out+used,cap-used,"5:0;on;0;0;0;%ld;%08lX;00000000;00000000;0;00;0\r\n",(long)power,(unsigned long)energy);
    if(n<0||(size_t)n>=cap-used)return 0;
    return used+(size_t)n;
}
