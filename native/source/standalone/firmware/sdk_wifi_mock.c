#include "sdk_wifi_mock.h"
#include "sdk_wifi_adapter.h"
#include "sdk_dhcp.h"
struct netif xnetif[1];
static int up, scan_fail, critical, unbalanced, api_in_critical, dhcp, off_calls;
static int join_fail;
static void api(void);
static unsigned network_init_calls;
void LwIP_Init(void){api();++network_init_calls;}
static void (*tcp_callback)(void *);
static void *tcp_context;
static int tcp_fail;
static int tcp_active,wrong_tcp_thread;
static struct dhcp client;
int tcpip_callback(void (*fn)(void *),void *user){api();if(tcp_fail)return -1;tcp_callback=fn;tcp_context=user;return 0;}
int dhcp_start(struct netif *n){api();n->dhcp=&client;client.state=0;return 0;}
void dhcp_stop(struct netif *n){api();n->dhcp=0;}
static void dispatch(void){void (*fn)(void *)=tcp_callback;tcp_callback=0;if(fn){tcp_active=1;fn(tcp_context);tcp_active=0;}}
static uint32_t pool_first, pool_last;
static rtw_result_t (*handler)(rtw_scan_handler_result_t *);
static void *context;
void mock_critical(int change){critical+=change;if(critical<0)unbalanced=1;}
static void api(void){if(critical)api_in_critical=1;}
int wifi_is_up(int interface){(void)interface;api();return up;}
int wifi_on(int mode){(void)mode;api();up=1;return 0;}
int wifi_off(void){api();up=0;++off_calls;return 0;}
int wifi_is_connected_to_ap(void){api();return join_fail?-1:0;}
int wifi_connect(char *ssid,int security,char *password,int length,int key_length,int key_id,void *semaphore){
    api();(void)ssid;(void)security;(void)password;(void)length;(void)key_length;(void)key_id;(void)semaphore;return join_fail?-1:0;
}
int wifi_start_ap(char *ssid,int security,char *password,int length,int password_length,int channel){
    api();return (!ssid||security||password||length<1||password_length||channel!=1)?-1:0;
}
int wifi_scan_networks(rtw_result_t (*callback)(rtw_scan_handler_result_t *),void *user){api();handler=callback;context=user;return scan_fail==2?2:scan_fail?-1:0;}
void netif_set_addr(struct netif *n,const struct ip_addr *ip,const struct ip_addr *mask,const struct ip_addr *gw){api();if(!tcp_active)wrong_tcp_thread=1;n->ip_addr=*ip;(void)mask;(void)gw;}
void dhcps_init(struct netif *n){api();if(!tcp_active)wrong_tcp_thread=1;(void)n;dhcp=1;}
void dhcps_deinit(void){api();if(!tcp_active)wrong_tcp_thread=1;dhcp=0;}
void dhcps_set_addr_pool(int enabled,struct ip_addr *first,struct ip_addr *last){api();if(!tcp_active)wrong_tcp_thread=1;if(enabled){pool_first=first->addr;pool_last=last->addr;}}
unsigned mttl_sdk_mock_tests(void){
    unsigned failed=0, i; mttl_scan snapshot; rtw_scan_handler_result_t result;
    mttl_sdk_network_init();mttl_sdk_network_init();
    if(network_init_calls!=1)++failed;
    for(i=0;i<sizeof(result);++i)((unsigned char *)&result)[i]=0;
    if(mttl_sdk_scan_start(1))++failed; // radio down
    if(!mttl_sdk_start_ap("MTTL-TEST")||dhcp)++failed;
    if(!mttl_sdk_ap_address()||dhcp)++failed;
    dispatch();
    if(!dhcp||xnetif[0].ip_addr.addr!=0xc0a80401u||pool_first!=0xc0a80402u||pool_last!=0xc0a80414u)++failed;
    if(mttl_sdk_start_ap("duplicate"))++failed;
    if(!mttl_sdk_scan_start(7)||mttl_sdk_scan_start(8))++failed;
    if(mttl_sdk_stop_radio()||off_calls)++failed;
    result.user_data=context;result.ap_details.SSID.val[0]='A';result.ap_details.SSID.len=1;result.ap_details.signal_strength=-43;
    handler(&result);
    if(!mttl_sdk_scan_snapshot(&snapshot)||snapshot.count!=1||snapshot.entries[0].rssi!=-43||!snapshot.pending)++failed;
    result.ap_details.SSID.len=0;result.ap_details.SSID.val[0]='B';result.ap_details.SSID.val[1]=0;handler(&result);
    if(!mttl_sdk_scan_snapshot(&snapshot)||snapshot.count!=2||snapshot.entries[1].ssid[0]!='B')++failed;
    {int32_t diagnostics[4];mttl_sdk_scan_diagnostics(diagnostics);if(diagnostics[0]!=2||diagnostics[1]||diagnostics[2]!=1||diagnostics[3])++failed;}
    for(i=0;i<32;i++)result.ap_details.SSID.val[i]='C';result.ap_details.SSID.len=0;handler(&result);
    if(!mttl_sdk_scan_snapshot(&snapshot)||snapshot.count!=3||snapshot.entries[2].ssid[31]!='C'||snapshot.entries[2].ssid[32])++failed;
    result.ap_details.SSID.len=33;handler(&result); /* Never trust invalid explicit lengths. */
    result.ap_details.SSID.len=0;result.ap_details.SSID.val[0]=0;handler(&result); /* Hidden SSID. */
    {int32_t diagnostics[4];mttl_sdk_scan_diagnostics(diagnostics);if(diagnostics[0]!=5||diagnostics[1]!=2||diagnostics[2]!=2)++failed;}
    result.scan_complete=1;handler(&result);
    if(mttl_sdk_scan_pending())++failed;
    if(!mttl_sdk_dhcp_stop())++failed;dispatch();
    if(!mttl_sdk_stop_radio()||up||dhcp)++failed;
    if(!mttl_sdk_start_ap("MTTL-TEST"))++failed;
    scan_fail=1;
    if(mttl_sdk_scan_start(9)||mttl_sdk_scan_pending())++failed;
    scan_fail=2; /* SDK timeout is positive, not a successful start. */
    if(mttl_sdk_scan_start(10)||mttl_sdk_scan_pending())++failed;
    if(critical||unbalanced||api_in_critical||mttl_sdk_scan_snapshot(0))++failed;
    scan_fail=0;mttl_sdk_stop_radio();
    if(mttl_sdk_join("S","short"))++failed;
    if(!mttl_sdk_join("S","password")||xnetif[0].ip_addr.addr)++failed;
    mttl_sdk_stop_radio();join_fail=1;
    if(mttl_sdk_join("S","password")||up)++failed;
    return failed;
}
unsigned mttl_dhcp_mock_tests(void){
    unsigned failed=0;
    xnetif[0].ip_addr.addr=0xc0a80401u;
    if(!mttl_sdk_dhcp_start()||!mttl_sdk_dhcp_busy()||mttl_sdk_dhcp_start())++failed;
    if(mttl_sdk_dhcp_poll())++failed;
    dispatch();
    if(mttl_sdk_dhcp_busy()||xnetif[0].ip_addr.addr||!xnetif[0].dhcp)++failed;
    mttl_sdk_dhcp_poll();dispatch();
    client.state=DHCP_BOUND;xnetif[0].ip_addr.addr=0xc000020au;
    mttl_sdk_dhcp_poll();dispatch();
    if(mttl_sdk_dhcp_poll()!=0xc000020au)++failed;
    dispatch();
    if(!mttl_sdk_dhcp_stop()||mttl_sdk_dhcp_poll())++failed;
    dispatch();
    if(xnetif[0].dhcp||xnetif[0].ip_addr.addr||mttl_sdk_dhcp_busy())++failed;
    tcp_fail=1;if(mttl_sdk_dhcp_start()||mttl_sdk_dhcp_busy())++failed;tcp_fail=0;
    if(critical||api_in_critical||wrong_tcp_thread)++failed;
    return failed;
}
