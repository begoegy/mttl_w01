#include "mqtt_engine.h"
#include "mqtt_command.h"
#include "deadline.h"
#include <stdio.h>
#include <string.h>
static void clear_tx(mttl_mqtt_engine *e){volatile uint8_t *p=e->tx;size_t i;for(i=0;i<sizeof(e->tx);i++)p[i]=0;e->tx_size=e->tx_sent=0;}
void mttl_mqtt_engine_init(mttl_mqtt_engine *e){memset(e,0,sizeof(*e));mttl_mq_init(&e->session);mttl_mq_rx_reset(&e->rx);}
static void failed(mttl_mqtt_engine *e,uint32_t now){mttl_mq_failed(&e->session,e->session.generation,now);clear_tx(e);}
static int queue(mttl_mqtt_engine *e,size_t n,uint32_t now){if(!n||n>sizeof(e->tx))return 0;e->tx_size=n;e->tx_sent=0;e->tx_since=now;return 1;}
int mttl_mqtt_engine_publish(mttl_mqtt_engine *e,const char *topic,const uint8_t *p,size_t n,int retain,uint32_t now){if(e->session.state!=MTTL_MQ_ONLINE||e->tx_size||e->rediscover||e->phase>=10)return 0;return queue(e,mttl_mq_publish_packet(e->tx,sizeof(e->tx),topic,p,n,retain),now);}
void mttl_mqtt_engine_step(mttl_mqtt_engine *e,const mttl_settings *s,const char *id,int network,uint32_t revision,uint32_t now,const mttl_mqtt_io *io){
    char base[96],topic[128],payload[768],client[32];uint8_t incoming[64];size_t n,used,pos;int rc,accepted;mttl_mq_action action;mttl_command command;
    if(!e||!s||!io||!io->open||!io->close||!io->send||!io->receive)return;
    if(!mttl_ha_base(s,id,base,sizeof(base)))network=0;
    /* A disabled configuration briefly connects to remove retained Discovery.
     * Retry after network loss/reboot; never subscribe or execute commands. */
    if(s->mqtt_enabled)e->cleanup_done=0;
    mttl_mq_update(&e->session,network,s->mqtt_enabled||(!e->cleanup_done&&s->broker[0]),revision,now);action=mttl_mq_step(&e->session,now);
    if(action==MTTL_MQ_CLOSE){rc=io->close(io->context);if(rc==1){mttl_mq_stopped(&e->session,e->session.generation,now);clear_tx(e);mttl_mq_rx_reset(&e->rx);e->phase=e->ping_wait=e->rediscover=0;}return;}
    if(e->session.state==MTTL_MQ_OFF||e->session.state==MTTL_MQ_BACKOFF||e->session.state==MTTL_MQ_STOPPING||e->session.state==MTTL_MQ_FAULT)return;
    if(action==MTTL_MQ_CONNECT){e->phase=0;e->entity=0;clear_tx(e);mttl_mq_rx_reset(&e->rx);}
    if(e->session.state==MTTL_MQ_CONNECTING&&!e->phase){rc=io->open(io->context,s->broker,s->port);if(rc<0){failed(e,now);return;}if(!rc)return;
        snprintf(client,sizeof(client),"mttl2_%s",id);snprintf(topic,sizeof(topic),"%s/availability",base);
        if(!queue(e,mttl_mq_connect_packet(e->tx,512,client,topic,s->mqtt_user,s->mqtt_password,30),now)){failed(e,now);return;}e->phase=1;}
    /* Same owner also performs synchronous NOR checkpoints. Queue age is
     * not proof of a stalled socket; allow that bounded scheduling gap. */
    if(!s->mqtt_enabled&&!e->cleanup_done&&e->phase<10&&(e->session.state==MTTL_MQ_ONLINE||e->phase==6||e->phase==7||e->phase==8)){e->phase=10;e->entity=0;e->rediscover=0;}
    if(e->tx_size){if(!mttl_remaining(e->tx_since,now,10000)){failed(e,now);return;}rc=io->send(io->context,e->tx+e->tx_sent,e->tx_size-e->tx_sent);
        if(rc<0||(size_t)rc>e->tx_size-e->tx_sent){failed(e,now);return;}e->tx_sent+=(size_t)rc;if(e->tx_sent<e->tx_size)return;clear_tx(e);e->last_tx=now;
        if(e->phase==2)e->phase=3;else if(e->phase==4)e->phase=5;else if(e->phase==6)e->phase=7;
    }
    if(mttl_mq_rx_feed(&e->rx,0,0,now,&used)<0){failed(e,now);return;}
    rc=io->receive(io->context,incoming,sizeof(incoming));if(rc<0||rc>(int)sizeof(incoming)){failed(e,now);return;}
    pos=0;while(pos<(size_t)rc){int ready=mttl_mq_rx_feed(&e->rx,incoming+pos,(size_t)rc-pos,now,&used);pos+=used;if(ready<0){failed(e,now);return;}if(!ready)break;
        if(e->phase==1){if(!mttl_mq_connack(e->rx.bytes,e->rx.total,&accepted)||!accepted||!mttl_mq_connected(&e->session,e->session.generation,now)){failed(e,now);return;}if(!s->mqtt_enabled){e->phase=10;e->entity=0;}else{snprintf(topic,sizeof(topic),"%s/+/set",base);if(!queue(e,mttl_mq_subscribe_packet(e->tx,512,topic,1),now)){failed(e,now);return;}e->phase=2;}}
        else if(e->phase==3||e->phase==5){unsigned packet=e->phase==3?1:2;if(!mttl_mq_suback(e->rx.bytes,e->rx.total,(uint16_t)packet,&accepted)||!accepted){failed(e,now);return;}
            if(packet==1){if(!queue(e,mttl_mq_subscribe_packet(e->tx,512,"homeassistant/status",2),now)){failed(e,now);return;}e->phase=4;}else{e->phase=6;e->entity=0;}}
        else if(e->phase>=10){if(e->phase==12&&e->rx.total==2&&e->rx.bytes[0]==0xd0&&e->rx.bytes[1]==0){e->cleanup_done=1;e->ping_wait=0;}}
        else if(e->session.state==MTTL_MQ_ONLINE&&s->mqtt_enabled){
            if(e->rx.total==2&&e->rx.bytes[0]==0xd0&&e->rx.bytes[1]==0)e->ping_wait=0;
            else if(mttl_mq_command(&e->session,e->session.generation,base,e->rx.bytes,e->rx.total,&command)){if(io->command)io->command(io->command_context,command.channel,command.on);}
            else{mttl_publish p;if(mttl_parse_publish(e->rx.bytes,e->rx.total,&p)&&p.topic_size==20&&!memcmp(p.topic,"homeassistant/status",20)&&p.payload_size==6&&!memcmp(p.payload,"online",6)){e->rediscover=1;e->entity=0;}}
        }else if(e->session.state!=MTTL_MQ_SYNCING){failed(e,now);return;}mttl_mq_rx_reset(&e->rx);
    }
    if(e->phase>=10){
        if(e->cleanup_done)return;
        if(e->phase==12){if(!mttl_remaining(e->ping_at,now,10000))failed(e,now);return;}
        if(e->tx_size)return;
        if(e->phase==10&&e->entity<MTTL_HA_ENTITIES){n=mttl_ha_discovery(s,id,e->entity,topic,sizeof(topic),payload,sizeof(payload));if(!n||!queue(e,mttl_mq_publish_packet(e->tx,sizeof(e->tx),topic,0,0,1),now)){failed(e,now);return;}++e->entity;return;}
        if(e->phase==10){snprintf(topic,sizeof(topic),"%s/availability",base);if(!queue(e,mttl_mq_publish_packet(e->tx,sizeof(e->tx),topic,0,0,1),now)){failed(e,now);return;}e->phase=11;return;}
        /* TCP ordering + PINGRESP fences all preceding retained deletions. */
        e->tx[0]=0xc0;e->tx[1]=0;queue(e,2,now);e->ping_at=now;e->phase=12;return;
    }
    if(e->phase==6||e->rediscover){if(e->tx_size)return;
        if(e->entity<MTTL_HA_ENTITIES){n=mttl_ha_discovery(s,id,e->entity,topic,sizeof(topic),payload,sizeof(payload));if(!n||!queue(e,mttl_mq_publish_packet(e->tx,sizeof(e->tx),topic,(uint8_t *)payload,n,1),now)){failed(e,now);return;}++e->entity;e->phase=6;}
        else{snprintf(topic,sizeof(topic),"%s/availability",base);if(!queue(e,mttl_mq_publish_packet(e->tx,512,topic,(const uint8_t *)"online",6,1),now)){failed(e,now);return;}e->phase=8;e->rediscover=0;}return;}
    if(e->phase==7){e->phase=6;return;}
    if(e->phase==8&&!e->tx_size){if(e->session.state==MTTL_MQ_SYNCING&&!mttl_mq_ready(&e->session,e->session.generation,now)){failed(e,now);return;}e->phase=9;}
    if(e->session.state==MTTL_MQ_ONLINE){if(e->ping_wait&&!mttl_remaining(e->ping_at,now,10000)){failed(e,now);return;}if(!e->tx_size&&!e->ping_wait&&!mttl_remaining(e->last_tx,now,20000)){e->tx[0]=0xc0;e->tx[1]=0;queue(e,2,now);e->ping_wait=1;e->ping_at=now;}}
}
