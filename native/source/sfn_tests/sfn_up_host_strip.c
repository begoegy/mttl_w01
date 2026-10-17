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
#include "../standalone/firmware/sfn_final_state.h"

typedef struct { int fd; int connected; } host_socket;
typedef struct { uint8_t channel,on; uint32_t epoch; } relay_job;
typedef struct {
 uint8_t on_mask; uint8_t child_lock; int voltage_mv; int rssi; uint32_t started_ms;
 sfn_voltage_guard_config vguard; uint32_t vguard_revision; sfn_schedule_sync schedule_sync; mttl_schedule_config schedule; sfn_schedule_meta schedule_meta; uint8_t clock_valid;
 relay_job relay_queue[4]; uint8_t relay_head,relay_count,relay_active; relay_job active_job; uint32_t relay_due_ms; unsigned relay_ms,relay_completed,fault_after; uint8_t guard_fault_mask;
 sfn_final_state final_state; uint32_t link_epoch,getinfo_epoch; uint8_t getinfo_pending;
} strip_state;
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
static int relay_busy(const strip_state *s){return s->relay_active||s->relay_count;}
static size_t encode_report(strip_state *s,char *out,size_t cap){sfn_up_outlet x[4];unsigned i;memset(x,0,sizeof(x));for(i=0;i<4;i++){x[i].channel=(uint8_t)(i+1);x[i].on=(s->on_mask&(1u<<i))!=0;x[i].state=x[i].on;x[i].runtime=(int32_t)((now_ms()-s->started_ms)/1000u);x[i].power_mw=x[i].on?(int32_t)((i+1)*1000):0;x[i].energy_wh=10+i;x[i].temperature_c=31+(int)i;x[i].overload="0";x[i].overheat="0";}return sfn_up_encode_getinfo(out,cap,x);}
static int relay_enqueue(strip_state *s,unsigned channel,int on){unsigned tail;if(channel<1||channel>4||s->relay_count>=4||!sfn_final_state_reserve_remote(&s->final_state))return 0;if(on&&(s->guard_fault_mask&(1u<<(channel-1)))){sfn_final_state_cancel_remote(&s->final_state);return 0;}tail=(s->relay_head+s->relay_count)&3u;s->relay_queue[tail].channel=(uint8_t)channel;s->relay_queue[tail].on=(uint8_t)on;s->relay_queue[tail].epoch=s->link_epoch;++s->relay_count;return 1;}
static void relay_cancel_waiting(strip_state *s){while(s->relay_count){sfn_final_state_cancel_remote(&s->final_state);s->relay_head=(uint8_t)((s->relay_head+1u)&3u);--s->relay_count;}}
static void relay_step(strip_state *s,sfn_up_client *client){uint32_t now=now_ms();sfn_final_state_ack ack;char frame[SFN_UP_TX_CAP];size_t n;if(!s->relay_active&&s->relay_count){s->active_job=s->relay_queue[s->relay_head];s->relay_head=(uint8_t)((s->relay_head+1u)&3u);--s->relay_count;s->relay_active=1;s->relay_due_ms=now+s->relay_ms;}if(s->relay_active&&(int32_t)(now-s->relay_due_ms)>=0){relay_job j=s->active_job;s->relay_active=0;++s->relay_completed;if(s->fault_after&&s->relay_completed==s->fault_after){(void)sfn_final_state_complete_remote(&s->final_state,j.epoch,j.channel,j.on,0);relay_cancel_waiting(s);}else{if(j.on)s->on_mask|=(uint8_t)(1u<<(j.channel-1));else s->on_mask&=(uint8_t)~(1u<<(j.channel-1));(void)sfn_final_state_complete_remote(&s->final_state,j.epoch,j.channel,j.on,1);}if(!s->relay_active&&s->relay_count){s->active_job=s->relay_queue[s->relay_head];s->relay_head=(uint8_t)((s->relay_head+1u)&3u);--s->relay_count;s->relay_active=1;s->relay_due_ms=now+s->relay_ms;}}if(s->getinfo_pending&&s->getinfo_epoch==s->link_epoch&&!relay_busy(s)&&sfn_up_client_ready(client)){n=encode_report(s,frame,sizeof(frame));if(n&&sfn_up_client_publish(client,frame,n))s->getinfo_pending=0;}if(!s->getinfo_pending&&sfn_up_client_ready(client)&&sfn_final_state_peek_current(&s->final_state,s->link_epoch,&ack)){n=sfn_up_encode_onoff_ack(frame,sizeof(frame),ack.channel,ack.on);if(n&&sfn_up_client_publish(client,frame,n))sfn_final_state_consume(&s->final_state);}}
static size_t handle(void *p,const sfn_up_command *c,char *out,size_t cap){strip_state *s=p;
 switch(c->type){
  case SFN_UP_CMD_GETINFO_ALL:if(relay_busy(s)){s->getinfo_pending=1;s->getinfo_epoch=s->link_epoch;return 0;}return encode_report(s,out,cap);
  case SFN_UP_CMD_ONOFF:(void)relay_enqueue(s,c->channel,c->on);return 0;
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
  case SFN_UP_CMD_OTA:return sfn_up_encode_ota_ack(out,cap);
  default:return 0;
 }
}
int main(int argc,char **argv){host_socket sock={-1,0};sfn_up_io io;sfn_up_client client;strip_state state;uint16_t port;uint32_t until;sfn_up_link_state before,after;int i;
 if(argc<3){fprintf(stderr,"usage: %s host port [--relay-ms N] [--fault-after N] [--guard-fault-mask N]\n",argv[0]);return 2;}port=(uint16_t)strtoul(argv[2],0,10);if(!port)return 2;
 signal(SIGTERM,stop_signal);signal(SIGINT,stop_signal);memset(&state,0,sizeof(state));state.on_mask=2;state.voltage_mv=224500;state.rssi=-55;state.relay_ms=100;state.link_epoch=1;sfn_final_state_init(&state.final_state);sfn_voltage_guard_default(&state.vguard);sfn_schedule_sync_init(&state.schedule_sync);mttl_schedule_default(&state.schedule);sfn_schedule_meta_default(&state.schedule_meta);
 for(i=3;i+1<argc;i+=2){if(!strcmp(argv[i],"--relay-ms"))state.relay_ms=(unsigned)strtoul(argv[i+1],0,10);else if(!strcmp(argv[i],"--fault-after"))state.fault_after=(unsigned)strtoul(argv[i+1],0,10);else if(!strcmp(argv[i],"--guard-fault-mask"))state.guard_fault_mask=(uint8_t)strtoul(argv[i+1],0,0);else return 2;}if(!state.relay_ms||state.relay_ms>5000)return 2;
 state.started_ms=now_ms();until=state.started_ms+45000u;io.context=&sock;io.open=sock_open;io.close=sock_close;io.send=sock_send;io.receive=sock_receive;sfn_up_client_init(&client);
 while(keep_running&&(int32_t)(until-now_ms())>0){relay_step(&state,&client);before=client.state;if(!sfn_up_client_step(&client,&io,1,argv[1],port,now_ms(),"lgutap","AABBCCDDE201","2.0.15",handle,&state))break;after=client.state;if((before==SFN_UP_LINK_ONLINE)!=(after==SFN_UP_LINK_ONLINE)){if(++state.link_epoch==0)++state.link_epoch;state.getinfo_pending=0;state.getinfo_epoch=0;}relay_step(&state,&client);{struct timespec ts={0,5000000};nanosleep(&ts,0);}}
 printf("HOST_STRIP_DONE reconnects=%lu commands=%lu rejected=%lu io_failures=%lu child_lock=%u on_mask=%u vguard_revision=%lu vguard_mask=%u schedule_generation=%lu schedule_count=%u relay_completed=%u outstanding=%u pending_ack=%u state=%d\n",(unsigned long)client.reconnects,(unsigned long)client.commands,(unsigned long)client.rejected,(unsigned long)client.io_failures,(unsigned)state.child_lock,(unsigned)state.on_mask,(unsigned long)state.vguard_revision,(unsigned)state.vguard.protected_mask,(unsigned long)state.schedule_meta.generation,(unsigned)state.schedule_meta.count,state.relay_completed,sfn_final_state_outstanding(&state.final_state),sfn_final_state_pending(&state.final_state),(int)client.state);
 sock_close(&sock);return 0;
}
