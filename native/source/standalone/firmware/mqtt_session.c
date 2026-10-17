#include "mqtt_session.h"
#include "deadline.h"
static void stop(mttl_mqtt_session *s,uint32_t now){s->state=MTTL_MQ_STOPPING;s->since=now;}
void mttl_mq_init(mttl_mqtt_session *s){s->state=MTTL_MQ_OFF;s->generation=s->revision=s->since=s->delay=0;s->desired=s->failures=0;}
void mttl_mq_update(mttl_mqtt_session *s,int network,int enabled,uint32_t revision,uint32_t now){
    uint8_t desired=network&&enabled;
    if(s->desired==desired&&s->revision==revision)return;
    s->desired=desired;s->revision=revision;s->failures=0;s->delay=0;
    if(s->state==MTTL_MQ_FAULT)return; /* cannot reuse unquiesced transport */
    if(s->state==MTTL_MQ_OFF){if(desired){s->state=MTTL_MQ_BACKOFF;s->since=now;}}
    else if(s->state==MTTL_MQ_BACKOFF){s->since=now;if(!desired)s->state=MTTL_MQ_OFF;}
    else if(s->state!=MTTL_MQ_STOPPING)stop(s,now);
}
mttl_mq_action mttl_mq_step(mttl_mqtt_session *s,uint32_t now){
    if(s->state==MTTL_MQ_STOPPING){if(!mttl_remaining(s->since,now,5000)){s->state=MTTL_MQ_FAULT;return MTTL_MQ_NONE;}return MTTL_MQ_CLOSE;}
    if(s->state==MTTL_MQ_BACKOFF&&s->desired&&!mttl_remaining(s->since,now,s->delay)){++s->generation;s->state=MTTL_MQ_CONNECTING;s->since=now;return MTTL_MQ_CONNECT;}
    if((s->state==MTTL_MQ_CONNECTING||s->state==MTTL_MQ_SYNCING)&&!mttl_remaining(s->since,now,10000))mttl_mq_failed(s,s->generation,now);
    return MTTL_MQ_NONE;
}
int mttl_mq_stopped(mttl_mqtt_session *s,uint32_t generation,uint32_t now){
    if(s->state!=MTTL_MQ_STOPPING||s->generation!=generation||!mttl_remaining(s->since,now,5000))return 0;
    s->state=s->desired?MTTL_MQ_BACKOFF:MTTL_MQ_OFF;s->since=now;return 1;
}
int mttl_mq_connected(mttl_mqtt_session *s,uint32_t generation,uint32_t now){
    if(s->state!=MTTL_MQ_CONNECTING||s->generation!=generation||!mttl_remaining(s->since,now,10000))return 0;
    s->state=MTTL_MQ_SYNCING;s->since=now;return 1;
}
int mttl_mq_ready(mttl_mqtt_session *s,uint32_t generation,uint32_t now){
    if(s->state!=MTTL_MQ_SYNCING||s->generation!=generation||!mttl_remaining(s->since,now,10000))return 0;
    s->state=MTTL_MQ_ONLINE;s->failures=0;s->delay=0;return 1;
}
void mttl_mq_failed(mttl_mqtt_session *s,uint32_t generation,uint32_t now){
    if(s->generation!=generation||(s->state!=MTTL_MQ_CONNECTING&&s->state!=MTTL_MQ_SYNCING&&s->state!=MTTL_MQ_ONLINE))return;
    if(s->failures<6)++s->failures;s->delay=1000u<<(s->failures-1);stop(s,now);
}
int mttl_mq_can_command(const mttl_mqtt_session *s,uint32_t generation,int retained){return s->state==MTTL_MQ_ONLINE&&s->desired&&s->generation==generation&&!retained;}
