/* Compile the local SDK Wi-Fi configuration implementation with fast-connect
 * persistence disabled. No edits to the reference SDK or archive are made.
 * Full compatibility with the vendor archive remains to be validated. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lwip_netconf.h"
#include "main.h"
#include "platform/platform_stdlib.h"
#include "wifi/wifi_conf.h"
#include "wifi/wifi_util.h"
#include "wifi/wifi_ind.h"
#include "tcpip.h"
#include "osdep_service.h"
#undef CONFIG_EXAMPLE_WLAN_FAST_CONNECT
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT 0
/* Keep stock event sorting/freeing, but bound its temporary result cache to
 * the same 16 networks exposed by our API instead of allocating 64 entries. */
#define wifi_scan_networks mttl_unused_vendor_scan_networks
#include "wifi_conf.c"
#undef wifi_scan_networks
int wifi_scan_networks(rtw_scan_result_handler_t handler,void *context){
    const unsigned count=16;
    if(scan_result_handler_ptr.scan_running)return RTW_TIMEOUT;
    scan_result_handler_ptr.scan_start_time=rtw_get_current_time();
    scan_result_handler_ptr.scan_running=1;
    scan_result_handler_ptr.gscan_result_handler=handler;
    scan_result_handler_ptr.max_ap_size=count;
    scan_result_handler_ptr.scan_cnt=0;
    scan_result_handler_ptr.scan_complete=RTW_FALSE;
    scan_result_handler_ptr.user_data=context;
    scan_result_handler_ptr.ap_details=rtw_zmalloc(count*sizeof(rtw_scan_result_t));
    if(!scan_result_handler_ptr.ap_details)goto failed;
    scan_result_handler_ptr.pap_details=rtw_zmalloc(count*sizeof(rtw_scan_result_t*));
    if(!scan_result_handler_ptr.pap_details)goto failed;
    if(wifi_scan(RTW_SCAN_COMMAMD<<4|RTW_SCAN_TYPE_ACTIVE,RTW_BSS_TYPE_ANY,NULL)==RTW_SUCCESS)return RTW_SUCCESS;
failed:
    if(scan_result_handler_ptr.pap_details)rtw_free(scan_result_handler_ptr.pap_details);
    if(scan_result_handler_ptr.ap_details)rtw_free(scan_result_handler_ptr.ap_details);
    rtw_memset(&scan_result_handler_ptr,0,sizeof(scan_result_handler_ptr));
    return RTW_ERROR;
}
