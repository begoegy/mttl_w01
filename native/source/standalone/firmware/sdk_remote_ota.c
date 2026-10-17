#ifdef MTTL_NATIVE_OTA
#include "sdk_remote_ota.h"
#include "sfn_ota_http.h"
#include "sfn_ota_auth.h"
#include "sfn_ota_policy.h"
#include "sfn_native_build_config.h"
#include "sdk_ota_runtime.h"
#include "sdk_boot_guard.h"
#include "sdk_worker.h"
#include "lwip/sockets.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sys_api.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>

static volatile uint8_t ota_state; /* 0 idle, 1 accepted/reply pending, 2 worker running */
static uint32_t ota_backend_ipv4,ota_sequence,ota_bytes;
static uint8_t ota_sha256[32];

static int safe_backend(uint32_t ip){unsigned first=(unsigned)(ip>>24);return ip==SFN_NATIVE_BACKEND_IPV4&&first!=0&&first!=127&&first<224&&ip!=0xffffffffu;}
static void ipv4_text(uint32_t ip,char out[16]){snprintf(out,16,"%lu.%lu.%lu.%lu",(unsigned long)(ip>>24),(unsigned long)((ip>>16)&255u),(unsigned long)((ip>>8)&255u),(unsigned long)(ip&255u));}
static int timeout_socket(int fd){struct timeval r={10,0},w={5,0};return setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&r,sizeof(r))==0&&setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&w,sizeof(w))==0;}
static int send_all(int fd,const uint8_t *p,size_t n){while(n){int r=send(fd,p,n,0);if(r<=0||(size_t)r>n)return 0;p+=r;n-=(size_t)r;}return 1;}
static int connect_http(uint32_t ip){struct sockaddr_in a;int fd;if(!safe_backend(ip))return -1;fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;if(!timeout_socket(fd)){close(fd);return -1;}memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(80);a.sin_addr.s_addr=htonl(ip);if(connect(fd,(struct sockaddr *)&a,sizeof(a))!=0){close(fd);return -1;}return fd;}
static void idle(void){taskENTER_CRITICAL();ota_backend_ipv4=ota_sequence=ota_bytes=0;memset(ota_sha256,0,sizeof(ota_sha256));ota_state=0;taskEXIT_CRITICAL();}
static int request_line(char *out,size_t cap,uint32_t ip,uint32_t sequence,const char *suffix){char host[16];int n;ipv4_text(ip,host);n=snprintf(out,cap,"GET /native-ota/%lu.%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",(unsigned long)sequence,suffix,host);return n>0&&(size_t)n<cap?n:0;}

static int fetch_signature(uint32_t ip,uint32_t expected_sequence,uint32_t expected_bytes,const uint8_t expected_sha[32]){
 char request[128];uint8_t header[512],signature[SFN_OTA_SIGNATURE_BYTES];size_t used=0,body=0,got=0;uint32_t length=0;int fd=-1,ok=0,nreq;
 fd=connect_http(ip);if(fd<0)goto done;nreq=request_line(request,sizeof(request),ip,expected_sequence,"sig");if(!nreq||!send_all(fd,(const uint8_t *)request,(size_t)nreq))goto done;
 while(used<sizeof(header)){int n=recv(fd,header+used,sizeof(header)-used,0),parsed;if(n<=0||(size_t)n>sizeof(header)-used)goto done;used+=(size_t)n;parsed=sfn_ota_http_header(header,used,&length,&body);if(parsed<0)goto done;if(parsed==1){size_t have=used-body;if(length!=SFN_OTA_SIGNATURE_BYTES||have>sizeof(signature))goto done;if(have){memcpy(signature,header+body,have);got=have;}break;}}
 while(got<sizeof(signature)){int n=recv(fd,signature+got,sizeof(signature)-got,0);if(n<=0||(size_t)n>sizeof(signature)-got)goto done;got+=(size_t)n;}
 ok=sfn_ota_auth_verify(expected_sequence,expected_bytes,expected_sha,signature,sizeof(signature));
done:if(fd>=0)close(fd);memset(signature,0,sizeof(signature));memset(header,0,sizeof(header));return ok;
}

