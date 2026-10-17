#ifndef MTTL_SDK_WIFI_ADAPTER_H
#define MTTL_SDK_WIFI_ADAPTER_H
#include "core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Offline integration draft, network-worker only. Caller must first stop and
 * join HTTP/MQTT tasks and complete any scan. No ISR/HTTP-handler calls.
 * lwIP thread ownership and DHCP pool still require target verification. */
int mttl_sdk_stop_radio(void);
void mttl_sdk_network_init(void);
int mttl_sdk_start_ap(const char *ssid);
/* Single network worker issues requests. Reader copies rather than borrowing
 * callback-owned state. Scan cannot be restarted until completion arrives. */
int mttl_sdk_scan_start(uint32_t generation);
int mttl_sdk_scan_snapshot(mttl_scan *destination);
int mttl_sdk_scan_pending(void);
/* received reports, rejected reports, inferred lengths, SDK start status */
void mttl_sdk_scan_diagnostics(int32_t out[4]);
/* Blocking SDK join (SDK timeout); dedicated worker only, never HTTP/RTW task.
 * Only open or WPA2 personal. Build must use no-fast-connect SDK source. */
int mttl_sdk_join(const char *ssid, const char *password);
int mttl_sdk_associated(void);
#ifdef __cplusplus
}
#endif
#endif
