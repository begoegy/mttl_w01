/* SINGLE HTTP owner. No extra task, blocking send, GPIO or credentials. */
#include "sdk_sse.h"
#include "sdk_worker.h"
#include "httpd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/sockets.h"
#include <string.h>
#include <stdio.h>
static int socket_fd=-1;
static char *frame;
static size_t size,offset;
static uint32_t sent_hash,pending_hash,queued_at,sent_at;
static uint32_t sent_relay,pending_relay;
static uint8_t relay_frame;
static uint32_t now(void){return (uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;}
int mttl_sdk_sse_active(void){return socket_fd>=0;}
void mttl_sdk_sse_close(void){if(socket_fd>=0)close(socket_fd);socket_fd=-1;if(frame)vPortFree(frame);frame=0;size=offset=0;}
void mttl_sdk_sse_open(struct httpd_conn *c){
 static const char header[]="HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store\r\nConnection: keep-alive\r\n\r\n";
 int n;
 mttl_worker_status status;mttl_sdk_status_snapshot(&status);
 if(status.state!=5){httpd_response_too_many_requests(c,"Event stream requires station connection");httpd_conn_close(c);return;}
 if(!httpd_request_is_method(c,"GET")||c->tls||c->request.content_len){httpd_response_bad_request(c,0);httpd_conn_close(c);return;}
 if(socket_fd>=0){httpd_response_too_many_requests(c,"One event stream only");httpd_conn_close(c);return;}
 frame=pvPortMalloc(1600);if(!frame){httpd_response_internal_server_error(c,0);httpd_conn_close(c);return;}
 n=send(c->sock,header,sizeof(header)-1,MSG_DONTWAIT);
 if(n!=sizeof(header)-1){vPortFree(frame);frame=0;httpd_conn_close(c);return;}
 socket_fd=c->sock;c->sock=-1;httpd_conn_close(c); /* Free request, retain socket. */
 sent_hash=pending_hash=0;size=offset=0;sent_at=now()-10000;
 sent_relay=0xffffffffu;relay_frame=0;
}
void mttl_sdk_sse_pump(void){
 uint32_t time=now();fd_set writable;struct timeval timeout={0,0};int n;
 if(socket_fd<0)return;
 if(!size){size_t count,i;uint32_t hash=2166136261u,relay=mttl_sdk_relay_snapshot();
   relay_frame=relay!=sent_relay;
   if(relay_frame){int length=snprintf(frame,1600,"event: relay\ndata: {\"known\":%lu,\"on\":%lu,\"fault\":%lu}\n\n",(unsigned long)(relay&15),(unsigned long)((relay>>4)&15),(unsigned long)(relay>>8));if(length<=0||length>=1600){mttl_sdk_sse_close();return;}size=(size_t)length;pending_relay=relay;}
   else{
   memcpy(frame,"event: device\ndata: ",20);count=mttl_sdk_device_json(frame+20,1575,0);
   if(!count)return;
   for(i=0;i<count;i++)hash=(hash^(uint8_t)frame[20+i])*16777619u;
   if(hash!=sent_hash){frame[count+20]='\n';frame[count+21]='\n';size=count+22;pending_hash=hash;}
   else if((uint32_t)(time-sent_at)>=10000){memcpy(frame,": keepalive\n\n",13);size=13;pending_hash=sent_hash;}
   else return;
   }
   offset=0;queued_at=time;
 }
 if((uint32_t)(time-queued_at)>5000){mttl_sdk_sse_close();return;}
 FD_ZERO(&writable);FD_SET(socket_fd,&writable);
 n=select(socket_fd+1,0,&writable,0,&timeout);if(n<0){mttl_sdk_sse_close();return;}if(!n)return;
 {size_t count=size-offset;if(count>1024)count=1024;n=send(socket_fd,frame+offset,count,MSG_DONTWAIT);}
 if(n<=0){mttl_sdk_sse_close();return;}offset+=n;
 if(offset==size){if(relay_frame)sent_relay=pending_relay;else sent_hash=pending_hash;sent_at=time;size=offset=0;}
}