static int stream_firmware(int fd,uint8_t *first,size_t first_n,size_t body_offset,uint32_t expected_sequence,uint32_t total,const uint8_t expected_fwr_sha[32]){
 uint8_t chunk[512],zero_sha[32]={0};uint32_t received=0;sfn_fwr_decoder decoder;sfn_ota_integrity integrity;
 sfn_fwr_decoder_init(&decoder);if(!sfn_ota_integrity_init(&integrity,total,expected_fwr_sha)||!mttl_sdk_ota_begin(expected_sequence,total,zero_sha))return 0;
 if(first_n>body_offset){size_t n=first_n-body_offset;if(n>total)goto fail;if(!sfn_ota_integrity_feed(&integrity,first+body_offset,n)||!sfn_fwr_decode(&decoder,first+body_offset,n)||!mttl_sdk_ota_feed(first+body_offset,n))goto fail;received=(uint32_t)n;}
 while(received<total){size_t want=total-received;int n;if(want>sizeof(chunk))want=sizeof(chunk);n=recv(fd,chunk,want,0);if(n<=0||(size_t)n>want)goto fail;if(!sfn_ota_integrity_feed(&integrity,chunk,(size_t)n)||!sfn_fwr_decode(&decoder,chunk,(size_t)n)||!mttl_sdk_ota_feed(chunk,(size_t)n))goto fail;received+=(uint32_t)n;}
 if(!sfn_ota_integrity_finish(&integrity))goto fail;
 memset(&decoder,0,sizeof(decoder));memset(&integrity,0,sizeof(integrity));memset(chunk,0,sizeof(chunk));return mttl_sdk_ota_finish();
fail:memset(&decoder,0,sizeof(decoder));memset(&integrity,0,sizeof(integrity));memset(chunk,0,sizeof(chunk));mttl_sdk_ota_abort();return 0;
}

static void ota_task(void *unused){char request[128];uint8_t header[512],expected_sha[32];size_t used=0,body=0;uint32_t length=0,ip=0,expected_sequence=0,expected_bytes=0;int fd=-1,ok=0,nreq;(void)unused;
 taskENTER_CRITICAL();ip=ota_backend_ipv4;expected_sequence=ota_sequence;expected_bytes=ota_bytes;memcpy(expected_sha,ota_sha256,sizeof(expected_sha));taskEXIT_CRITICAL();
 /* Authenticate release metadata before taking the flash/relay barrier. */
 if(!fetch_signature(ip,expected_sequence,expected_bytes,expected_sha))goto done;
 if(!mttl_sdk_energy_barrier())goto done;fd=connect_http(ip);if(fd<0)goto release;nreq=request_line(request,sizeof(request),ip,expected_sequence,"fwr");if(!nreq||!send_all(fd,(const uint8_t *)request,(size_t)nreq))goto release;
 while(used<sizeof(header)){int n=recv(fd,header+used,sizeof(header)-used,0),parsed;if(n<=0||(size_t)n>sizeof(header)-used)goto release;used+=(size_t)n;parsed=sfn_ota_http_header(header,used,&length,&body);if(parsed<0)goto release;if(parsed==1){if(length!=expected_bytes)goto release;ok=stream_firmware(fd,header,used,body,expected_sequence,length,expected_sha);break;}}
release:if(fd>=0)close(fd);memset(expected_sha,0,sizeof(expected_sha));if(!ok){mttl_sdk_ota_abort();mttl_sdk_energy_release();goto done;}vTaskDelay(500/portTICK_RATE_MS+1);sys_reset();for(;;)vTaskDelay(portMAX_DELAY);
done:idle();vTaskDelete(0);}

int sfn_sdk_remote_ota_request(uint32_t backend_ipv4,uint32_t sequence,uint32_t bytes,const uint8_t sha256[32]){int ok=0;unsigned i;uint8_t any=0;if(!sha256||!safe_backend(backend_ipv4)||!sfn_ota_sequence_allowed(SFN_NATIVE_OTA_SEQUENCE,sequence)||bytes<44u||bytes>0x6a014u||!sfn_sdk_boot_guard_trusted())return 0;for(i=0;i<32;i++)any|=sha256[i];if(!any)return 0;taskENTER_CRITICAL();if(!ota_state){ota_backend_ipv4=backend_ipv4;ota_sequence=sequence;ota_bytes=bytes;memcpy(ota_sha256,sha256,32);ota_state=1;ok=1;}taskEXIT_CRITICAL();return ok;}
void sfn_sdk_remote_ota_start_if_ready(int drained){int start=0;if(!drained)return;taskENTER_CRITICAL();if(ota_state==1){ota_state=2;start=1;}taskEXIT_CRITICAL();if(start&&xTaskCreate(ota_task,"sfn_ota",1280,0,tskIDLE_PRIORITY+1,0)!=pdPASS)idle();}
int sfn_sdk_remote_ota_busy(void){int busy;taskENTER_CRITICAL();busy=ota_state!=0;taskEXIT_CRITICAL();return busy;}
#endif
