#include "sdk_ntp.h"
#include "ntp_transport.h"
#include "lwip/sockets.h"
#include "osdep_service.h"
#include <string.h>
/* Fixed time server avoids blocking DNS in the network owner. Unauthenticated
 * SNTP is display-only; it must never authorize firmware or relay operations. */
static int descriptor=-1;
static struct sockaddr_in peer;
static mttl_ntp_transport transport;
static int initialized;
static int open_io(void *p,uint32_t ip,uint16_t port){unsigned long nb=1;(void)p;
 descriptor=socket(AF_INET,SOCK_DGRAM,0);if(descriptor<0)return 0;
 if(lwip_ioctl(descriptor,FIONBIO,&nb)!=0){close(descriptor);descriptor=-1;return 0;}
 memset(&peer,0,sizeof(peer));peer.sin_family=AF_INET;peer.sin_port=htons(port);peer.sin_addr.s_addr=htonl(ip);return 1;
}
static int close_io(void *p){(void)p;if(descriptor>=0&&close(descriptor)!=0)return 0;descriptor=-1;return 1;}
static int send_io(void *p,const uint8_t *data,size_t n){(void)p;return sendto(descriptor,data,n,0,(struct sockaddr *)&peer,sizeof(peer));}
static int recv_io(void *p,uint8_t *data,size_t n,uint32_t *ip,uint16_t *port){struct sockaddr_in from;socklen_t len=sizeof(from);fd_set set;struct timeval timeout={0,0};int rc;(void)p;
 FD_ZERO(&set);FD_SET(descriptor,&set);rc=select(descriptor+1,&set,0,0,&timeout);if(rc<=0)return rc;
 memset(&from,0,sizeof(from));rc=recvfrom(descriptor,data,n,0,(struct sockaddr *)&from,&len);
 if(rc>0){*ip=ntohl(from.sin_addr.s_addr);*port=ntohs(from.sin_port);}return rc;
}
/* SDK generator is tick-seeded, NOT cryptographic. Experimental display-only
 * correlation token; production adapter must replace it with verified entropy. */
static int entropy_io(void *p,uint8_t *data,size_t n){uint32_t aligned[2];(void)p;if(n!=8||rtw_get_random_bytes(aligned,sizeof(aligned))!=0)return 0;memcpy(data,aligned,8);return 1;}
void mttl_sdk_ntp_tick(mttl_ntp_client *client,int online,uint32_t now){
 static const mttl_ntp_io io={0,open_io,close_io,send_io,recv_io,entropy_io};
 if(!initialized){mttl_ntp_transport_init(&transport,0xa29fc801u);initialized=1;}
 mttl_ntp_transport_tick(&transport,client,&io,online,now);
}
