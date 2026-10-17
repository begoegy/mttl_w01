#include "sdk_dhcp.h"
#ifdef MTTL_SDK_WIFI_TEST
#include "sdk_wifi_mock.h"
#else
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/tcpip.h"
#include "task.h"
/* Vendor server header redeclares the client's dhcp_msg tag and port macros.
 * Server message type is private; keep both declarations distinct. */
#undef DHCP_SERVER_PORT
#undef DHCP_CLIENT_PORT
#define dhcp_msg mttl_dhcps_msg
#include "dhcp/dhcps.h"
#undef dhcp_msg
#endif
extern struct netif xnetif[];
static uint8_t pending,operation;
static uint32_t address;
static void execute(void *context){
    struct netif *n=&xnetif[0];uint32_t ip=0;
    struct ip_addr zero;
    (void)context;
    if(operation==4){
        struct ip_addr mask,gateway,first,last;
        dhcp_stop(n);dhcps_deinit();
        IP4_ADDR(&zero,192,168,4,1);IP4_ADDR(&gateway,192,168,4,1);IP4_ADDR(&mask,255,255,255,0);
        netif_set_addr(n,&zero,&mask,&gateway);
        IP4_ADDR(&first,192,168,4,2);IP4_ADDR(&last,192,168,4,20);
        dhcps_set_addr_pool(1,&first,&last);dhcps_init(n);
    }else if(operation==1||operation==2){
        dhcps_deinit();
        dhcp_stop(n);IP4_ADDR(&zero,0,0,0,0);netif_set_addr(n,&zero,&zero,&zero);
        if(operation==1)dhcp_start(n);
    }else if(n->dhcp && n->dhcp->state==DHCP_BOUND){
        ip=((uint32_t)ip4_addr1(&n->ip_addr)<<24)|((uint32_t)ip4_addr2(&n->ip_addr)<<16)|
            ((uint32_t)ip4_addr3(&n->ip_addr)<<8)|ip4_addr4(&n->ip_addr);
    }
    taskENTER_CRITICAL();address=ip;pending=0;taskEXIT_CRITICAL();
}
static int queue(uint8_t op){
    taskENTER_CRITICAL();
    if(pending){taskEXIT_CRITICAL();return 0;}
    pending=1;operation=op;if(op!=3)address=0;
    taskEXIT_CRITICAL();
    if(tcpip_callback(execute,0)!=ERR_OK){taskENTER_CRITICAL();pending=0;taskEXIT_CRITICAL();return 0;}
    return 1;
}
int mttl_sdk_dhcp_start(void){return queue(1);}
int mttl_sdk_dhcp_stop(void){return queue(2);}
int mttl_sdk_ap_address(void){return queue(4);}
int mttl_sdk_dhcp_busy(void){int busy;taskENTER_CRITICAL();busy=pending;taskEXIT_CRITICAL();return busy;}
uint32_t mttl_sdk_dhcp_poll(void){
    uint32_t ip;
    queue(3);
    taskENTER_CRITICAL();ip=address;taskEXIT_CRITICAL();
    return ip;
}
