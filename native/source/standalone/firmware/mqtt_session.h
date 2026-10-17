#ifndef MTTL_MQTT_SESSION_H
#define MTTL_MQTT_SESSION_H
#include <stdint.h>
typedef enum { MTTL_MQ_OFF,MTTL_MQ_STOPPING,MTTL_MQ_BACKOFF,MTTL_MQ_CONNECTING,MTTL_MQ_SYNCING,MTTL_MQ_ONLINE,MTTL_MQ_FAULT } mttl_mq_state;
typedef enum { MTTL_MQ_NONE,MTTL_MQ_CLOSE,MTTL_MQ_CONNECT } mttl_mq_action;
typedef struct {
    mttl_mq_state state;uint32_t generation,revision,since,delay;
    uint8_t desired,failures;
} mttl_mqtt_session;
/* Single owner. CLOSE is a complete socket/callback ownership barrier, not
 * merely DISCONNECT sent. CONNECT must configure will before broker handshake.
 * SYNCING includes subscribe acknowledgement + discovery/state publication. */
void mttl_mq_init(mttl_mqtt_session *);
void mttl_mq_update(mttl_mqtt_session *,int network,int enabled,uint32_t revision,uint32_t now);
mttl_mq_action mttl_mq_step(mttl_mqtt_session *,uint32_t now);
int mttl_mq_stopped(mttl_mqtt_session *,uint32_t generation,uint32_t now);
int mttl_mq_connected(mttl_mqtt_session *,uint32_t generation,uint32_t now);
int mttl_mq_ready(mttl_mqtt_session *,uint32_t generation,uint32_t now);
void mttl_mq_failed(mttl_mqtt_session *,uint32_t generation,uint32_t now);
int mttl_mq_can_command(const mttl_mqtt_session *,uint32_t generation,int retained);
#endif
