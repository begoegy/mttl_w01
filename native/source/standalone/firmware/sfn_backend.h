#ifndef SFN_BACKEND_H
#define SFN_BACKEND_H
#include <stdint.h>
#include "settings.h"
/* Zero IPv4 means not provisioned yet. Non-zero endpoints must be safe unicast IPv4. */
int sfn_backend_valid(uint32_t ipv4,uint16_t port);
/* Pin the build-owned endpoint into a settings candidate. Returns 1 changed, 0 invalid/no change. */
int sfn_backend_pin(mttl_settings *settings,uint32_t ipv4,uint16_t port);
#endif
