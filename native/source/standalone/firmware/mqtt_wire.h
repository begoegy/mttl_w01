#ifndef MTTL_MQTT_WIRE_H
#define MTTL_MQTT_WIRE_H
#include <stddef.h>
#include <stdint.h>
#define MTTL_MQ_WIRE_CAP 512
size_t mttl_mq_connect_packet(uint8_t *,size_t,const char *client,const char *will_topic,const char *user,const char *password,uint16_t keepalive);
/* Outgoing PUBLISH up to 1024B; inputs must not alias output. Incoming limit
 * remains 512B, sufficient for bounded commands and broker control frames. */
size_t mttl_mq_publish_packet(uint8_t *,size_t,const char *topic,const uint8_t *payload,size_t size,int retained);
size_t mttl_mq_subscribe_packet(uint8_t *,size_t,const char *filter,uint16_t packet_id);
int mttl_mq_connack(const uint8_t *,size_t,int *accepted);
int mttl_mq_suback(const uint8_t *,size_t,uint16_t packet_id,int *accepted);
typedef struct { uint8_t bytes[MTTL_MQ_WIRE_CAP];uint16_t used,total;uint32_t since;uint8_t failed; } mttl_mq_rx;
void mttl_mq_rx_reset(mttl_mq_rx *);
/* 1 complete, 0 pending, -1 fault. Complete buffer remains until reset.
 * consumed may be less than input size: caller retains following frames.
 * First byte starts 2-second frame budget. Call with zero bytes to poll. */
int mttl_mq_rx_feed(mttl_mq_rx *,const uint8_t *,size_t,uint32_t now,size_t *consumed);
#endif
