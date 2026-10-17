#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_up_client.h"

typedef struct {int open_mode,opened,closed,send_limit,recv_fail;char input[2048],sent[4096];size_t in_pos,in_size,sent_size;unsigned handled;} mock;
static int op(void *p,const char *h,uint16_t port){mock *m=p;assert(!strcmp(h,"10.0.0.5")&&port==10086);if(m->open_mode<0)return -1;if(m->open_mode==0){m->open_mode=1;return 0;}m->opened=1;return 1;}
static int cl(void *p){mock *m=p;m->closed++;m->opened=0;return 1;}
static int tx(void *p,const uint8_t *b,size_t n){mock *m=p;size_t take;if(!m->opened)return -1;take=m->send_limit>0&&(size_t)m->send_limit<n?(size_t)m->send_limit:n;if(m->sent_size+take>sizeof(m->sent))return -1;memcpy(m->sent+m->sent_size,b,take);m->sent_size+=take;return (int)take;}
static int rx(void *p,uint8_t *b,size_t n){mock *m=p;size_t take;if(m->recv_fail)return -1;if(m->in_pos==m->in_size)return 0;take=m->in_size-m->in_pos;if(take>n)take=n;if(take>7)take=7;memcpy(b,m->input+m->in_pos,take);m->in_pos+=take;return (int)take;}
static void feed(mock *m,const char *s){size_t n=strlen(s);assert(m->in_size+n<sizeof(m->input));memcpy(m->input+m->in_size,s,n);m->in_size+=n;}
static size_t handle(void *p,const sfn_up_command *c,char *out,size_t cap){mock *m=p;m->handled++;switch(c->type){case SFN_UP_CMD_ONOFF:return sfn_up_encode_onoff_ack(out,cap,c->channel,c->on);case SFN_UP_CMD_CHILDLOCK_GET:return sfn_up_encode_childlock(out,cap,7);default:return 0;}}
static void stepn(sfn_up_client *c,sfn_up_io *io,mock *m,uint32_t *now,unsigned count){unsigned i;(void)m;for(i=0;i<count;i++){assert(sfn_up_client_step(c,io,1,"10.0.0.5",10086,*now,"lgutap","88D03907723D","0.1.55-2.1.0",handle,m));*now+=10;}}
int main(void){sfn_up_client c;sfn_up_io io;mock m;uint32_t now=0;char event[64];size_t n;
 memset(&m,0,sizeof(m));m.send_limit=5;io.context=&m;io.open=op;io.close=cl;io.send=tx;io.receive=rx;sfn_up_client_init(&c);
 stepn(&c,&io,&m,&now,30);assert(c.state==SFN_UP_LINK_ONLINE&&c.boot_sent);assert(strstr(m.sent,"up:bootinfo:lgutap;88D03907723D;88D03907723D;0.1.55-2.1.0;connect\r\n"));
 feed(&m,"up:onoff:2:on\r\nup:childlock\r\n");stepn(&c,&io,&m,&now,50);assert(m.handled==2);assert(strstr(m.sent,"up:onoff:2:on\r\n"));assert(strstr(m.sent,"up:cl:7\r\n"));
 n=sfn_up_encode_event(event,sizeof(event),3,0);assert(sfn_up_client_publish(&c,event,n));stepn(&c,&io,&m,&now,20);assert(strstr(m.sent,"up:event:onoff:3:off\r\n"));
 /* Unknown command is rejected without dropping a healthy transport. */
 feed(&m,"up:ota:10.0.0.1\r\n");stepn(&c,&io,&m,&now,20);assert(c.rejected==1&&c.state==SFN_UP_LINK_ONLINE);
 /* A dead TCP path (what keepalive reports after a WAN black-hole) must close,
  * back off, and establish a fresh session without rebooting the device. */
 {unsigned reconnects=c.reconnects;int closed=m.closed;m.recv_fail=1;stepn(&c,&io,&m,&now,1);assert(c.state==SFN_UP_LINK_BACKOFF&&m.closed==closed+1);m.recv_fail=0;now+=2100;stepn(&c,&io,&m,&now,30);assert(c.state==SFN_UP_LINK_ONLINE&&c.boot_sent&&c.reconnects>reconnects);}
 /* Oversized unterminated input forces reconnect rather than buffer overrun/desync. */
 {char huge[300];memset(huge,'A',sizeof(huge));memcpy(m.input+m.in_size,huge,sizeof(huge));m.in_size+=sizeof(huge);}stepn(&c,&io,&m,&now,60);assert(c.io_failures>=1&&m.closed>=1);
 /* Network-down closes immediately and clears state. */
 assert(sfn_up_client_step(&c,&io,0,"10.0.0.5",10086,now,"lgutap","88D03907723D","0.1.55-2.1.0",handle,&m));assert(c.state==SFN_UP_LINK_OFF);
 /* Failed connect uses backoff; no tight reconnect loop. */
 memset(&m,0,sizeof(m));m.open_mode=-1;io.context=&m;sfn_up_client_init(&c);now=0;assert(sfn_up_client_step(&c,&io,1,"10.0.0.5",10086,now,"lgutap","88D03907723D","0.1.55-2.1.0",handle,&m));assert(c.state==SFN_UP_LINK_BACKOFF);{int closed=m.closed;now+=100;stepn(&c,&io,&m,&now,5);assert(m.closed==closed);}
 puts("SFN_UP_CLIENT_TESTS=PASS");return 0;}
