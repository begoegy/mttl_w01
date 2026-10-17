#include "ntp_transport.h"
#include <string.h>
extern unsigned mttl_test_count,mttl_test_failed,mttl_failed_ids[64];
static void check(int ok){++mttl_test_count;if(!ok&&mttl_test_failed<64)mttl_failed_ids[mttl_test_failed++]=mttl_test_count;}
typedef struct {int opens,closes,close_fail,send_fail,entropy_fail,n;uint32_t source;uint16_t port;uint8_t packet[49],request[48];} mock;
static int open_io(void *p,uint32_t ip,uint16_t port){mock *m=p;check(ip==0xc0000201u&&port==123);m->opens++;return 1;}
static int close_io(void *p){mock *m=p;m->closes++;return !m->close_fail;}
static int send_io(void *p,const uint8_t *b,size_t n){mock *m=p;memcpy(m->request,b,n);return m->send_fail?47:(int)n;}
static int recv_io(void *p,uint8_t *b,size_t n,uint32_t *ip,uint16_t *port){mock *m=p;memcpy(b,m->packet,n);*ip=m->source;*port=m->port;return m->n;}
static int entropy_io(void *p,uint8_t *b,size_t n){mock *m=p;memset(b,1,n);return !m->entropy_fail;}
void mttl_ntp_transport_test(void){
    mock m; mttl_ntp_transport t;mttl_ntp_client c;
    mttl_ntp_io io={&m,open_io,close_io,send_io,recv_io,entropy_io};
    memset(&m,0,sizeof(m));m.source=0xc0000201u;m.port=123;
    check(!mttl_ntp_transport_init(&t,0));check(!mttl_ntp_transport_init(&t,0xe0000001u));
    check(mttl_ntp_transport_init(&t,m.source));mttl_ntp_client_init(&c,1577836800ull);
    check(!mttl_ntp_transport_tick(&t,&c,&io,1,0)&&t.opened&&c.waiting);
    m.packet[0]=0x24;m.packet[1]=1;memcpy(m.packet+24,m.request+40,8);
    /* 2020-01-01 NTP seconds, receive and transmit timestamp. */
    m.packet[32]=m.packet[40]=0xe1;m.packet[33]=m.packet[41]=0xb6;
    m.packet[34]=m.packet[42]=0x5f;m.packet[35]=m.packet[43]=0x80;
    m.n=49;check(!mttl_ntp_transport_tick(&t,&c,&io,1,1)&&!c.time.valid);
    m.n=48;m.port=124;check(!mttl_ntp_transport_tick(&t,&c,&io,1,2));
    m.port=123;m.source++;check(!mttl_ntp_transport_tick(&t,&c,&io,1,3));m.source--;
    m.packet[24]^=1;check(!mttl_ntp_transport_tick(&t,&c,&io,1,4));m.packet[24]^=1;
    check(mttl_ntp_transport_tick(&t,&c,&io,1,5)&&c.time.valid&&!t.opened);
    check(!mttl_ntp_transport_tick(&t,&c,&io,1,6)&&m.opens==1);
    mttl_ntp_transport_tick(&t,&c,&io,0,7);m.n=0;
    mttl_ntp_transport_tick(&t,&c,&io,1,8);check(t.opened);
    m.close_fail=1;mttl_ntp_transport_tick(&t,&c,&io,0,9);check(t.opened&&!c.waiting);
    mttl_ntp_transport_tick(&t,&c,&io,1,10);check(m.opens==2&&t.opened);
    m.close_fail=0;mttl_ntp_transport_tick(&t,&c,&io,1,11);check(m.opens==3&&c.waiting);
    mttl_ntp_transport_tick(&t,&c,&io,1,3011);check(!t.opened&&!c.waiting&&c.delay==5000);
    m.entropy_fail=1;mttl_ntp_transport_tick(&t,&c,&io,1,8011);check(!t.opened&&m.opens==3);
    m.entropy_fail=0;m.send_fail=1;mttl_ntp_transport_tick(&t,&c,&io,1,13011);check(!t.opened&&!c.waiting);
}
