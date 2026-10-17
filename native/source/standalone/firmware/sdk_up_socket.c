#include "sdk_up_socket.h"
#include "lwip/sockets.h"
#include <errno.h>
#include <string.h>
static int socket_error(int fd){int error=0;socklen_t length=sizeof(error);return getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)==0?error:-1;}
/* A WAN outage can leave Wi-Fi associated while the old TCP path is black-holed.
 * Keepalive makes lwIP prove that an idle path is still alive; on failure recv/send
 * reports the dead socket and the existing client backoff opens a fresh connection. */
static int configure_keepalive(int fd){
 int enabled=1,idle_seconds=30,interval_seconds=10,probe_count=3;
 if(setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&enabled,sizeof(enabled))!=0)return 0;
 if(setsockopt(fd,IPPROTO_TCP,TCP_KEEPIDLE,&idle_seconds,sizeof(idle_seconds))!=0)return 0;
 if(setsockopt(fd,IPPROTO_TCP,TCP_KEEPINTVL,&interval_seconds,sizeof(interval_seconds))!=0)return 0;
 if(setsockopt(fd,IPPROTO_TCP,TCP_KEEPCNT,&probe_count,sizeof(probe_count))!=0)return 0;
 return 1;
}
static int ready(int fd,int writing){fd_set set;struct timeval timeout;FD_ZERO(&set);FD_SET(fd,&set);timeout.tv_sec=timeout.tv_usec=0;return select(fd+1,writing?0:&set,writing?&set:0,0,&timeout);}
static int address(const char *s,uint32_t *value){unsigned i,n,parts=0;uint32_t out=0;while(parts<4){n=0;i=0;while(*s>='0'&&*s<='9'){n=n*10+(unsigned)(*s++-'0');if(++i>3||n>255)return 0;}if(!i)return 0;out=(out<<8)|n;++parts;if(parts<4){if(*s++!='.')return 0;}else if(*s)return 0;}if((out>>24)==0||(out>>24)==127||(out>>24)>=224||out==0xffffffffu)return 0;*value=out;return 1;}
static int drop(void *context){sfn_sdk_up_socket *s=context;if(s->socket>=0&&close(s->socket)!=0)return -1;s->socket=-1;s->connected=0;return 1;}
static int open_socket(void *context,const char *host,uint16_t port){sfn_sdk_up_socket *s=context;uint32_t ip;struct sockaddr_in a;int error,rc;unsigned long nonblocking=1;
 if(s->connected)return 1;
 if(s->socket<0){if(!port||!address(host,&ip))return -1;s->socket=socket(AF_INET,SOCK_STREAM,0);if(s->socket<0)return -1;
  if(lwip_ioctl(s->socket,FIONBIO,&nonblocking)!=0||!configure_keepalive(s->socket)){drop(s);return -1;}
  memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(ip);
  rc=connect(s->socket,(struct sockaddr *)&a,sizeof(a));if(rc==0){s->connected=1;return 1;}
  error=socket_error(s->socket);if(error!=EINPROGRESS&&error!=EWOULDBLOCK&&error!=EALREADY){drop(s);return -1;}return 0;}
 rc=ready(s->socket,1);if(rc<0)return -1;if(!rc)return 0;if(socket_error(s->socket)!=0)return -1;s->connected=1;return 1;
}
static int transmit(void *context,const uint8_t *data,size_t size){sfn_sdk_up_socket *s=context;int rc;if(s->socket<0||!s->connected)return -1;rc=ready(s->socket,1);if(rc<=0)return rc;rc=send(s->socket,data,size,0);return rc<0&&socket_error(s->socket)==EWOULDBLOCK?0:rc;}
static int receive(void *context,uint8_t *data,size_t size){sfn_sdk_up_socket *s=context;int rc;if(s->socket<0||!s->connected)return -1;rc=ready(s->socket,0);if(rc<=0)return rc;rc=recv(s->socket,data,size,0);if(rc==0)return -1;return rc<0&&socket_error(s->socket)==EWOULDBLOCK?0:rc;}
void sfn_sdk_up_socket_init(sfn_sdk_up_socket *s){s->socket=-1;s->connected=0;}
void sfn_sdk_up_socket_io(sfn_sdk_up_socket *s,sfn_up_io *io){io->context=s;io->open=open_socket;io->close=drop;io->send=transmit;io->receive=receive;}
