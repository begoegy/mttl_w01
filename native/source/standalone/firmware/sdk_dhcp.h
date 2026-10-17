#ifndef MTTL_SDK_DHCP_H
#define MTTL_SDK_DHCP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Worker requests; callbacks run on lwIP thread. Static mailbox must not be
 * reused until callback completes. 1=queued, 0=busy/queue failure. */
int mttl_sdk_dhcp_start(void);
int mttl_sdk_dhcp_stop(void);
int mttl_sdk_ap_address(void);
uint32_t mttl_sdk_dhcp_poll(void);
int mttl_sdk_dhcp_busy(void);
#ifdef __cplusplus
}
#endif
#endif
