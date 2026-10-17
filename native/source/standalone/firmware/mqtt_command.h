#ifndef MTTL_MQTT_COMMAND_H
#define MTTL_MQTT_COMMAND_H
#include "core.h"
#include "mqtt_session.h"
/* Prefix must include unique device identity, e.g. mttl/97c0123.
 * Output channel 0..3, 4=all; caller performs atomic queue/admission.
 * Validation only: no relay or queue mutation. Complete bounded QoS0 frame. */
int mttl_mq_command(const mttl_mqtt_session *,uint32_t generation,const char *prefix,const uint8_t *,size_t,mttl_command *);
#endif
