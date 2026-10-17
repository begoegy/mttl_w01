#ifndef MTTL_SDK_MQTT_SOCKET_H
#define MTTL_SDK_MQTT_SOCKET_H
#include "mqtt_engine.h"
/* Compile-only IPv4 plaintext adapter. No DNS/TLS; owns one nonblocking socket.
 * Default offline setup never instantiates/opens it. */
typedef struct { int socket,connected; } mttl_sdk_mqtt_socket;
void mttl_sdk_mqtt_socket_init(mttl_sdk_mqtt_socket *);
void mttl_sdk_mqtt_socket_io(mttl_sdk_mqtt_socket *,mttl_mqtt_io *);
#endif
