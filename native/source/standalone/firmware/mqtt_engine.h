#ifndef MTTL_MQTT_ENGINE_H
#define MTTL_MQTT_ENGINE_H
#include "mqtt_session.h"
#include "mqtt_wire.h"
#include "ha_discovery.h"
#include "core.h"
typedef struct {
    void *context;
    /* Bounded nonblocking: open 1 connected, 0 pending, -1 error;
     * close 1 ownership released, 0 pending, -1 error; send/recv byte counts.
     * No callback may retain supplied data. All calls by one owner. */
    int (*open)(void *,const char *,uint16_t);
    int (*close)(void *);
    int (*send)(void *,const uint8_t *,size_t);
    int (*receive)(void *,uint8_t *,size_t);
    int (*command)(void *,uint8_t,uint8_t);
    void *command_context;
} mttl_mqtt_io;
typedef struct {
    mttl_mqtt_session session;mttl_mq_rx rx;
    uint8_t tx[1024];size_t tx_size,tx_sent;uint32_t tx_since,last_tx,ping_at;
    unsigned phase,entity;uint8_t ping_wait,rediscover,cleanup_done;
} mttl_mqtt_engine;
void mttl_mqtt_engine_init(mttl_mqtt_engine *);
void mttl_mqtt_engine_step(mttl_mqtt_engine *,const mttl_settings *,const char *id,int network,uint32_t revision,uint32_t now,const mttl_mqtt_io *);
/* One bounded QoS0 publication; retain only durable logical state, not sensor samples. */
int mttl_mqtt_engine_publish(mttl_mqtt_engine *,const char *topic,const uint8_t *,size_t,int retain,uint32_t now);
#endif
