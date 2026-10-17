/* Real SDK Wi-Fi adapter; mock APIs are selected only by offline tests. */
#include "sdk_wifi_adapter.h"
#ifdef MTTL_SDK_WIFI_TEST
#include "sdk_wifi_mock.h"
#else
#include "wifi_conf.h"
#include "lwip/netif.h"
#include "dhcp/dhcps.h"
#include "task.h"
#include "lwip_netconf.h"
#endif
extern struct netif xnetif[];
static mttl_scan scan_cache;
static uint32_t scan_generation;
static int32_t scan_diagnostics[4];
static uint8_t network_initialized;
void mttl_sdk_network_init(void) {
    /* Single network worker only; before any radio/netif/DHCP operation. */
    if (!network_initialized) { LwIP_Init(); network_initialized=1; }
}

static rtw_result_t scan_result(rtw_scan_handler_result_t *result) {
    if (!result || result->user_data != &scan_generation) return RTW_SUCCESS;
    taskENTER_CRITICAL();
    if (result->scan_complete) {
        mttl_scan_finish(&scan_cache, scan_generation);
    } else {
        unsigned length=result->ap_details.SSID.len;
        ++scan_diagnostics[0];
        /* Some driver reports populate the string without its length byte.
         * Infer only a missing length, never read beyond the SSID limit. */
        if(!length){while(length<32&&result->ap_details.SSID.val[length])++length;if(length)++scan_diagnostics[2];}
        if(!mttl_scan_add(&scan_cache, scan_generation, result->ap_details.SSID.val,
                      length, result->ap_details.signal_strength,
                      (uint32_t)result->ap_details.security))++scan_diagnostics[1];
    }
    taskEXIT_CRITICAL();
    return RTW_SUCCESS;
}
int mttl_sdk_scan_pending(void) {
    int pending;
    taskENTER_CRITICAL(); pending = scan_cache.pending; taskEXIT_CRITICAL();
    return pending;
}
int mttl_sdk_scan_start(uint32_t generation) {
    int result;
    if (!wifi_is_up(RTW_STA_INTERFACE) || mttl_sdk_scan_pending()) return 0;
    taskENTER_CRITICAL();
    scan_diagnostics[0]=scan_diagnostics[1]=scan_diagnostics[2]=scan_diagnostics[3]=0;
    scan_generation = generation; mttl_scan_begin(&scan_cache, generation);
    taskEXIT_CRITICAL();
    result=wifi_scan_networks(scan_result, &scan_generation);
    taskENTER_CRITICAL();scan_diagnostics[3]=result;taskEXIT_CRITICAL();
    if (result != RTW_SUCCESS) {
        taskENTER_CRITICAL(); mttl_scan_finish(&scan_cache, generation); taskEXIT_CRITICAL();
        return 0;
    }
    return 1;
}
void mttl_sdk_scan_diagnostics(int32_t out[4]){unsigned i;taskENTER_CRITICAL();for(i=0;i<4;i++)out[i]=scan_diagnostics[i];taskEXIT_CRITICAL();}
int mttl_sdk_scan_snapshot(mttl_scan *destination) {
    unsigned i;
    const unsigned char *source = (const unsigned char *)&scan_cache;
    unsigned char *target = (unsigned char *)destination;
    if (!destination) return 0;
    /* Bounded ~720-byte copy. No blocking APIs, logging or allocations here.
     * Target interrupt latency must be measured before production use. */
    taskENTER_CRITICAL();
    for (i=0; i<sizeof(scan_cache); ++i) target[i]=source[i];
    taskEXIT_CRITICAL(); return 1;
}

int mttl_sdk_stop_radio(void) {
    if (mttl_sdk_scan_pending()) return 0;
    /* HTTP/MQTT ownership is outside this adapter; disconnect != task join. */
    /* Worker must complete TCP/IP DHCP/AP cleanup before calling this. */
    if (wifi_off() < 0) return 0;
    return wifi_is_up(RTW_STA_INTERFACE) == 0;
}
int mttl_sdk_associated(void) { return wifi_is_connected_to_ap()==RTW_SUCCESS; }
int mttl_sdk_join(const char *ssid,const char *password) {
    unsigned length=0,key_length=0,i;
    if(!ssid||!password||mttl_sdk_scan_pending())return 0;
    while(length<=32&&ssid[length])++length;
    while(key_length<=64&&password[key_length])++key_length;
    if(!length||length>32||key_length>64||(key_length&&key_length<8))return 0;
    if(key_length==64)for(i=0;i<64;++i){char c=password[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return 0;}
    if(wifi_is_up(RTW_STA_INTERFACE))return 0;
    if(wifi_on(RTW_MODE_STA)<0)return 0;
    /* Completed worker stop clears the old address on the TCP/IP thread. */
    if(wifi_connect((char *)ssid,key_length?RTW_SECURITY_WPA2_AES_PSK:RTW_SECURITY_OPEN,
                    key_length?(char *)password:0,length,key_length,-1,0)<0 || !mttl_sdk_associated()){
        wifi_off();return 0;
    }
    return 1;
}
int mttl_sdk_start_ap(const char *ssid) {
    unsigned length = 0;
    if (!ssid) return 0;
    while (length <= 32 && ssid[length]) ++length;
    if (!length || length > 32) return 0;
    /* Require completed stop; don't silently shut down another worker here. */
    if (wifi_is_up(RTW_STA_INTERFACE)) return 0;
    if (wifi_on(RTW_MODE_AP) < 0) return 0;
    if (wifi_start_ap((char *)ssid, RTW_SECURITY_OPEN, 0, length, 0, 1) < 0) {
        wifi_off(); return 0;
    }
    /* Address/pool/server setup is queued separately by the worker. */
    /* This means the SDK accepted startup, not station association confirmed. */
    return 1;
}
