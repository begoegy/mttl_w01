#include "mqtt_wire.h"
#include "deadline.h"
#include "settings.h"
static size_t auth(const char *s){size_t n=0;if(!mttl_settings_utf8(s,65))return 65;while(s[n])++n;return n;}
static size_t text(const char *s,size_t limit,int filter){size_t n=0;unsigned char c;if(!s)return limit+1;while(n<=limit&&s[n]){c=(unsigned char)s[n];if(c<32||c>126||(!filter&&(c=='+'||c=='#')))return limit+1;++n;}return n;}
static size_t finish(uint8_t *out,size_t cap,uint8_t first,const uint8_t *body,size_t n){
    uint8_t length[4];size_t count=0,i,value=n;
    do{uint8_t b=value%128;value/=128;if(value)b|=128;length[count++]=b;}while(value&&count<4);
    if(!out||n+count+1>cap||n+count+1>MTTL_MQ_WIRE_CAP)return 0;
    out[0]=first;for(i=0;i<count;i++)out[1+i]=length[i];for(i=0;i<n;i++)out[1+count+i]=body[i];return 1+count+n;
}
static void wipe(uint8_t *p,size_t n){volatile uint8_t *q=p;while(n--)*q++=0;}
static int put(uint8_t *body,size_t *used,const char *s,size_t n){size_t i;if(*used+n+2>MTTL_MQ_WIRE_CAP)return 0;body[(*used)++]=(uint8_t)(n>>8);body[(*used)++]=(uint8_t)n;for(i=0;i<n;i++)body[(*used)++]=(uint8_t)s[i];return 1;}
size_t mttl_mq_connect_packet(uint8_t *out,size_t cap,const char *client,const char *will,const char *user,const char *password,uint16_t keepalive){
    uint8_t body[MTTL_MQ_WIRE_CAP];size_t n=0,result=0,a=text(client,64,0),b=text(will,128,0),u=auth(user),p=auth(password);
    if(!a||a>64||!b||b>128||u>64||p>64||(!u&&p)||!keepalive)return 0;
    put(body,&n,"MQTT",4);body[n++]=4;body[n++]=(uint8_t)(0x26|(u?0x80:0)|(p?0x40:0));body[n++]=(uint8_t)(keepalive>>8);body[n++]=(uint8_t)keepalive;
    if(!put(body,&n,client,a)||!put(body,&n,will,b)||!put(body,&n,"offline",7))goto done;
    if(u&&!put(body,&n,user,u))goto done;if(p&&!put(body,&n,password,p))goto done;
    result=finish(out,cap,0x10,body,n);
done:wipe(body,sizeof(body));return result;
}
size_t mttl_mq_publish_packet(uint8_t *out,size_t cap,const char *topic,const uint8_t *payload,size_t size,int retained){
    uint8_t length[2];size_t n,i,t=text(topic,128,0),value,count=0,at;
    if(!out||!t||t>128||(!payload&&size)||size>1024-2-t)return 0;
    n=2+t+size;value=n;do{uint8_t b=value%128;value/=128;if(value)b|=128;length[count++]=b;}while(value);
    if(n+count+1>cap||n+count+1>1024)return 0;
    out[0]=retained?0x31:0x30;for(i=0;i<count;i++)out[1+i]=length[i];at=1+count;
    out[at++]=(uint8_t)(t>>8);out[at++]=(uint8_t)t;for(i=0;i<t;i++)out[at++]=(uint8_t)topic[i];for(i=0;i<size;i++)out[at++]=payload[i];return at;
}
size_t mttl_mq_subscribe_packet(uint8_t *out,size_t cap,const char *filter,uint16_t id){
    uint8_t body[MTTL_MQ_WIRE_CAP];size_t n=2,t=text(filter,128,1),i;
    if(!t||t>128||!id)return 0;
    for(i=0;i<t;i++)if((filter[i]=='+'&&(i&&filter[i-1]!='/'))||(filter[i]=='+'&&i+1<t&&filter[i+1]!='/')||(filter[i]=='#'&&(i+1!=t||(i&&filter[i-1]!='/'))))return 0;
    body[0]=(uint8_t)(id>>8);body[1]=(uint8_t)id;put(body,&n,filter,t);body[n++]=0;return finish(out,cap,0x82,body,n);
}
int mttl_mq_connack(const uint8_t *p,size_t n,int *accepted){if(!p||!accepted||n!=4||p[0]!=0x20||p[1]!=2||p[2]!=0||p[3]>5)return 0;*accepted=p[3]==0;return 1;}
int mttl_mq_suback(const uint8_t *p,size_t n,uint16_t id,int *accepted){if(!p||!accepted||!id||n!=5||p[0]!=0x90||p[1]!=3||p[2]!=(uint8_t)(id>>8)||p[3]!=(uint8_t)id||(p[4]!=0&&p[4]!=0x80))return 0;*accepted=p[4]==0;return 1;}
void mttl_mq_rx_reset(mttl_mq_rx *r){wipe(r->bytes,sizeof(r->bytes));r->used=r->total=0;r->since=0;r->failed=0;}
int mttl_mq_rx_feed(mttl_mq_rx *r,const uint8_t *in,size_t size,uint32_t now,size_t *consumed){
    size_t i;uint32_t remaining,factor;unsigned j;uint8_t b;
    if(!r||!consumed||(!in&&size))return -1;*consumed=0;
    if(r->failed)return -1;if(r->total&&r->used==r->total)return 1;
    if(r->used&&!mttl_remaining(r->since,now,2000))goto fault;
    for(i=0;i<size;i++){
        if(!r->used)r->since=now;if(r->used==MTTL_MQ_WIRE_CAP)goto fault;
        r->bytes[r->used++]=in[i];++*consumed;
        if(!r->total&&r->used>=2){remaining=0;factor=1;
            for(j=1;j<r->used&&j<=4;j++){b=r->bytes[j];remaining+=(b&127)*factor;
                if(!(b&128)){if(j>1&&b==0)goto fault;if(remaining+j+1>MTTL_MQ_WIRE_CAP)goto fault;r->total=(uint16_t)(remaining+j+1);break;}factor*=128;
            }
            if(!r->total&&r->used>=5)goto fault;
        }
        if(r->total&&r->used==r->total)return 1;
    }return 0;
fault:r->failed=1;return -1;
}
