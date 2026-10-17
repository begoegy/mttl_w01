/* Test-only SDK substitutes. Not target ABI definitions. */
#ifndef MTTL_SDK_WIFI_MOCK_H
#define MTTL_SDK_WIFI_MOCK_H
#include <stdint.h>
typedef int rtw_result_t;
#define RTW_SUCCESS 0
#define RTW_STA_INTERFACE 0
#define RTW_MODE_AP 1
#define RTW_MODE_STA 0
#define RTW_SECURITY_WPA2_AES_PSK 1
#define RTW_SECURITY_OPEN 0
typedef struct {
    struct { uint8_t val[33]; uint8_t len; } SSID;
    int16_t signal_strength; uint32_t security;
} mock_ap;
typedef struct { mock_ap ap_details; int scan_complete; void *user_data; } rtw_scan_handler_result_t;
struct ip_addr { uint32_t addr; };
struct dhcp { int state; };
struct netif { struct ip_addr ip_addr; struct dhcp *dhcp; };
#define DHCP_BOUND 10
#define ERR_OK 0
#define ip4_addr1(p) (((p)->addr>>24)&255)
#define ip4_addr2(p) (((p)->addr>>16)&255)
#define ip4_addr3(p) (((p)->addr>>8)&255)
#define ip4_addr4(p) ((p)->addr&255)
int dhcp_start(struct netif *);
void dhcp_stop(struct netif *);
int tcpip_callback(void (*fn)(void *),void *context);
unsigned mttl_dhcp_mock_tests(void);
#define IP4_ADDR(p,a,b,c,d) ((p)->addr=((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(d))
#define taskENTER_CRITICAL() mock_critical(1)
#define taskEXIT_CRITICAL() mock_critical(-1)
void mock_critical(int change);
void LwIP_Init(void);
int wifi_is_up(int interface);
int wifi_on(int mode);
int wifi_off(void);
int wifi_is_connected_to_ap(void);
int wifi_connect(char *,int,char *,int,int,int,void *);
int wifi_start_ap(char *ssid, int security, char *password, int length, int password_length, int channel);
int wifi_scan_networks(rtw_result_t (*callback)(rtw_scan_handler_result_t *), void *context);
void netif_set_addr(struct netif *, const struct ip_addr *, const struct ip_addr *, const struct ip_addr *);
void dhcps_init(struct netif *);
void dhcps_deinit(void);
void dhcps_set_addr_pool(int, struct ip_addr *, struct ip_addr *);
unsigned mttl_sdk_mock_tests(void);
#endif
