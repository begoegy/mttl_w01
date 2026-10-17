#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "../standalone/firmware/sfn_up_client.h"
#include "../standalone/firmware/sfn_voltage_policy.h"
#include "../standalone/firmware/sfn_schedule_sync.h"

typedef struct { int fd; int connected; } host_socket;
typedef struct { uint8_t on_mask; uint8_t child_lock; int voltage_mv; int rssi; uint32_t started_ms; sfn_voltage_guard_config vguard; uint32_t vguard_revision; sfn_schedule_sync schedule_sync; mttl_schedule_config schedule; sfn_schedule_meta schedule_meta; uint8_t clock_valid; } strip_state;
static volatile sig_atomic_t keep_running=1;
static void stop_signal(int sig){(void)sig;keep_running=0;}
static uint32_t now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint32_t)((uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u);}
static int sock_close(void *p){host_socket *s=p;if(s->fd>=0)close(s->fd);s->fd=-1;s->connected=0;return 1;}
static int writable(int fd){fd_set w;struct timeval tv={0,0};FD_ZERO(&w);FD_SET(fd,&w);return select(fd+1,0,&w,0,&tv);}
static int sock_open(void *p,const char *host,uint16_t port){host_socket *s=p;struct sockaddr_in a;int e,flags;
 if(s->connected)return 1;
 if(s->fd<0){s->fd=socket(AF_INET,SOCK_STREAM,0);if(s->fd<0)return -1;flags=fcntl(s->fd,F_GETFL,0);if(flags<0||fcntl(s->fd,F_SETFL,flags|O_NONBLOCK)<0){sock_close(s);return -1;}
  memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(port);if(inet_pton(AF_INET,host,&a.sin_addr)!=1){sock_close(s);return -1;}
  if(connect(s->fd,(struct sockaddr *)&a,sizeof(a))==0){s->connected=1;return 1;}if(errno!=EINPROGRESS&&errno!=EWOULDBLOCK){sock_close(s);return -1;}return 0;}
 e=writable(s->fd);if(e<0){sock_close(s);return -1;}if(!e)return 0;{socklen_t n=sizeof(e);if(getsockopt(s->fd,SOL_SOCKET,SO_ERROR,&e,&n)<0||e){sock_close(s);return -1;}}s->connected=1;return 1;
}
static int sock_send(void *p,const uint8_t *b,size_t n){host_socket *s=p;ssize_t r;if(s->fd<0||!s->connected)return -1;r=send(s->fd,b,n,MSG_NOSIGNAL|MSG_DONTWAIT);if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return 0;return r<0?-1:(int)r;}
static int sock_receive(void *p,uint8_t *b,size_t n){host_socket *s=p;ssize_t r;if(s->fd<0||!s->connected)return -1;r=recv(s->fd,b,n,MSG_DONTWAIT);if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return 0;return r<=0?-1:(int)r;}
static size_t handle(void *p,const sfn_up_command *c,char *out,size_t cap){strip_state *s=p;unsigned i;
 switch(c->type){
  case SFN_UP_CMD_GETINFO_ALL:{sfn_up_outlet x[4];memset(x,0,sizeof(x));for(i=0;i<4;i++){x[i].channel=(uint8_t)(i+1);x[i].on=(s->on_mask&(1u<<i))!=0;x[i].state=x[i].on;x[i].runtime=(int32_t)((now_ms()-s->started_ms)/1000u);x[i].power_mw=x[i].on?(int32_t)((i+1)*1000):0;x[i].energy_wh=10+i;x[i].temperature_c=31+(int)i;x[i].overload="0";x[i].overheat="0";}return sfn_up_encode_getinfo(out,cap,x);}
  case SFN_UP_CMD_ONOFF:if(c->on)s->on_mask|=(uint8_t)(1u<<(c->channel-1));else s->on_mask&=(uint8_t)~(1u<<(c->channel-1));return sfn_up_encode_onoff_ack(out,cap,c->channel,c->on);
  case SFN_UP_CMD_VOLTAGE:return sfn_up_encode_voltage(out,cap,1,s->voltage_mv);
  case SFN_UP_CMD_RSSI:return sfn_up_encode_rssi(out,cap,s->rssi);
  case SFN_UP_CMD_CHILDLOCK_GET:return sfn_up_encode_childlock(out,cap,s->child_lock);
  case SFN_UP_CMD_CHILDLOCK_SET:s->child_lock=c->mask;return sfn_up_encode_childlock(out,cap,s->child_lock);
  case SFN_UP_CMD_VGUARD_SET:
   if(!sfn_voltage_policy_decode(c->flags,c->thresholds,c->timing,c->recovery_seconds,&s->vguard))return 0;
   s->vguard_revision=c->revision;return sfn_up_encode_vguard_ack(out,cap,c->revision);
  case SFN_UP_CMD_CLOCK_SYNC:
   if(!sfn_schedule_sync_clock(&s->schedule_sync,c->epoch,c->offset_minutes,c->transition1_epoch,c->transition1_offset_minutes,c->transition2_epoch,c->transition2_offset_minutes))return 0;
   s->clock_valid=1;
   return sfn_up_encode_clock_ack(out,cap,c->epoch);
  case SFN_UP_CMD_SCHEDULE_BEGIN:(void)sfn_schedule_sync_begin(&s->schedule_sync,c->generation,c->count);return 0;
  case SFN_UP_CMD_SCHEDULE_ITEM:(void)sfn_schedule_sync_item(&s->schedule_sync,c->slot,c->outlet,c->repeat,c->days,c->date_days,c->minute,c->on);return 0;
  case SFN_UP_CMD_SCHEDULE_COMMIT:
   if(!sfn_schedule_sync_commit(&s->schedule_sync,c->generation,&s->schedule,&s->schedule_meta))return 0;
   return sfn_up_encode_schedule_commit_ack(out,cap,s->schedule_meta.generation,s->schedule_meta.count);
  case SFN_UP_CMD_SCHEDULE_STATUS:return sfn_up_encode_schedule_status(out,cap,s->schedule_meta.generation,s->schedule_meta.count,s->clock_valid,s->schedule_meta.valid);
  default:return 0;
 }
}
int main(int argc,char **argv){host_socket sock={-1,0};sfn_up_io io;sfn_up_client client;strip_state state;uint16_t port;uint32_t until;
 if(argc!=3){fprintf(stderr,"usage: %s host port\n",argv[0]);return 2;}port=(uint16_t)strtoul(argv[2],0,10);if(!port)return 2;
 signal(SIGTERM,stop_signal);signal(SIGINT,stop_signal);memset(&state,0,sizeof(state));state.on_mask=2;state.voltage_mv=224500;state.rssi=-55;sfn_voltage_guard_default(&state.vguard);sfn_schedule_sync_init(&state.schedule_sync);mttl_schedule_default(&state.schedule);sfn_schedule_meta_default(&state.schedule_meta);state.started_ms=now_ms();until=state.started_ms+30000u;
 io.context=&sock;io.open=sock_open;io.close=sock_close;io.send=sock_send;io.receive=sock_receive;sfn_up_client_init(&client);
 while(keep_running&&(int32_t)(until-now_ms())>0){if(!sfn_up_client_step(&client,&io,1,argv[1],port,now_ms(),"lgutap","AABBCCDDE201","2.0.12",handle,&state))break;{struct timespec ts={0,10000000};nanosleep(&ts,0);}}
 printf("HOST_STRIP_DONE reconnects=%lu commands=%lu rejected=%lu io_failures=%lu child_lock=%u on_mask=%u vguard_revision=%lu vguard_mask=%u schedule_generation=%lu schedule_count=%u state=%d\n",(unsigned long)client.reconnects,(unsigned long)client.commands,(unsigned long)client.rejected,(unsigned long)client.io_failures,(unsigned)state.child_lock,(unsigned)state.on_mask,(unsigned long)state.vguard_revision,(unsigned)state.vguard.protected_mask,(unsigned long)state.schedule_meta.generation,(unsigned)state.schedule_meta.count,(int)client.state);
 sock_close(&sock);return 0;
}
