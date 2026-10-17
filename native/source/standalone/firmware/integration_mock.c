#include "device_app.h"
#include "ota_guard.h"
#include "ota_stream.h"
#include "ota_hash.h"
#include "ota_readback.h"
#include "ota_stage.h"
#include <string.h>
char mttl_test_ha_json[25][768],mttl_test_ha_topics[25][128];
char mttl_test_device_json[2048],mttl_test_config_json[2048];
extern unsigned mttl_test_count,mttl_test_failed,mttl_failed_ids[64];
static void check(int p){++mttl_test_count;if(!p&&mttl_test_failed<64)mttl_failed_ids[mttl_test_failed++]=mttl_test_count;}
typedef struct {const uint8_t *data;uint32_t base,size,calls;int failed,corrupt;} readback_mock;
static int readback_read(void *context,uint32_t address,uint8_t *out,size_t n){readback_mock *m=context;m->calls++;if(m->failed||address<m->base||address-m->base>m->size||n>m->size-(address-m->base))return 0;memcpy(out,m->data+address-m->base,n);if(m->corrupt&&n)out[n-1]^=1;return 1;}
typedef struct {uint8_t bytes[4096];unsigned calls,fail;int corrupt,range_bad;} stage_mock;
static stage_mock staged;
static int stage_range(stage_mock *m,uint32_t a,size_t n){if(a<0x80000||a>0x81000||n>0x81000-a){m->range_bad=1;return 0;}return ++m->calls!=m->fail;}
static int stage_erase(void *p,uint32_t a){stage_mock *m=p;if(!stage_range(m,a,4096))return 0;memset(m->bytes,255,4096);return 1;}
static int stage_write(void *p,uint32_t a,const uint8_t *d,size_t n){stage_mock *m=p;if(n>256||!stage_range(m,a,n))return 0;memcpy(m->bytes+a-0x80000,d,n);return 1;}
static int stage_read(void *p,uint32_t a,uint8_t *d,size_t n){stage_mock *m=p;if(!stage_range(m,a,n))return 0;memcpy(d,m->bytes+a-0x80000,n);if(m->corrupt&&n)d[n-1]^=1;return 1;}
typedef struct {uint8_t tx[1024],rx[512];size_t tx_n,rx_n;unsigned discovery,online,pings,commands,closed,deletions;uint8_t deny_connect,deny_subscribe,stall_tx,fail_rx,drop_ping;} broker;
static broker b;
static mttl_device_app app;
static int open_mock(void *p,const char *host,uint16_t port){(void)p;return !strcmp(host,"192.168.0.4")&&port==1883?1:-1;}
static int close_mock(void *p){broker *v=p;++v->closed;v->tx_n=v->rx_n=0;return 1;}
static int send_mock(void *p,const uint8_t *data,size_t n){broker *v=p;size_t i,total,mul=1,body=0,take=n>7?7:n;
 if(v->stall_tx)return 0;if(take>sizeof(v->tx)-v->tx_n)return -1;
 memcpy(v->tx+v->tx_n,data,take);v->tx_n+=take;if(v->tx_n<2)return (int)take;
 for(i=1;i<v->tx_n;i++){body+=(v->tx[i]&127)*mul;mul*=128;if(!(v->tx[i]&128))break;}if(i==v->tx_n)return (int)take;total=i+1+body;if(v->tx_n<total)return (int)take;
 if(v->tx[0]==0x10){const uint8_t ack[]={0x20,2,0,0};memcpy(v->rx,ack,4);v->rx[3]=v->deny_connect?5:0;v->rx_n=4;}
 else if(v->tx[0]==0x82){v->rx[0]=0x90;v->rx[1]=3;v->rx[2]=v->tx[i+1];v->rx[3]=v->tx[i+2];v->rx[4]=v->deny_subscribe?0x80:0;v->rx_n=5;}
 else if((v->tx[0]&0xf0)==0x30){mttl_publish pub;if(mttl_parse_publish(v->tx,total,&pub)){if(!pub.payload_size&&pub.retained)++v->deletions;else if(pub.topic_size>14&&!memcmp(pub.topic,"homeassistant/",14))++v->discovery;else if(pub.payload_size==6&&!memcmp(pub.payload,"online",6))++v->online;}}
 else if(v->tx[0]==0xc0){if(!v->drop_ping){v->rx[0]=0xd0;v->rx[1]=0;v->rx_n=2;}++v->pings;}
 v->tx_n=0;return (int)take;
}
static int recv_mock(void *p,uint8_t *data,size_t n){broker *v=p;size_t take=v->rx_n>n?n:v->rx_n;if(v->fail_rx)return -1;memcpy(data,v->rx,take);memmove(v->rx,v->rx+take,v->rx_n-take);v->rx_n-=take;return (int)take;}
static int command_mock(void *p,uint8_t ch,uint8_t on){broker *v=p;if(ch==3&&on)++v->commands;return 1;}
void mttl_integration_test(void){mttl_settings s;mttl_mqtt_engine e;mttl_mqtt_io io;unsigned i;uint32_t now=0;char tiny[2];
 memset(&b,0,sizeof(b));mttl_settings_default(&s);strcpy(s.broker,"192.168.0.4");s.mqtt_enabled=1;
 for(i=0;i<25;i++)check(mttl_ha_discovery(&s,"aabbccddeeff",i,mttl_test_ha_topics[i],128,mttl_test_ha_json[i],768)>0);
 for(i=0;i<25;i++)check(i<5?!strstr(mttl_test_ha_json[i],"\"exp_aft\""):strstr(mttl_test_ha_json[i],"\"exp_aft\":30,")!=0);
 check(!mttl_ha_discovery(&s,"bad",0,tiny,2,tiny,2));check(!mttl_ha_discovery(&s,"aabbccddeeff",25,tiny,2,tiny,2));
 {char json[768],topic[128];uint8_t packet[1024];size_t n;memset(s.device,'"',64);s.device[64]=0;memset(s.topic,'x',64);s.topic[64]=0;
 n=mttl_ha_discovery(&s,"aabbccddeeff",0,topic,sizeof(topic),json,sizeof(json));check(n>0);check(mttl_mq_publish_packet(packet,sizeof(packet),topic,(const uint8_t *)json,n,1)>0);mttl_settings_default(&s);strcpy(s.broker,"192.168.0.4");s.mqtt_enabled=1;}
 memset(&io,0,sizeof(io));io.context=&b;io.command_context=&b;io.open=open_mock;io.close=close_mock;io.send=send_mock;io.receive=recv_mock;io.command=command_mock;mttl_mqtt_engine_init(&e);
 for(i=0;i<4000&&e.session.state!=MTTL_MQ_ONLINE;i++,now++)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,now,&io);
 check(e.session.state==MTTL_MQ_ONLINE);check(b.discovery==25&&b.online==1);
 b.rx_n=mttl_mq_publish_packet(b.rx,512,"mttl/standalone/aabbccddeeff/4/set",(const uint8_t *)"ON",2,0);
 mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,++now,&io);check(b.commands==1);
 b.rx_n=mttl_mq_publish_packet(b.rx,512,"homeassistant/status",(const uint8_t *)"online",6,1);
 for(i=0;i<4000&&b.online<2;i++,now++)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,now,&io);check(b.discovery==50&&b.online==2);
 b.rx_n=mttl_mq_publish_packet(b.rx,512,"mttl/standalone/aabbccddeeff/4/set",(const uint8_t *)"ON",2,1);
 mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,++now,&io);check(b.commands==1);
 now+=21000;for(i=0;i<5;i++)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,now+i,&io);check(b.pings==1&&!e.ping_wait);
 mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",0,1,now+10,&io);check(b.closed==1);
 /* Full connect/delete/fence/disconnect with seven-byte partial sends. */
 memset(&b,0,sizeof(b));mttl_mqtt_engine_init(&e);s.mqtt_enabled=0;
 for(i=0;i<4000&&!e.cleanup_done;i++)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,2,i,&io);
 check(e.cleanup_done&&b.deletions==26&&!b.discovery&&!b.online&&b.pings==1);
 mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,2,i+1,&io);
 check(b.closed==1&&e.session.state==MTTL_MQ_OFF);
 s.mqtt_enabled=1;
 for(now=i+2;now<i+4000&&e.session.state!=MTTL_MQ_ONLINE;now++)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,3,now,&io);
 check(e.session.state==MTTL_MQ_ONLINE&&b.discovery==25&&b.online==1);
 /* Missing fence response must retry, not mark cleanup complete. */
 s.mqtt_enabled=0;b.drop_ping=1;
 for(i=0;i<14000;i+=10)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,4,now+i,&io);
 check(!e.cleanup_done&&b.closed>1&&b.deletions>=52);
 s.mqtt_enabled=1;
 mttl_device_init(&app,1700000000);check(!app.hardware.verified&&!mttl_device_command(&app,0,1));
 check(!mttl_hardware_begin(&app.hardware,0,1));
 {mttl_ota_hash h;uint8_t expected[32],digest[32];
 check(mttl_ota_hash_parse("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",expected));
 mttl_ota_hash_init(&h);check(mttl_ota_hash_update(&h,(const uint8_t *)"a",1));check(mttl_ota_hash_update(&h,(const uint8_t *)"bc",2));check(mttl_ota_hash_finish(&h,digest)&&!memcmp(digest,expected,32));
 check(!mttl_ota_hash_parse("bad",expected));check(!mttl_ota_hash_parse("za7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",expected));
 check(mttl_ota_hash_parse("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",expected));mttl_ota_hash_init(&h);check(mttl_ota_hash_finish(&h,digest)&&!memcmp(digest,expected,32));
 }
 {mttl_ota_layout l={0xb000,0x80000,0xf0000,0x100000};mttl_ota_plan p;
 check(mttl_ota_plan_make(&l,0xb000,276160,&p)&&p.target==0x80000&&p.erase_end<=0xf0000);
 check(mttl_ota_plan_make(&l,0x80000,276160,&p)&&p.target==0xb000&&p.erase_end<=0x80000);
 check(!mttl_ota_plan_make(&l,0,276160,&p));check(!mttl_ota_plan_make(&l,0xb000,0x70001,&p));check(!mttl_ota_plan_make(&l,0xb000,0xffffffffu,&p));
 check(mttl_ota_signature((const uint8_t *)"81958711")==MTTL_OTA_NEW);check(mttl_ota_signature((const uint8_t *)"01958711")==MTTL_OTA_OLD);
 check(mttl_ota_signature((const uint8_t *)"8195\xff\xff\xff\xff")==MTTL_OTA_INVALID);check(mttl_ota_signature(0)==MTTL_OTA_INVALID);
 check(!mttl_ota_bin_validate(0,44));check(!mttl_ota_bin_validate((const uint8_t *)"bad",3));
 {mttl_ota_commit c;uint32_t address,value;unsigned step;
 check(!mttl_ota_commit_init(&c,&l,0xb000,0x80000,0));check(!mttl_ota_commit_action(&c,&address,&value));
 check(!mttl_ota_commit_init(&c,&l,0xb000,0xb000,1));
 check(mttl_ota_commit_init(&c,&l,0xb000,0x80000,1));
 for(step=0;step<3;step++){check(!mttl_ota_commit_done(&c));check(mttl_ota_commit_action(&c,&address,&value));check(address==(step==2?0xb008:step==1?0x8000c:0x80008));check(mttl_ota_commit_ack(&c,1,value));}
 check(mttl_ota_commit_done(&c));check(!mttl_ota_commit_action(&c,&address,&value));check(!mttl_ota_commit_ack(&c,1,value));
 for(step=0;step<3;step++){unsigned i;check(mttl_ota_commit_init(&c,&l,0x80000,0xb000,1));for(i=0;i<step;i++){mttl_ota_commit_action(&c,&address,&value);mttl_ota_commit_ack(&c,1,value);}mttl_ota_commit_action(&c,&address,&value);check(!mttl_ota_commit_ack(&c,1,value^1));check(!mttl_ota_commit_done(&c));check(!mttl_ota_commit_action(&c,&address,&value));}
 check(mttl_ota_commit_init(&c,&l,0xb000,0x80000,1));check(!mttl_ota_commit_ack(&c,0,0x35393138));check(!mttl_ota_commit_done(&c));
 }
 {uint8_t image[64]={44,0,0,0,0,0x60,0,0x10},signature[8];uint32_t sum=0;unsigned k;
 static const uint8_t tail[20]={0x52,0x54,0x4b,0x57,0x69,0x6e,0,0xff,0,1,1,0,0x95,0x81,1,1,0,0,0,0};
 memset(image+8,255,8);image[16]=0x19;image[17]=0x60;image[19]=0x10;memcpy(image+20,tail,20);
 for(k=0;k<60;k++)sum+=image[k];for(k=0;k<4;k++)image[60+k]=(uint8_t)(sum>>(8*k));
 check(mttl_ota_bin_validate(image,64));image[41]^=1;check(!mttl_ota_bin_validate(image,64));image[41]^=1;
 {mttl_ota_stage stage;mttl_ota_stage_io io={&staged,stage_erase,stage_write,stage_read};mttl_ota_hash h;uint8_t digest[32];unsigned chunk,pos,n,cut;
 mttl_ota_hash_init(&h);mttl_ota_hash_update(&h,image,64);mttl_ota_hash_finish(&h,digest);
 for(chunk=1;chunk<=17;chunk++){memset(&staged,0,sizeof(staged));check(mttl_ota_stage_begin(&stage,&l,0xb000,64,digest,&io));
 for(pos=0;pos<64;pos+=n){n=64-pos;if(n>chunk)n=chunk;if(!mttl_ota_stage_feed(&stage,image+pos,n))break;}
 check(pos==64&&mttl_ota_stage_finish(&stage)&&stage.state==MTTL_STAGE_VERIFIED&&!staged.range_bad);
 check(!mttl_ota_stage_feed(&stage,image,1)&&!mttl_ota_stage_finish(&stage));check(!memcmp(staged.bytes+8,"\xff\xff\xff\xff\xff\xff\xff\xff",8));}
 for(cut=1;cut<=3;cut++){memset(&staged,0,sizeof(staged));staged.fail=cut;
 if(mttl_ota_stage_begin(&stage,&l,0xb000,64,digest,&io)&&mttl_ota_stage_feed(&stage,image,64))mttl_ota_stage_finish(&stage);
 check(stage.state==MTTL_STAGE_FAILED&&!staged.range_bad);check(!mttl_ota_stage_feed(&stage,image,1));}
 memset(&staged,0,sizeof(staged));check(mttl_ota_stage_begin(&stage,&l,0xb000,64,digest,&io));check(mttl_ota_stage_feed(&stage,image,63));check(!mttl_ota_stage_finish(&stage)&&stage.state==MTTL_STAGE_FAILED);
 memset(&staged,0,sizeof(staged));check(mttl_ota_stage_begin(&stage,&l,0xb000,64,digest,&io));image[8]='8';check(!mttl_ota_stage_feed(&stage,image,64)&&staged.calls==1);image[8]=255;
 memset(&staged,0,sizeof(staged));check(mttl_ota_stage_begin(&stage,&l,0xb000,64,digest,&io));check(!mttl_ota_stage_feed(&stage,image,65)&&staged.calls==1);
 memset(&staged,0,sizeof(staged));digest[0]^=1;check(mttl_ota_stage_begin(&stage,&l,0xb000,64,digest,&io));check(mttl_ota_stage_feed(&stage,image,64));check(!mttl_ota_stage_finish(&stage));digest[0]^=1;
 memset(&staged,0,sizeof(staged));staged.corrupt=1;check(mttl_ota_stage_begin(&stage,&l,0xb000,64,digest,&io));check(mttl_ota_stage_feed(&stage,image,64));check(!mttl_ota_stage_finish(&stage));
 memset(&staged,0,sizeof(staged));check(!mttl_ota_stage_begin(&stage,&l,0,64,digest,&io)&&!staged.calls);
 check(mttl_ota_stage_begin(&stage,&l,0xb000,64,digest,&io));mttl_ota_stage_abort(&stage);check(!mttl_ota_stage_feed(&stage,image,64));
 }
 {mttl_ota_hash hash;uint8_t digest[32];readback_mock mock={image,0x80000,64,0,0,0};
 mttl_ota_hash_init(&hash);mttl_ota_hash_update(&hash,image,64);mttl_ota_hash_finish(&hash,digest);
 check(mttl_ota_readback(&l,0xb000,0x80000,64,digest,readback_read,&mock));
 digest[0]^=1;check(!mttl_ota_readback(&l,0xb000,0x80000,64,digest,readback_read,&mock));digest[0]^=1;
 mock.failed=1;check(!mttl_ota_readback(&l,0xb000,0x80000,64,digest,readback_read,&mock));mock.failed=0;mock.corrupt=1;check(!mttl_ota_readback(&l,0xb000,0x80000,64,digest,readback_read,&mock));mock.corrupt=0;
 mock.calls=0;check(!mttl_ota_readback(&l,0xb000,0xb000,64,digest,readback_read,&mock)&&!mock.calls);check(!mttl_ota_readback(&l,0,0x80000,64,digest,readback_read,&mock)&&!mock.calls);
 }
 {mttl_ota_stream stream;unsigned chunk,pos,n;
 for(chunk=1;chunk<=9;chunk++){check(mttl_ota_stream_init(&stream,64));for(pos=0;pos<64;pos+=n){n=64-pos;if(n>chunk)n=chunk;if(!mttl_ota_stream_feed(&stream,image+pos,n))break;}check(pos==64&&mttl_ota_stream_finish(&stream));}
 check(!mttl_ota_stream_init(&stream,0));check(!mttl_ota_stream_init(&stream,0xffffffffu));
 check(mttl_ota_stream_init(&stream,64));check(mttl_ota_stream_feed(&stream,image,63));check(!mttl_ota_stream_finish(&stream));
 check(mttl_ota_stream_init(&stream,64));check(!mttl_ota_stream_feed(&stream,image,65));
 check(mttl_ota_stream_init(&stream,64));image[41]^=1;check(mttl_ota_stream_feed(&stream,image,64));check(!mttl_ota_stream_finish(&stream));image[41]^=1;
 }
 for(k=1;k<8;k++){memset(signature,255,8);memcpy(signature,"81958711",k);check(mttl_ota_signature(signature)==MTTL_OTA_INVALID);}
 }
 }
 {mttl_button_input input;mttl_button_input_init(&input,30);
 check(!mttl_button_input_poll(&input,1,1,0));check(!mttl_button_input_poll(&input,1,0,100));check(!mttl_button_input_poll(&input,1,0,130)&&input.armed);
 check(!mttl_button_input_poll(&input,1,1,200));check(!mttl_button_input_poll(&input,1,0,210));check(!mttl_button_input_poll(&input,1,0,240));
 check(!mttl_button_input_poll(&input,1,1,300));check(!mttl_button_input_poll(&input,1,1,330));check(!mttl_button_input_poll(&input,1,0,400));check(mttl_button_input_poll(&input,1,0,430)==1);
 check(!mttl_button_input_poll(&input,1,1,500));check(!mttl_button_input_poll(&input,1,1,530));check(!mttl_button_input_poll(&input,1,0,10500));check(mttl_button_input_poll(&input,1,0,10530)==2);
 check(!mttl_button_input_poll(&input,0,0,11000)&&!input.armed);check(!mttl_button_input_poll(&input,1,1,12000));check(!mttl_button_input_poll(&input,1,0,12100));check(!mttl_button_input_poll(&input,1,0,12130)&&input.armed);
 mttl_button_input_init(&input,30);check(!mttl_button_input_poll(&input,1,0,0xffffffc0u));check(!mttl_button_input_poll(&input,1,0,0xffffffe0u));check(!mttl_button_input_poll(&input,1,1,0xfffffff0u));check(!mttl_button_input_poll(&input,1,1,0x10));check(!mttl_button_input_poll(&input,1,0,0x30));check(mttl_button_input_poll(&input,1,0,0x50)==1);
 }
 {mttl_button_input p;mttl_button_input_init(&p,30);
 check(!mttl_main_button_input_poll(&p,1,0,0));check(!mttl_main_button_input_poll(&p,1,0,30));
 check(!mttl_main_button_input_poll(&p,1,1,40));check(!mttl_main_button_input_poll(&p,1,1,70));
 check(!mttl_main_button_input_poll(&p,1,1,10069));check(mttl_main_button_input_poll(&p,1,1,10070)==2);
 check(!mttl_main_button_input_poll(&p,1,1,15000));check(!mttl_main_button_input_poll(&p,1,0,15010));check(!mttl_main_button_input_poll(&p,1,0,15040));
 check(!mttl_main_button_input_poll(&p,1,1,16000));check(!mttl_main_button_input_poll(&p,1,1,16030));check(!mttl_main_button_input_poll(&p,1,0,16060));check(mttl_main_button_input_poll(&p,1,0,16090)==1);
 check(!mttl_main_button_input_poll(&p,0,1,17000)&&!p.armed&&!p.hold_notified);
 mttl_button_input_init(&p,30);check(!mttl_main_button_input_poll(&p,1,0,0xffffff00u));check(!mttl_main_button_input_poll(&p,1,0,0xffffff20u));
 check(!mttl_main_button_input_poll(&p,1,1,0xfffffff0u));check(!mttl_main_button_input_poll(&p,1,1,0x0eu));
 check(!mttl_main_button_input_poll(&p,1,1,0x271du));check(mttl_main_button_input_poll(&p,1,1,0x271eu)==2);
 check(!mttl_main_button_input_poll(&p,1,0,0x2720u));check(!mttl_main_button_input_poll(&p,1,0,0x2740u));
 p.button.down=p.armed=p.stable=1;p.button.pressed_at=0;p.hold_notified=0;p.debounce_ms=0;check(!mttl_main_button_input_poll(&p,1,1,20000));
 p.debounce_ms=0x80000000u;check(!mttl_main_button_input_poll(&p,1,1,20000));
 }
 mttl_hardware_sample(&app.samples.voltage,0,2200,0);check(!app.samples.voltage.valid);
 {uint8_t body[1024];size_t n;strcpy(s.wifi_password,"wifi-secret");strcpy(s.ssid,"SSID");strcpy(s.mqtt_user,"user");strcpy(s.mqtt_password,"mqtt-secret");n=mttl_settings_pack(&s,body,sizeof(body));check(n&&mttl_device_configure(&app,body,n,0,0));check(app.revision==1&&!app.persistent);check(!mttl_device_configure(&app,body,n-1,0,0)&&app.revision==1);}
 check(mttl_device_status(&app,0,mttl_test_device_json,sizeof(mttl_test_device_json))>0);
 {unsigned ch;size_t size;static char worst[1536];
 memset(app.settings.device,'"',64);app.settings.device[64]=0;app.revision=0xffffffffu;
 for(ch=0;ch<4;ch++){memset(app.settings.channel[ch],'\\',64);app.settings.channel[ch][64]=0;mttl_sample_update(&app.samples.power[ch],INT32_MIN,1,0);mttl_sample_update(&app.samples.meter[ch],INT32_MIN,1,0);mttl_sample_update(&app.samples.current[ch],INT32_MIN,1,0);mttl_sample_update(&app.samples.temperature[ch],INT32_MIN,1,0);}
 mttl_sample_update(&app.samples.total_power,INT32_MIN,1,0);mttl_sample_update(&app.samples.total_meter,INT32_MIN,1,0);mttl_sample_update(&app.samples.total_current,INT32_MIN,1,0);mttl_sample_update(&app.samples.voltage,INT32_MIN,1,0);
 size=mttl_device_status(&app,0,worst,sizeof(worst));check(size>0&&size<sizeof(worst));check(!mttl_device_status(&app,0,worst,size));
 mttl_device_init(&app,1700000000);strcpy(app.settings.wifi_password,"wifi-secret");strcpy(app.settings.mqtt_password,"mqtt-secret");
 }
 check(mttl_device_settings_json(&app,mttl_test_config_json,sizeof(mttl_test_config_json))>0);
 {mttl_publish message;mttl_device_init(&app,1700000000);app.mqtt.session.state=MTTL_MQ_ONLINE;
  app.control.known_mask=app.mqtt_known=15;app.mqtt_all=1;
  check(!mttl_device_mqtt_pending(&app));
  app.control.on_mask=app.mqtt_on=15;check(mttl_device_mqtt_pending(&app));
  app.mqtt_was_online=1;check(!mttl_device_mqtt_pending(&app));
  app.mqtt.tx_size=1;check(mttl_device_mqtt_pending(&app));app.mqtt.tx_size=0;
  app.mqtt_known=7;check(mttl_device_mqtt_pending(&app));
  app.control.on_mask=app.mqtt_on=0;app.mqtt_all=app.mqtt_was_online=0;
  app.control.known_mask=15;app.mqtt_known=15;app.publish_index=20;app.control.on_mask=2;
  mttl_device_tick(&app,"aabbccddeeff",1,1,0);
  check(mttl_parse_publish(app.mqtt.tx,app.mqtt.tx_size,&message));
  check(message.topic_size>=4&&!memcmp(message.topic+message.topic_size-4,"/sw2",4));
  check(message.payload_size==2&&!memcmp(message.payload,"ON",2));
  check(app.mqtt.tx[0]&1); /* HA late subscribers receive completed switch state. */
  check(app.mqtt_on==2&&app.publish_at==1);
  check(app.publish_index==20); /* Switch publication preserves sensor cursor. */
  /* Full output queue must not mark a newer state as already published. */
  app.control.on_mask=0;mttl_device_tick(&app,"aabbccddeeff",1,2,0);check(app.mqtt_on==2);
  app.mqtt.tx_size=0;mttl_device_tick(&app,"aabbccddeeff",1,3,0);
  check(mttl_parse_publish(app.mqtt.tx,app.mqtt.tx_size,&message));
  check(message.payload_size==3&&!memcmp(message.payload,"OFF",3));
  check(app.mqtt.tx[0]&1);
  check(app.publish_index==20);
  /* Sequential completions must not republish the other channels' old OFF. */
  app.mqtt.tx_size=0;app.mqtt_all=1;app.control.on_mask=1;
  mttl_device_tick(&app,"aabbccddeeff",1,200,0);
  check(mttl_parse_publish(app.mqtt.tx,app.mqtt.tx_size,&message));
  check(message.topic_size>=4&&!memcmp(message.topic+message.topic_size-4,"/sw1",4));
  app.mqtt.tx_size=0;mttl_device_tick(&app,"aabbccddeeff",1,400,0);
  check(!app.mqtt.tx_size);
  check(app.publish_index==21); /* Invalid sensor still advances rotation. */
  app.control.on_mask=3;mttl_device_tick(&app,"aabbccddeeff",1,401,0);
  check(mttl_parse_publish(app.mqtt.tx,app.mqtt.tx_size,&message));
  check(message.topic_size>=4&&!memcmp(message.topic+message.topic_size-4,"/sw2",4));
  app.mqtt.tx_size=0;app.control.on_mask=7;mttl_device_tick(&app,"aabbccddeeff",1,402,0);
  check(mttl_parse_publish(app.mqtt.tx,app.mqtt.tx_size,&message));
  check(message.topic_size>=4&&!memcmp(message.topic+message.topic_size-4,"/sw3",4));
  check(app.publish_index==21); /* Consecutive switch completions never rewind. */
  /* Sensors remain transient; retained stale measurements must not be replayed. */
  app.mqtt.tx_size=0;app.mqtt_on=app.control.on_mask;app.mqtt_all=1;
  app.publish_index=5;mttl_sample_update(&app.samples.total_power,1234,1,700);
  mttl_device_tick(&app,"aabbccddeeff",1,700,0);
  check(mttl_parse_publish(app.mqtt.tx,app.mqtt.tx_size,&message));
  check(!(app.mqtt.tx[0]&1));
  /* A HA birth rediscovery also schedules unchanged confirmed states again. */
  app.mqtt.tx_size=0;app.mqtt.rediscover=1;
  mttl_device_tick(&app,"aabbccddeeff",1,701,0);
  check(!app.mqtt_known&&!app.mqtt_all&&!app.mqtt.tx_size);
  app.mqtt.rediscover=0;mttl_device_tick(&app,"aabbccddeeff",1,702,0);
  check(mttl_parse_publish(app.mqtt.tx,app.mqtt.tx_size,&message));
  check(message.topic_size>=4&&!memcmp(message.topic+message.topic_size-4,"/sw1",4));
  check(app.mqtt.tx[0]&1);
  app.mqtt.session.state=MTTL_MQ_OFF;mttl_device_tick(&app,"aabbccddeeff",0,4,0);check(!app.mqtt_known&&!app.mqtt_all);
  mttl_device_init(&app,1700000000);strcpy(app.settings.wifi_password,"wifi-secret");strcpy(app.settings.mqtt_password,"mqtt-secret");
 }
 check(!strstr(mttl_test_config_json,"wifi-secret")&&!strstr(mttl_test_config_json,"mqtt-secret"));
 /* Fault injection: never reaches ONLINE on broker denial or stalled TX. */
 for(i=0;i<3;i++){unsigned step;memset(&b,0,sizeof(b));mttl_mqtt_engine_init(&e);if(i==0)b.deny_connect=1;else if(i==1)b.deny_subscribe=1;else b.stall_tx=1;
  /* Advance modeled time by 10ms; timeout coverage need not busy-poll every ms. */
  for(step=0;step<1200;step++)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,step*10,&io);
  check(e.session.state!=MTTL_MQ_ONLINE&&b.online==0&&b.closed>0);
 }
 /* A queued state survives a normal multi-second flash scheduling gap,
  * while a genuinely stalled transmitter still fails at its bound. */
 memset(&b,0,sizeof(b));mttl_mqtt_engine_init(&e);for(i=0;i<4000&&e.session.state!=MTTL_MQ_ONLINE;i++)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,i,&io);
 check(e.session.state==MTTL_MQ_ONLINE);
 now=i+1;check(mttl_mqtt_engine_publish(&e,"mttl/standalone/aabbccddeeff/sw1",(const uint8_t *)"ON",2,1,now));
 b.stall_tx=1;mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,now+5000,&io);
 check(e.session.state==MTTL_MQ_ONLINE&&e.tx_size>0);
 b.stall_tx=0;for(i=0;i<100&&e.tx_size;i++)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,now+5001+i,&io);
 check(e.session.state==MTTL_MQ_ONLINE&&!e.tx_size);
 now+=6000;check(mttl_mqtt_engine_publish(&e,"mttl/standalone/aabbccddeeff/sw1",(const uint8_t *)"OFF",3,1,now));
 b.stall_tx=1;mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,now+10000,&io);
 check(e.session.state!=MTTL_MQ_ONLINE);
 memset(&b,0,sizeof(b));mttl_mqtt_engine_init(&e);for(i=0;i<4000&&e.session.state!=MTTL_MQ_ONLINE;i++)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,i,&io);
 check(e.session.state==MTTL_MQ_ONLINE);b.fail_rx=1;mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,i+1,&io);mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,i+2,&io);check(b.closed>0&&e.session.state!=MTTL_MQ_ONLINE);
 memset(&b,0,sizeof(b));mttl_mqtt_engine_init(&e);for(i=0;i<4000&&e.session.state!=MTTL_MQ_ONLINE;i++)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,i,&io);
 b.drop_ping=1;for(now=i+21000;now<i+32000;now+=100)mttl_mqtt_engine_step(&e,&s,"aabbccddeeff",1,1,now,&io);check(b.pings==1&&b.closed>0);
}
