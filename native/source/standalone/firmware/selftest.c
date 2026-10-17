#include "core.h"
#include <limits.h>
#include "deadline.h"
#include "settings_record.h"
#include "ntp.h"
#include "ntp_client.h"
#include "mqtt_session.h"
#include "mqtt_command.h"
#include "mqtt_wire.h"
#include "mqtt_engine.h"
#include "protection.h"
void mttl_integration_test(void);
void *memcpy(void *destination,const void *source,size_t size){
    uint8_t *d=destination;const uint8_t *s=source;size_t i;
    for(i=0;i<size;i++)d[i]=s[i];return destination;
}

/* Linked only into the offline executable. No target startup or peripheral code. */
uint32_t mttl_test_count, mttl_test_failed, mttl_failed_ids[64];
/* Compiler-generated structure initialization needs this freestanding helper. */
void *memset(void *p, int value, size_t size) {
    uint8_t *bytes = p;
    while (size--) *bytes++ = (uint8_t)value;
    return p;
}
static void check(int pass) {
    ++mttl_test_count;
    if (!pass && mttl_test_failed < 64) mttl_failed_ids[mttl_test_failed++] = mttl_test_count;
}
static unsigned cleanup_deletes,cleanup_closed,cleanup_ping,cleanup_ack,cleanup_bad;
static int cleanup_close(void *p){(void)p;++cleanup_closed;return 1;}
static int cleanup_open(void *p,const char *host,uint16_t port){(void)p;(void)host;(void)port;return 1;}
static int cleanup_send(void *p,const uint8_t *bytes,size_t n){
    mttl_publish pub;(void)p;
    if(bytes[0]==0x31){if(!mttl_parse_publish(bytes,n,&pub)||pub.payload_size)++cleanup_bad;else ++cleanup_deletes;}
    else if(bytes[0]==0xc0)++cleanup_ping;
    return (int)n;
}
static int cleanup_receive(void *p,uint8_t *bytes,size_t n){(void)p;(void)n;
    if(cleanup_ping&&cleanup_ack){bytes[0]=0xd0;bytes[1]=0;cleanup_ack=0;return 2;}return 0;
}
static void cleanup_test(void){
    static mttl_mqtt_engine e;static mttl_settings s;
    mttl_mqtt_io io={0,cleanup_open,cleanup_close,cleanup_send,cleanup_receive,0,0};unsigned i;
    mttl_settings_default(&s);memcpy(s.broker,"broker",7);s.mqtt_enabled=0;
    mttl_mqtt_engine_init(&e);e.session.state=MTTL_MQ_ONLINE;e.session.desired=1;e.phase=9;
    cleanup_deletes=cleanup_closed=cleanup_ping=cleanup_ack=cleanup_bad=0;
    for(i=0;i<80;i++)mttl_mqtt_engine_step(&e,&s,"88d03904e3c4",1,0,i,&io);
    check(cleanup_deletes==MTTL_HA_ENTITIES+1&&!cleanup_bad);
    check(cleanup_ping==1&&!cleanup_closed&&!e.cleanup_done);
    check(!mttl_mqtt_engine_publish(&e,"test",0,0,0,80));
    cleanup_ack=1;
    for(i=80;i<85;i++)mttl_mqtt_engine_step(&e,&s,"88d03904e3c4",1,0,i,&io);
    check(e.cleanup_done&&cleanup_closed==1&&e.session.state==MTTL_MQ_OFF);
    s.mqtt_enabled=1;mttl_mqtt_engine_step(&e,&s,"88d03904e3c4",1,1,90,&io);
    check(!e.cleanup_done&&e.session.desired);
    /* Disabled with no network stays pending, including a fresh boot. */
    mttl_mqtt_engine_init(&e);s.mqtt_enabled=0;
    mttl_mqtt_engine_step(&e,&s,"88d03904e3c4",0,0,0,&io);
    check(!e.cleanup_done&&e.session.state==MTTL_MQ_OFF);
    mttl_mqtt_engine_step(&e,&s,"88d03904e3c4",1,0,1,&io);
    check(e.session.state==MTTL_MQ_CONNECTING);
}
typedef struct { unsigned begins, ends, overlap; int active, fail_start, fail_end; } mock;
static int begin(void *p, uint8_t channel, uint8_t on) {
    mock *m = p; (void)channel; (void)on;
    ++m->begins;
    if (m->active) ++m->overlap;
    m->active = 1;
    return !m->fail_start;
}
static int end(void *p, uint8_t channel) {
    mock *m = p; (void)channel;
    ++m->ends;
    if (m->fail_end) return 0;
    m->active = 0;
    return 1;
}
static int commit(void *p, const char *ssid, const char *password) {
    unsigned *calls = p;
    ++*calls;
    return ssid[0] == 'T' && password[0] == 'x';
}
static void tests_control(void) {
    mock m = {0,0,0,0,0,0};
    mttl_output output = {&m, begin, end};
    mttl_control c;
    mttl_button b = {0,0};
    unsigned i;
    mttl_control_init(&c,100);
    mttl_control_tick(&c,&output,0);
    check(!m.begins && !c.known_mask);
    check(mttl_request(&c,MTTL_ALL,1));
    for (i = 0; i <= 400; i += 100) mttl_control_tick(&c,&output,i);
    check(c.known_mask == 15 && c.on_mask == 15 && m.begins == 4 && m.ends == 4 && !m.overlap);
    check(mttl_request(&c,MTTL_ALL,0) && mttl_request(&c,MTTL_ALL,1));
    check(!mttl_request(&c,0,1) && c.count == 8);
    mttl_control_init(&c,100);
    mttl_request(&c,0,1); mttl_control_tick(&c,&output,0);
    m.fail_end = 1; mttl_control_tick(&c,&output,100);
    check(c.fault == MTTL_END_FAULT && !c.known_mask && c.active);
    check(!mttl_request(&c,MTTL_ALL,0));
    m.active = 0; m.fail_end = 0;
    mttl_control_init(&c,100); m.fail_start = 1;
    mttl_request(&c,0,1); mttl_control_tick(&c,&output,0);
    check(c.fault == MTTL_START_FAULT && !m.active);
    m.fail_start = 0; mttl_control_init(&c,100);
    mttl_request(&c,0,1); mttl_control_tick(&c,&output,0xffffffd0u);
    mttl_control_tick(&c,&output,51); check(c.active && !c.known_mask);
    mttl_control_tick(&c,&output,52); check(!c.active && c.known_mask == 1);
    mttl_button_edge(&b,1,0xfffffff0u);
    check(mttl_button_edge(&b,0,9984) == 2);
    check(mttl_button_edge(&b,0,9985) == 0);
    mttl_control_init(&c,0); check(c.fault != MTTL_OK);
}
static void tests_meter(void) {
    mttl_sample s[4]; int32_t value = -1; unsigned i;
    for (i = 0; i < 4; ++i) mttl_sample_update(&s[i],19,1,0);
    check(mttl_current_sum(s,0,5000,&value) && value == 76);
    mttl_sample_update(&s[0],0,0,1);
    check(!mttl_current_sum(s,1,5000,&value) && s[0].value == 19);
    mttl_sample_update(&s[0],0,1,0);
    check(mttl_sample_read(&s[0],5000,5000,&value) && value == 0);
    check(!mttl_sample_read(&s[0],5001,5000,&value));
    for (i = 0; i < 4; ++i) mttl_sample_update(&s[i],INT32_MAX,1,0);
    check(!mttl_current_sum(s,0,5000,&value));
    mttl_sample_update(&s[0],12,1,0xfffffff0u);
    check(mttl_sample_read(&s[0],10,26,&value) && value == 12);
}
static void tests_protocol(void) {
    static const uint8_t good[] = {0x30,4,0,1,'t','A'};
    static const uint8_t bad[] = {0x30,2,0,16};
    static const uint8_t huge[] = {0x30,2,255,255};
    static const uint8_t continuation[] = {0x30,128,128,128,128,0};
    mttl_publish p; mttl_send s; uint16_t value = 0;
    check(mttl_parse_publish(good,sizeof(good),&p) && p.payload_size == 1 && p.payload[0] == 'A');
    check(!mttl_parse_publish(bad,sizeof(bad),&p) && !p.payload);
    check(!mttl_parse_publish(huge,sizeof(huge),&p));
    check(!mttl_parse_publish(continuation,sizeof(continuation),&p));
    check(mttl_content_length((const uint8_t *)"1024",4,&value) && value == 1024);
    check(!mttl_content_length((const uint8_t *)"11111111111",11,&value));
    check(!mttl_content_length((const uint8_t *)"1025",4,&value));
    check(mttl_send_init(&s,128,0,1000) && mttl_send_progress(&s,1,1) == 0 && s.sent == 1);
    check(mttl_send_progress(&s,64,2) == 0 && s.sent == 65);
    check(mttl_send_progress(&s,63,3) == 1 && s.sent == 128);
    mttl_send_init(&s,1,0xffffff00u,1000);
    check(mttl_send_progress(&s,0,743) == 0 && mttl_send_progress(&s,1,744) == -1);
    mttl_send_init(&s,1,0,1000); check(mttl_send_progress(&s,-1,0) == -1 && s.sent == 0);
}
static void tests_network(void) {
    mttl_network n; uint32_t gen; unsigned calls = 0;
    mttl_network_init(&n); gen = mttl_provision(&n,0);
    check(!mttl_stopped(&n,gen-1,1) && n.state == MTTL_NET_STOPPING);
    check(mttl_stopped(&n,gen,2));
    check(mttl_wifi_submit(&n,(const uint8_t *)"T",1,(const uint8_t *)"x",1,3));
    gen = n.generation;
    check(!mttl_associated(&n,gen-1,4) && mttl_associated(&n,gen,4));
    check(mttl_got_ip(&n,gen,0xc000020au,5,commit,&calls) && calls == 1 && n.state == MTTL_NET_ONLINE && !n.password[0]);
    gen = mttl_provision(&n,10); mttl_stopped(&n,gen,11);
    mttl_wifi_submit(&n,(const uint8_t *)"T",1,(const uint8_t *)"x",1,12);
    gen = n.generation; mttl_associated(&n,gen,13);
    check(!mttl_got_ip(&n,gen,0xc000020au,15013,commit,&calls) && calls == 1 && n.state == MTTL_NET_STOPPING);
    mttl_network_init(&n); gen = mttl_provision(&n,0);
    check(!mttl_stopped(&n,gen,5000) && n.state == MTTL_NET_FAULT);
}
uint32_t mttl_selftest(void) {
    mttl_test_count = mttl_test_failed = 0;
    {
        uint8_t packet[512];size_t n,used;int accepted;mttl_publish publish;
        static mttl_mq_rx rx;
        static const uint8_t ack[]={0x20,2,0,0,0xd0,0};
        uint8_t suback[]={0x90,3,0,1,0};
        static const uint8_t nonminimal[]={0x30,0x80,0};
        static const uint8_t huge[]={0x30,0x80,4};
        n=mttl_mq_connect_packet(packet,sizeof(packet),"mttl-a","mttl/a/availability","user","pass",30);
        check(n>20&&packet[0]==0x10&&packet[9]==0xe6);
        check(!mttl_mq_connect_packet(packet,1,"mttl-a","mttl/a/availability","user","pass",30));
        check(!mttl_mq_connect_packet(packet,512,"mttl-a","mttl/a/availability","","pass",30));
        n=mttl_mq_publish_packet(packet,512,"mttl/a/state",(const uint8_t *)"online",6,1);
        check(mttl_parse_publish(packet,n,&publish)&&publish.retained&&publish.payload_size==6);
        check(!mttl_mq_publish_packet(packet,512,"mttl/+/state",(const uint8_t *)"online",6,1));
        check(mttl_mq_subscribe_packet(packet,512,"mttl/a/+/set",1)>0&&packet[0]==0x82);
        check(!mttl_mq_subscribe_packet(packet,512,"mttl/a/x+/set",1));
        check(mttl_mq_connack(ack,4,&accepted)&&accepted);
        check(!mttl_mq_connack(ack,6,&accepted));
        check(mttl_mq_suback(suback,5,1,&accepted)&&accepted);
        check(!mttl_mq_suback(suback,5,2,&accepted));suback[4]=0x80;
        check(mttl_mq_suback(suback,5,1,&accepted)&&!accepted);
        mttl_mq_rx_reset(&rx);check(!mttl_mq_rx_feed(&rx,ack,1,0,&used)&&used==1);
        check(mttl_mq_rx_feed(&rx,ack+1,5,1,&used)==1&&used==3&&rx.total==4);
        mttl_mq_rx_reset(&rx);check(mttl_mq_rx_feed(&rx,ack+4,2,2,&used)==1&&rx.total==2);
        mttl_mq_rx_reset(&rx);check(mttl_mq_rx_feed(&rx,nonminimal,3,0,&used)==-1);
        mttl_mq_rx_reset(&rx);check(mttl_mq_rx_feed(&rx,huge,3,0,&used)==-1);
        mttl_mq_rx_reset(&rx);mttl_mq_rx_feed(&rx,ack,1,0,&used);
        check(mttl_mq_rx_feed(&rx,0,0,2000,&used)==-1);
    }
    {
        mttl_mqtt_session s;uint32_t generation;mttl_mq_init(&s);
        check(mttl_mq_step(&s,0)==MTTL_MQ_NONE);
        mttl_mq_update(&s,1,1,1,0);check(mttl_mq_step(&s,0)==MTTL_MQ_CONNECT);generation=s.generation;
        check(!mttl_mq_can_command(&s,generation,0));
        check(!mttl_mq_connected(&s,generation+1,1));
        check(mttl_mq_connected(&s,generation,1));check(mttl_mq_ready(&s,generation,2));
        check(mttl_mq_can_command(&s,generation,0)&&!mttl_mq_can_command(&s,generation,1));
        {
            uint8_t frame[]="\x30\x10\x00\x0c" "mttl/a/1/setON";
            static const uint8_t all[]="\x30\x13\x00\x0e" "mttl/a/all/setOFF";
            mttl_command command={99,99};
            check(mttl_mq_command(&s,generation,"mttl/a",frame,sizeof(frame)-1,&command)&&command.channel==0&&command.on==1);
            check(!mttl_mq_command(&s,generation,"mttl/b",frame,sizeof(frame)-1,&command));
            check(!mttl_mq_command(&s,generation+1,"mttl/a",frame,sizeof(frame)-1,&command));
            frame[0]=0x31;check(!mttl_mq_command(&s,generation,"mttl/a",frame,sizeof(frame)-1,&command));frame[0]=0x30;
            check(mttl_mq_command(&s,generation,"mttl/a",all,sizeof(all)-1,&command)&&command.channel==4&&!command.on);
            check(!mttl_mq_command(&s,generation,"mttl/a",frame,sizeof(frame)-2,&command));
        }
        mttl_mq_update(&s,1,1,2,3);check(mttl_mq_step(&s,3)==MTTL_MQ_CLOSE&&!mttl_mq_can_command(&s,generation,0));
        check(mttl_mq_stopped(&s,generation,4));check(mttl_mq_step(&s,4)==MTTL_MQ_CONNECT);
        check(!mttl_mq_connected(&s,generation,5));generation=s.generation;
        mttl_mq_failed(&s,generation,5);check(mttl_mq_stopped(&s,generation,6));
        check(mttl_mq_step(&s,1005)==MTTL_MQ_NONE&&mttl_mq_step(&s,1006)==MTTL_MQ_CONNECT);
        mttl_mq_update(&s,0,1,2,1007);mttl_mq_step(&s,6007);check(s.state==MTTL_MQ_FAULT);
        mttl_mq_update(&s,1,1,3,6008);check(mttl_mq_step(&s,6008)==MTTL_MQ_NONE);
    }
    {extern unsigned mttl_settings_mock_tests(void);check(mttl_settings_mock_tests()==0);}
    {
        uint8_t p[48],token[8]={1,2,3,4,5,6,7,8};unsigned i;
        uint64_t ms=0;mttl_time_anchor anchor={0,0,0};
        check(mttl_ntp_request(p,token)&&p[0]==0x23&&p[40]==1);
        for(i=0;i<48;i++)p[i]=0;p[0]=0x24;p[1]=1;for(i=0;i<8;i++)p[24+i]=token[i];
        /* 2020-01-01 Unix 1577836800 = NTP 3786825600. */
        p[32]=p[40]=0xe1;p[33]=p[41]=0xb6;p[34]=p[42]=0x5f;p[35]=p[43]=0x80;p[44]=0x80;
        check(mttl_ntp_response(p,48,token,1577836800ull,&ms)&&ms==1577836800500ull);
        p[24]^=1;check(!mttl_ntp_response(p,48,token,1577836800ull,&ms));p[24]^=1;
        p[0]|=0xc0;check(!mttl_ntp_response(p,48,token,1577836800ull,&ms));p[0]=0x24;
        p[1]=0;check(!mttl_ntp_response(p,48,token,1577836800ull,&ms));p[1]=1;
        check(!mttl_ntp_response(p,47,token,1577836800ull,&ms));
        /* NTP era 1, second 1 = Unix 2085978497 (2036 wrap). */
        p[40]=p[41]=p[42]=0;p[43]=1;p[44]=0;
        check(mttl_ntp_response(p,48,token,2085978497ull,&ms)&&ms==2085978497000ull);
        check(!mttl_time_now(&anchor,0,&ms));mttl_time_set(&anchor,1000,0xfffffff0u);
        check(mttl_time_now(&anchor,0x10u,&ms)&&ms==1032);
        check(!mttl_time_now(&anchor,0x7ffffff0u,&ms));
        {
            mttl_ntp_client client;uint32_t generation;uint8_t request[48];
            mttl_ntp_client_init(&client,2085978497ull);
            check(!mttl_ntp_due(&client,0));mttl_ntp_link(&client,1,0);
            check(mttl_ntp_begin(&client,0,token,request,&generation));
            check(!mttl_ntp_accept(&client,1,generation+1,p,48));
            check(mttl_ntp_accept(&client,2,generation,p,48)&&client.time.valid);
            check(!mttl_ntp_due(&client,3600001));
            check(mttl_ntp_begin(&client,3600002,token,request,&generation));
            check(!mttl_ntp_due(&client,3603002)&&!client.waiting);
            check(!mttl_ntp_due(&client,3608001)&&mttl_ntp_due(&client,3608002));
            check(mttl_ntp_begin(&client,3608002,token,request,&generation));
            mttl_ntp_link(&client,0,3608003);
            check(!mttl_ntp_accept(&client,3608004,generation,p,48));
            mttl_ntp_link(&client,1,3608005);check(mttl_ntp_due(&client,3608005));
        }
    }
    {
        static uint8_t a[MTTL_SETTINGS_SLOT],b[MTTL_SETTINGS_SLOT];
        static const uint8_t body[]={1,2,3,4};uint32_t seq;size_t size;
        check(mttl_settings_encode(a,body,4,0xffffffffu));
        check(!mttl_settings_validate(a,&seq,&size));
        a[MTTL_SETTINGS_SLOT-1]=0;
        check(mttl_settings_validate(a,&seq,&size)&&seq==0xffffffffu&&size==4);
        check(mttl_settings_encode(b,body,4,0));b[MTTL_SETTINGS_SLOT-1]=0;
        check(mttl_settings_choose(a,b)==1);
        b[16]^=1;check(mttl_settings_choose(a,b)==0);b[16]^=1;
        b[4]^=1;check(!mttl_settings_validate(b,&seq,&size));
        mttl_settings_encode(b,body,4,0xffffffffu);b[MTTL_SETTINGS_SLOT-1]=0;
        check(mttl_settings_choose(a,b)==-2);
        mttl_settings_encode(b,body,4,0x7fffffffu);b[MTTL_SETTINGS_SLOT-1]=0;
        check(mttl_settings_choose(a,b)==-2);
        check(!mttl_settings_encode(b,body,MTTL_SETTINGS_SLOT,0));
    }
    tests_control(); tests_meter(); tests_protocol(); tests_network();
    {
        extern unsigned mttl_sdk_mock_tests(void);
        check(mttl_sdk_mock_tests()==0);
        extern unsigned mttl_lifecycle_mock_tests(void);
        check(mttl_lifecycle_mock_tests()==0);
        extern unsigned mttl_dhcp_mock_tests(void);
        check(mttl_dhcp_mock_tests()==0);
        {
            extern int mttl_wifi_form(const unsigned char *,size_t,char *,char *);
            extern int mttl_wifi_form_type(const char *);
            char ssid[33],password[65];
            static const unsigned char valid[]="ssid=Tommy+Bedroom&password=abc%26defgh";
            static const unsigned char duplicate[]="ssid=A&ssid=B&password=";
            static const unsigned char zero[]="ssid=%00&password=";
            static const unsigned char bad[]="ssid=%Q0&password=";
            static const unsigned char open[]="ssid=A&password=";
            check(mttl_wifi_form(valid,sizeof(valid)-1,ssid,password)&&ssid[5]==' '&&password[3]=='&');
            check(!mttl_wifi_form(duplicate,sizeof(duplicate)-1,ssid,password));
            check(!mttl_wifi_form(zero,sizeof(zero)-1,ssid,password));
            check(!mttl_wifi_form(bad,sizeof(bad)-1,ssid,password));
            check(mttl_wifi_form(open,sizeof(open)-1,ssid,password)&&!password[0]);
            check(!mttl_wifi_form(open,321,ssid,password));
            check(!mttl_wifi_form((const unsigned char *)"ssid=A&",7,ssid,password));
            check(mttl_remaining(100,100,5000)==5000);
            check(mttl_remaining(100,5099,5000)==1);
            check(!mttl_remaining(100,5100,5000));
            check(mttl_remaining(0xfffffff0u,0x10u,100)==68);
            check(mttl_wifi_form_type("application/x-www-form-urlencoded"));
            check(mttl_wifi_form_type(" Application/X-WWW-Form-Urlencoded; charset=UTF-8 "));
            check(!mttl_wifi_form_type(0));
            check(!mttl_wifi_form_type(""));
            check(!mttl_wifi_form_type("application/json"));
            check(!mttl_wifi_form_type("application/x-www-form-urlencoded-extra"));
            check(!mttl_wifi_form_type("application/x-www-form-urlencoded; charset=latin1"));
            check(!mttl_wifi_form_type("application/x-www-form-urlencoded; charset=utf-8; other=1"));
        }
    }
    {
        mttl_scan scan;
        uint8_t ssid[33]; unsigned i;
        for (i=0;i<33;i++) ssid[i]='A';
        mttl_scan_begin(&scan, 7);
        check(!mttl_scan_add(&scan,6,ssid,32,-43,0));
        check(!mttl_scan_add(&scan,7,ssid,33,-43,0));
        check(mttl_scan_add(&scan,7,ssid,32,-43,123) && scan.entries[0].ssid[32]==0 && scan.entries[0].security==123);
        for (i=1;i<MTTL_SCAN_CAP;i++) mttl_scan_add(&scan,7,ssid,1,-43,0);
        check(scan.count==MTTL_SCAN_CAP && !mttl_scan_add(&scan,7,ssid,1,-43,0) && scan.truncated);
        check(!mttl_scan_finish(&scan,6) && mttl_scan_finish(&scan,7));
        check(!mttl_scan_add(&scan,7,ssid,1,-43,0));
        mttl_scan_begin(&scan,8);
        check(!scan.count && !scan.truncated && scan.pending);
        ssid[0]=0; check(!mttl_scan_add(&scan,8,ssid,1,-43,0));
    }
    {mttl_protection_config c;mttl_protection_state p={0};mttl_protection_default(&c);
     check(mttl_protection_valid(&c)&&c.enabled==7);
     check(!mttl_protection_round(&p,&c,7,900,16000,3000000));
     check(mttl_protection_round(&p,&c,7,901,0,0)==1);
     check(!mttl_protection_round(&p,&c,7,0,16001,3000001));
     check(mttl_protection_round(&p,&c,7,0,16001,3000001)==6);
     check(!mttl_protection_round(&p,&c,7,0,16001,3000001));
     check(!mttl_protection_round(&p,&c,1,0,16001,3000001)&&p.error_mask==6&&!p.current_count&&!p.power_count);
     check(!mttl_protection_round(&p,&c,7,0,16001,3000001));
     check(!mttl_protection_round(&p,&c,7,0,0,0)&&!p.current_count&&!p.power_count);
     c.enabled=0;check(!mttl_protection_round(&p,&c,7,9999,99999,9999999)&&!p.error_mask);
     c.temperature_c=49;check(!mttl_protection_valid(&c));c.temperature_c=50;c.current_ma=999;check(!mttl_protection_valid(&c));
     c.current_ma=1000;c.power_w=99;check(!mttl_protection_valid(&c));c.power_w=100;check(mttl_protection_valid(&c));
    }
    {mttl_settings a,b;uint8_t bytes[1024];size_t n;mttl_settings_default(&a);
     a.protection.temperature_c=65;a.protection.current_ma=2300;a.protection.power_w=750;a.protection.enabled=5;
     a.relay_known=15;a.relay_on=2;a.energy_valid=1;a.energy_wh[0]=1234;
     n=mttl_settings_storage_pack(&a,bytes,sizeof(bytes));check(n&&mttl_settings_storage_unpack(bytes,n,&b));
     check(b.protection.temperature_c==65&&b.protection.current_ma==2300&&b.protection.power_w==750&&b.protection.enabled==5);
     check(b.relay_known==15&&b.relay_on==2&&b.energy_wh[0]==1234);
     n=mttl_settings_pack(&a,bytes,sizeof(bytes));bytes[0]=1;
     check(mttl_settings_unpack(bytes,n-57,&b)&&b.protection.enabled==7&&b.protection.temperature_c==90&&b.protection.current_ma==16000&&b.protection.power_w==3000);
     bytes[0]=5;bytes[n-57]=8;check(!mttl_settings_unpack(bytes,n,&b));
    }
    cleanup_test();
    mttl_integration_test();
    {extern void mttl_ntp_transport_test(void);mttl_ntp_transport_test();}
    {extern void mttl_ota_stage_test(void);mttl_ota_stage_test();}
    {extern void mttl_sdk_flash_test(void);mttl_sdk_flash_test();}
    {extern void mttl_ota_identity_test(void);mttl_ota_identity_test();}
    {extern void mttl_ota_session_test(void);mttl_ota_session_test();}
    return mttl_test_failed;
}
