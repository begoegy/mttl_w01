/* Keep vendor utility code intact; replace only socket I/O entry points. */
#define httpd_read mttl_vendor_httpd_read
#define httpd_write mttl_vendor_httpd_write
#include "../../work/reference/ameba-arduino-1/Arduino_package/hardware/system/component/common/network/httpd/httpd_util.c"
#undef httpd_read
#undef httpd_write
#include "deadline.h"
#include <errno.h>
#include "sdk_worker.h"
volatile int mttl_http_last_send_result,mttl_http_last_send_error;
volatile uint32_t mttl_http_last_send_size;
static struct httpd_conn *active;
static uint32_t started;
static uint32_t budget=5000;
static int receive_timeout,send_timeout;
static uint32_t now_ms(void){return (uint32_t)xTaskGetTickCount()*portTICK_RATE_MS;}
/* SINGLE HTTP worker only. One deadline includes header/body/response. */
void mttl_http_begin(struct httpd_conn *c){active=c;started=now_ms();budget=5000;receive_timeout=send_timeout=0;}
#ifdef MTTL_OTA_TEST
/* Only the same-origin FWR upload handler extends the total deadline. */
void mttl_http_ota_budget(struct httpd_conn *c){if(c==active)budget=120000;}
#endif
void mttl_http_end(void){active=0;}
static int prepare(struct httpd_conn *c,int option){
    uint32_t remaining;int timeout,*cached=option==SO_RCVTIMEO?&receive_timeout:&send_timeout;
    if(c!=active||c->sock<0||c->tls)return 0;
    remaining=mttl_remaining(started,now_ms(),budget);
    if(!remaining)return 0;
    timeout=(int)(remaining>1000?1000:remaining);
    if(*cached==timeout)return 1;
    if(setsockopt(c->sock,SOL_SOCKET,option,&timeout,sizeof(timeout))!=0)return 0;
    *cached=timeout;return 1;
}
int httpd_read(struct httpd_conn *c,uint8_t *buf,size_t length){
    int result=prepare(c,SO_RCVTIMEO)?mttl_vendor_httpd_read(c,buf,length):-1;
    if(result>0)mttl_sdk_watchdog_http(1);
    return result;
}
int httpd_write(struct httpd_conn *c,uint8_t *buf,size_t length){
    uint32_t remaining;fd_set writable;struct timeval timeout;int ready,n,error;unsigned attempts=0;size_t error_size;
    if(c!=active||c->sock<0||c->tls||!buf)return -1;
    if(length>256)length=256;
    mttl_http_last_send_size=length;mttl_http_last_send_error=0;
    while(attempts++<64){
        remaining=mttl_remaining(started,now_ms(),budget);if(!remaining)break;
        if(remaining>1000)remaining=1000;
        timeout.tv_sec=remaining/1000;timeout.tv_usec=(remaining%1000)*1000;
        FD_ZERO(&writable);FD_SET(c->sock,&writable);
        ready=select(c->sock+1,0,&writable,0,&timeout);
        if(ready<0)break;
        if(!mttl_remaining(started,now_ms(),budget))break;
        if(!ready||!FD_ISSET(c->sock,&writable))continue;
        n=send(c->sock,buf,length,MSG_DONTWAIT);mttl_http_last_send_result=n;
        if(n>=0){if(n>0)mttl_sdk_watchdog_http(1);return n;}
        /* Per-socket error avoids relying on SDK/newlib global errno sharing. */
        error=0;error_size=sizeof(error);
        if(getsockopt(c->sock,SOL_SOCKET,SO_ERROR,&error,&error_size)!=0)break;
        mttl_http_last_send_error=error;
        if(error!=EWOULDBLOCK&&error!=EAGAIN&&error!=ENOMEM&&error!=ENOBUFS)break;
        vTaskDelay(10/portTICK_RATE_MS+1);
    }
    mttl_http_last_send_result=-1;return -1;
}
