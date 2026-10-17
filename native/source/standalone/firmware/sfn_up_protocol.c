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
static int dec_mask(const char *p,unsigned *value){
    unsigned v=0,n=0;if(!p||!*p)return 0;while(*p){if(*p<'0'||*p>'9'||n++>2)return 0;v=v*10u+(unsigned)(*p-'0');++p;}if(v>31)return 0;*value=v;return 1;
}
int sfn_up_parse_command(const char *line,size_t size,sfn_up_command *out){
    char b[129];unsigned mask;if(!out)return 0;memset(out,0,sizeof(*out));if(!clean(line,size,b))return 0;
    if(exact(b,"up:getinfo:all")){out->type=SFN_UP_CMD_GETINFO_ALL;return 1;}
    if(exact(b,"up:power_report:1:vol")){out->type=SFN_UP_CMD_VOLTAGE;out->channel=1;return 1;}
    if(exact(b,"up:query:wifirssi")){out->type=SFN_UP_CMD_RSSI;return 1;}
    if(exact(b,"up:childlock")){out->type=SFN_UP_CMD_CHILDLOCK_GET;return 1;}
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
