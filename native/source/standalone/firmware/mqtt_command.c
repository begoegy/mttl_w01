#include "mqtt_command.h"
int mttl_mq_command(const mttl_mqtt_session *s,uint32_t generation,const char *prefix,const uint8_t *frame,size_t size,mttl_command *out){
    mttl_publish p;size_t n=0,i,pos;uint8_t channel,on;
    if(!s||!prefix||!frame||!out||!mttl_parse_publish(frame,size,&p)||!mttl_mq_can_command(s,generation,p.retained))return 0;
    while(n<128&&prefix[n]){unsigned char c=(unsigned char)prefix[n];if(c<33||c>126||c=='+'||c=='#')return 0;++n;}
    if(!n||n==128||prefix[n-1]=='/'||p.topic_size<n+6)return 0;
    for(i=0;i<n;i++)if(p.topic[i]!=(uint8_t)prefix[i])return 0;
    if(p.topic[n]!='/')return 0;pos=n+1;
    if(p.topic_size==n+6&&p.topic[pos]>='1'&&p.topic[pos]<='4'){channel=p.topic[pos]-'1';++pos;}
    else if(p.topic_size==n+8&&p.topic[pos]=='a'&&p.topic[pos+1]=='l'&&p.topic[pos+2]=='l'){channel=4;pos+=3;}
    else return 0;
    if(p.topic[pos]!='/'||p.topic[pos+1]!='s'||p.topic[pos+2]!='e'||p.topic[pos+3]!='t')return 0;
    if(p.payload_size==2&&p.payload[0]=='O'&&p.payload[1]=='N')on=1;
    else if(p.payload_size==3&&p.payload[0]=='O'&&p.payload[1]=='F'&&p.payload[2]=='F')on=0;
    else return 0;
    out->channel=channel;out->on=on;return 1;
}
