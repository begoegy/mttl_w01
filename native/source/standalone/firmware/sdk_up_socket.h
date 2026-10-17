#ifndef SFN_SDK_UP_SOCKET_H
#define SFN_SDK_UP_SOCKET_H
#include "sfn_up_client.h"
typedef struct { int socket,connected; } sfn_sdk_up_socket;
void sfn_sdk_up_socket_init(sfn_sdk_up_socket *);
void sfn_sdk_up_socket_io(sfn_sdk_up_socket *,sfn_up_io *);
#endif
