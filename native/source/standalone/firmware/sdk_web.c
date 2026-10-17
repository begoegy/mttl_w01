#include "sdk_web.h"
#include "httpd.h"
#include "sdk_worker.h"
#include "wifi_form.h"
#include "sdk_wifi_adapter.h"
#include "ui_assets.h"
#include "sdk_sse.h"
#include "sdk_relay.h"
#ifdef MTTL_OTA_TEST
#include "ota_test_config.h"
#include "sdk_ota_runtime.h"
#include "sys_api.h"
#include "FreeRTOS.h"
#include "task.h"
extern void mttl_http_ota_budget(struct httpd_conn *);
#endif
static int started;
static void ota_redirect(struct httpd_conn *c){
 if(!httpd_request_is_method(c,"GET")){httpd_response_method_not_allowed(c,0);httpd_conn_close(c);return;}
 if(httpd_response_write_header_start(c,"302 Found","text/plain",0)>=0&&httpd_response_write_header(c,"Location","/#firmware")>=0&&httpd_response_write_header(c,"Cache-Control","no-store")>=0&&httpd_response_write_header(c,"Connection","close")>=0)httpd_response_write_header_finish(c);
 httpd_conn_close(c);
}
extern int mttl_httpd_stopped(void);
static const char version[]="{\"version\":\"2.0.12\"}";
static void response(struct httpd_conn *connection,const char *body,size_t length,char *type){
    if(httpd_response_write_header_start(connection,"200 OK",type,length)<0 ||
       httpd_response_write_header(connection,"Cache-Control","no-store")<0 ||
       httpd_response_write_header(connection,"Connection","close")<0 ||
       httpd_response_write_header_finish(connection)<0){httpd_conn_close(connection);return;}
    {size_t sent=0;while(sent<length){int n=httpd_response_write_data(connection,(uint8_t *)body+sent,length-sent);if(n<=0||(size_t)n>length-sent)break;sent+=n;}}
    httpd_conn_close(connection);
}
static void send(struct httpd_conn *connection,const char *body,size_t length,char *type){
    if(!httpd_request_is_method(connection,"GET")){
        httpd_response_method_not_allowed(connection,0);httpd_conn_close(connection);return;
    }
    response(connection,body,length,type);
}
static void asset(struct httpd_conn *c){
    unsigned i;size_t sent=0;const mttl_asset *file=0;
    if(!httpd_request_is_method(c,"GET")){httpd_response_method_not_allowed(c,0);httpd_conn_close(c);return;}
    for(i=0;i<sizeof(mttl_assets)/sizeof(mttl_assets[0]);i++)if(strlen(mttl_assets[i].path)==c->request.path_len&&!memcmp(c->request.path,mttl_assets[i].path,c->request.path_len)){file=&mttl_assets[i];break;}
    if(!file){httpd_response_not_found(c,0);httpd_conn_close(c);return;}
    if(httpd_response_write_header_start(c,"200 OK",(char *)file->type,file->size)<0||httpd_response_write_header(c,"Content-Encoding","gzip")<0||httpd_response_write_header(c,"Cache-Control","no-store")<0||httpd_response_write_header(c,"Connection","close")<0||httpd_response_write_header_finish(c)<0){httpd_conn_close(c);return;}
    while(sent<file->size){int n=httpd_response_write_data(c,(uint8_t *)file->data+sent,file->size-sent);if(n<=0||(size_t)n>file->size-sent)break;sent+=n;}
    httpd_conn_close(c);
}
static void hex_ssid(const char *ssid,char hex[65]){unsigned i;static const char digits[]="0123456789abcdef";for(i=0;i<32&&ssid[i];i++){unsigned char c=(unsigned char)ssid[i];hex[2*i]=digits[c>>4];hex[2*i+1]=digits[c&15];}hex[2*i]=0;}
/* SINGLE-mode HTTP worker only; static response/snapshot avoids large stacks. */
/* Largest reply: 16 scan entries (<2 KiB); device/config snapshots <=2 KiB. */
/* HTTP_THREAD_SINGLE: configuration bytes are copied by config_reserve
 * before responding. The input and output never need separate storage. */
static union {char json[1792];uint8_t settings[1024];} http_scratch;
#define json http_scratch.json
static int origin_ok(struct httpd_conn *c);
static void control_page(struct httpd_conn *c){uint8_t body[2];size_t n=0;
 if(httpd_request_is_method(c,"GET")){uint32_t t[4];int length;mttl_sdk_control_timing(t);length=snprintf(json,sizeof(json),"{\"sequence\":%lu,\"wait_ms\":%lu,\"run_ms\":%lu,\"busy\":%lu}",(unsigned long)t[0],(unsigned long)t[1],(unsigned long)t[2],(unsigned long)t[3]);if(length>0&&(size_t)length<sizeof(json))response(c,json,length,"application/json");else httpd_conn_close(c);return;}
 if(!httpd_request_is_method(c,"POST")){httpd_response_method_not_allowed(c,0);goto close;}
 if(!origin_ok(c)||c->request.content_len!=2){httpd_response_bad_request(c,"Two-byte channel/on command required");goto close;}
 while(n<2){int count=httpd_request_read_data(c,body+n,2-n);if(count<=0||(size_t)count>2-n)goto close;n+=count;}
 if(!mttl_sdk_control_request(body[0],body[1])){httpd_response_too_many_requests(c,"Output disabled, busy or faulted");goto close;}
 {static const char accepted[]="{\"accepted\":true,\"contact_feedback\":false}";response(c,accepted,sizeof(accepted)-1,"application/json");return;}
close:httpd_conn_close(c);
}
static void device_page(struct httpd_conn *c){size_t n=mttl_sdk_device_json(json,sizeof(json),0);if(!n){httpd_response_internal_server_error(c,0);httpd_conn_close(c);return;}send(c,json,n,"application/json");}
/* Same-origin AP/LAN configuration; owner confirms persistent flash save. */
static void config_page(struct httpd_conn *c){
    uint8_t *body=http_scratch.settings;size_t used=0,n;int accepted=0;char *type=0;
    if(httpd_request_is_method(c,"GET")){n=mttl_sdk_device_json(json,sizeof(json),1);if(n)send(c,json,n,"application/json");else{httpd_response_internal_server_error(c,0);httpd_conn_close(c);}return;}
    if(!httpd_request_is_method(c,"POST")){httpd_response_method_not_allowed(c,0);httpd_conn_close(c);return;}
    if(!origin_ok(c)){httpd_response_bad_request(c,"Same-origin required");httpd_conn_close(c);return;}
    httpd_request_get_header_field(c,"Content-Type",&type);accepted=type&&!strcmp(type,"application/octet-stream");if(type)httpd_free(type);
    if(!accepted||!c->request.content_len||c->request.content_len>sizeof(http_scratch.settings)){httpd_response_bad_request(c,"Invalid settings body");httpd_conn_close(c);return;}accepted=0;
    while(used<c->request.content_len){int count=httpd_request_read_data(c,body+used,c->request.content_len-used);if(count<=0||(size_t)count>c->request.content_len-used)goto close;used+=(size_t)count;}
    {char *keep=0;int preserve;httpd_request_get_header_field(c,"X-MTTL-Keep-Password",&keep);preserve=keep&&!strcmp(keep,"1");if(keep)httpd_free(keep);if(!mttl_sdk_config_reserve(body,used,preserve)){httpd_response_bad_request(c,"Invalid settings or not ready");goto close;}}
    {static const char response[]="{\"accepted\":true,\"pending\":true}";if(httpd_response_write_header_start(c,"202 Accepted","application/json",sizeof(response)-1)>=0&&httpd_response_write_header(c,"Connection","close")>=0&&httpd_response_write_header(c,"Cache-Control","no-store")>=0&&httpd_response_write_header_finish(c)>=0&&httpd_response_write_data(c,(uint8_t *)response,sizeof(response)-1)==sizeof(response)-1)accepted=1;}
    httpd_conn_close(c);mttl_sdk_config_release(accepted);goto wipe;
close:httpd_conn_close(c);
wipe:{volatile uint8_t *p=body;for(n=0;n<sizeof(http_scratch.settings);n++)p[n]=0;}
}
/* Vendor ROM snprintf does not reliably implement %llu. */
static void decimal64(uint64_t value,char out[32]){char reversed[20];unsigned n=0,i;do{reversed[n++]=(char)('0'+value%10);value/=10;}while(value);for(i=0;i<n;i++)out[i]=reversed[n-1-i];out[n]=0;}
static void status_page(struct httpd_conn *c){
    mttl_worker_status status;char hex[65],rssi[20];int size;
    mttl_sdk_status_snapshot(&status);hex_ssid(status.ssid,hex);
    if(status.rssi_valid)snprintf(rssi,sizeof(rssi),"%ld",(long)status.rssi);else strcpy(rssi,"null");
    {char time[32],connected[32];strcpy(time,"null");strcpy(connected,"null");if(status.time_valid)decimal64(status.unix_ms,time);if(status.connected_unix_ms)decimal64(status.connected_unix_ms,connected);
    size=snprintf(json,sizeof(json),"{\"state\":%lu,\"ip\":%lu,\"uptime_ms\":%lu,\"rssi\":%s,\"ssid_hex\":\"%s\",\"mac\":\"%s\",\"unix_ms\":%s,\"connected_unix_ms\":%s,\"meter_initialized\":%u,\"meter_reads\":%u,\"energy_reads\":%u,\"energy_counter_raw\":[%lu,%lu,%lu,%lu],\"energy_restore_pending\":%u,\"hardware_ready\":%s}",(unsigned long)status.state,(unsigned long)status.ip,(unsigned long)status.uptime_ms,rssi,hex,status.mac,time,connected,(unsigned)status.meter_initialized,(unsigned)status.meter_reads,(unsigned)status.energy_reads,(unsigned long)status.energy_raw[0],(unsigned long)status.energy_raw[1],(unsigned long)status.energy_raw[2],(unsigned long)status.energy_raw[3],(unsigned)status.energy_restore_pending,status.hardware_ready?"true":"false");}
    if(size<0||(size_t)size>=sizeof(json)){httpd_response_internal_server_error(c,0);httpd_conn_close(c);return;}send(c,json,size,"application/json");
}
static void memory_page(struct httpd_conn *c){
 mttl_worker_status status;int size;mttl_sdk_status_snapshot(&status);
 size=snprintf(json,sizeof(json),"{\"heap_free_bytes\":%lu,\"heap_min_free_bytes\":%lu,\"worker_observed_stack_descent_bytes\":%lu,\"stack_high_water_verified\":false}",(unsigned long)status.heap_free_bytes,(unsigned long)status.heap_min_free_bytes,(unsigned long)status.worker_observed_stack_descent_bytes);
 if(size<0||(size_t)size>=sizeof(json)){httpd_response_internal_server_error(c,0);httpd_conn_close(c);return;}send(c,json,size,"application/json");
}
static void energy_page(struct httpd_conn *c){
 mttl_worker_status status;int size;mttl_sdk_status_snapshot(&status);
 size=snprintf(json,sizeof(json),"{\"loaded_mask\":%u,\"saved_mask\":%u,\"restore_pending\":%u,\"save_count\":%lu,\"save_failures\":%lu}",(unsigned)status.energy_loaded_mask,(unsigned)status.energy_saved_mask,(unsigned)status.energy_restore_pending,(unsigned long)status.energy_save_count,(unsigned long)status.energy_save_failures);
 if(size<0||(size_t)size>=sizeof(json)){httpd_response_internal_server_error(c,0);httpd_conn_close(c);return;}send(c,json,size,"application/json");
}
static int origin_ok(struct httpd_conn *c){char *origin=0,wanted[48];int valid;mttl_worker_status status;mttl_sdk_status_snapshot(&status);if((status.state!=MTTL_NET_AP&&status.state!=MTTL_NET_ONLINE)||!status.ip)return 0;snprintf(wanted,sizeof(wanted),"http://%lu.%lu.%lu.%lu",(unsigned long)(status.ip>>24),(unsigned long)((status.ip>>16)&255),(unsigned long)((status.ip>>8)&255),(unsigned long)(status.ip&255));httpd_request_get_header_field(c,"Origin",&origin);valid=origin&&!strcmp(origin,wanted);if(origin)httpd_free(origin);return valid;}
static void scan_page(struct httpd_conn *c){
    /* Keep formatter/socket stack headroom. GET-only bounded allocation;
     * allocation failure is a HTTP error, never a scan-task reset. */
    mttl_scan scan_copy;
    char *buffer=0;const size_t cap=2304;
    unsigned i;int size;size_t used;char hex[65];int32_t diagnostics[4];
    if(httpd_request_is_method(c,"POST")){
        static const char response[]="{\"accepted\":true}";
        if(!origin_ok(c)){httpd_response_bad_request(c,"Invalid origin");httpd_conn_close(c);return;}
        if(!mttl_sdk_scan_request()){httpd_response_too_many_requests(c,"Busy");httpd_conn_close(c);return;}
        httpd_response_write_header_start(c,"202 Accepted","application/json",sizeof(response)-1);httpd_response_write_header(c,"Cache-Control","no-store");httpd_response_write_header(c,"Connection","close");httpd_response_write_header_finish(c);httpd_response_write_data(c,(uint8_t *)response,sizeof(response)-1);httpd_conn_close(c);return;
    }
    buffer=pvPortMalloc(cap);if(!buffer)goto failure;
    mttl_sdk_scan_snapshot(&scan_copy);
    mttl_sdk_scan_diagnostics(diagnostics);
    size=snprintf(buffer,cap,"{\"pending\":%s,\"failed\":%s,\"truncated\":%s,\"generation\":%lu,\"reports\":%ld,\"rejected\":%ld,\"inferred_lengths\":%ld,\"sdk_result\":%ld,\"networks\":[",scan_copy.pending||mttl_sdk_scan_queued()?"true":"false",mttl_sdk_scan_failed()?"true":"false",scan_copy.truncated?"true":"false",(unsigned long)scan_copy.generation,(long)diagnostics[0],(long)diagnostics[1],(long)diagnostics[2],(long)diagnostics[3]);
    if(size<0||(size_t)size>=cap)goto failure;used=size;
    for(i=0;i<scan_copy.count;i++){
        hex_ssid(scan_copy.entries[i].ssid,hex);
        size=snprintf(buffer+used,cap-used,"%s{\"ssid_hex\":\"%s\",\"rssi\":%ld,\"security\":%lu}",i?",":"",hex,(long)scan_copy.entries[i].rssi,(unsigned long)scan_copy.entries[i].security);
        if(size<0||(size_t)size>=cap-used)goto failure;used+=size;
    }
    if(used+3>cap)goto failure;buffer[used++]=']';buffer[used++]='}';buffer[used]=0;send(c,buffer,used,"application/json");goto cleanup;
failure:
    httpd_response_internal_server_error(c,0);httpd_conn_close(c);
cleanup:
    if(buffer)vPortFree(buffer);
}
static void info(struct httpd_conn *c){send(c,version,sizeof(version)-1,"application/json");}
#ifdef MTTL_OTA_TEST
static void ota_status(struct httpd_conn *c){
    uint32_t active=mttl_sdk_ota_probe();int n=snprintf(json,sizeof(json),"{\"version\":\"2.0.12\",\"build_id\":\"" MTTL_OTA_BUILD_ID "\",\"ota_ready\":%s,\"active_slot\":%lu,\"settings_persistent\":%s,\"restore_versions\":[\"1.0.66\",\"1.0.68\"]}",active?"true":"false",(unsigned long)active,mttl_sdk_settings_persistent()?"true":"false");
    if(n>0&&(size_t)n<sizeof(json))send(c,json,n,"application/json");else{httpd_response_internal_server_error(c,0);httpd_conn_close(c);}
}
static int ota_access(struct httpd_conn *c){
    mttl_worker_status status;char wanted[48],*value=0;int ok;
    mttl_sdk_status_snapshot(&status);
    if((status.state!=MTTL_NET_AP&&status.state!=MTTL_NET_ONLINE)||!status.ip)return 0;
    snprintf(wanted,sizeof(wanted),"http://%lu.%lu.%lu.%lu",(unsigned long)(status.ip>>24),(unsigned long)((status.ip>>16)&255),(unsigned long)((status.ip>>8)&255),(unsigned long)(status.ip&255));
    httpd_request_get_header_field(c,"Origin",&value);ok=value&&!strcmp(value,wanted);if(value)httpd_free(value);if(!ok)return 0;
    return ok;
}
static void ota_upload(struct httpd_conn *c){
    static const uint8_t seed[16]={0xca,0x59,0x23,0xe7,0xd7,0x9a,0x84,0xe8,0x43,0xb3,0xaf,0xe7,0x94,0xb1,0xaf,0xa7};
    uint8_t expected[32]={0},key[16],chunk[256];uint32_t received=0;char *value=0;int ok;
    if(!httpd_request_is_method(c,"POST")){httpd_response_method_not_allowed(c,0);goto close;}
    if(!ota_access(c)){httpd_response_bad_request(c,"Same-origin required");goto close;}
    value=0;httpd_request_get_header_field(c,"Content-Type",&value);ok=value&&!strcmp(value,"application/x-mttl-fwr");if(value)httpd_free(value);if(!ok){httpd_response_bad_request(c,"FWR required");goto close;}
    mttl_http_ota_budget(c);
    mttl_sdk_sse_close(); /* Reclaim stream buffer before flash workspace. */
    /* Drain an in-flight pulse and hold its owner BEFORE any OTA flash write. */
    if(!mttl_sdk_energy_barrier()){httpd_response_bad_request(c,"Owner checkpoint/hold failed");goto close;}
    if(!mttl_sdk_ota_begin(c->request.content_len,expected)){mttl_sdk_energy_release();httpd_response_bad_request(c,"Geometry or unique running slot not verified; no write");goto close;}
    while(received<c->request.content_len){size_t take=c->request.content_len-received,i;int n;if(take>sizeof(chunk))take=sizeof(chunk);n=httpd_request_read_data(c,chunk,take);if(n<=0||(size_t)n>take)goto fail;for(i=0;i<(size_t)n;i++){uint32_t at=received+i;if(at<16)key[at]=(uint8_t)((seed[at]^chunk[i])+1);else chunk[i]^=key[at&15];}if(!mttl_sdk_ota_feed(chunk,n))goto fail;received+=(uint32_t)n;}
    if(!mttl_sdk_ota_finish())goto fail;
    {static const char done[]="{\"written\":true,\"activated\":true,\"rebooting\":true}";
     static const char stock[]="{\"written\":true,\"activated\":true,\"rebooting\":true,\"restore_version\":\"1.0.66\"}";
     static const char local[]="{\"written\":true,\"activated\":true,\"rebooting\":true,\"restore_version\":\"1.0.68\"}";
     int version=mttl_sdk_ota_restore_version();if(version==66)response(c,stock,sizeof(stock)-1,"application/json");else if(version==68)response(c,local,sizeof(local)-1,"application/json");else response(c,done,sizeof(done)-1,"application/json");}
    vTaskDelay(1000/portTICK_RATE_MS+1);sys_reset();return;
fail:mttl_sdk_energy_release();mttl_sdk_ota_abort();httpd_response_bad_request(c,"OTA failed; checkpoint or image activation not completed");
close:httpd_conn_close(c);
}
#endif
static void configure(struct httpd_conn *c){
    unsigned char body[321];char ssid[33],password[65];size_t received=0;int accepted=0;
    if(!httpd_request_is_method(c,"POST")){httpd_response_method_not_allowed(c,0);goto close;}
    if(!origin_ok(c)){httpd_response_bad_request(c,"Invalid origin");goto close;}
    {
        char *type=0;int valid;
        httpd_request_get_header_field(c,"Content-Type",&type);
        valid=mttl_wifi_form_type(type);if(type)httpd_free(type);
        if(!valid){httpd_response_bad_request(c,"Expected form content type");goto close;}
    }
    if(!c->request.content_len||c->request.content_len>320){httpd_response_bad_request(c,"Invalid body size");goto close;}
    while(received<c->request.content_len){int n=httpd_request_read_data(c,body+received,c->request.content_len-received);if(n<=0||(size_t)n>c->request.content_len-received)goto close;received+=n;}
    if(!mttl_wifi_form(body,received,ssid,password)){httpd_response_bad_request(c,"Invalid Wi-Fi settings");goto clear;}
    if(!mttl_sdk_wifi_reserve(ssid,password)){httpd_response_too_many_requests(c,"Not ready or busy");goto clear;}
    {
        static const char response[]="{\"accepted\":true,\"persistent\":false}";
        if(httpd_response_write_header_start(c,"202 Accepted","application/json",sizeof(response)-1)>=0 &&
           httpd_response_write_header(c,"Cache-Control","no-store")>=0 &&
           httpd_response_write_header(c,"Connection","close")>=0 &&
           httpd_response_write_header_finish(c)>=0 &&
           httpd_response_write_data(c,(uint8_t *)response,sizeof(response)-1)==sizeof(response)-1)accepted=1;
    }
    httpd_conn_close(c);mttl_sdk_wifi_release(accepted);
    goto clear_only;
clear:
    httpd_conn_close(c);
clear_only:
    {volatile unsigned char *p=body;volatile char *s=ssid,*key=password;size_t i;for(i=0;i<sizeof(body);i++)p[i]=0;for(i=0;i<33;i++)s[i]=0;for(i=0;i<65;i++)key[i]=0;}
    return;
close:
    httpd_conn_close(c);goto clear_only;
}
int mttl_sdk_web_start(void){
    unsigned i;
    if(started)return 1;
    if(!mttl_httpd_stopped())return 0;
    httpd_setup_debug(HTTPD_DEBUG_OFF);
#ifdef MTTL_OTA_TEST
    if(httpd_reg_page_callback("/ota",ota_redirect)<0||httpd_reg_page_callback("/api/firmware/status",ota_status)<0||httpd_reg_page_callback("/api/firmware/upload",ota_upload)<0){httpd_clear_page_callbacks();return 0;}
#endif
    if(httpd_reg_page_callback("/api/control",control_page)<0){httpd_clear_page_callbacks();return 0;}
    if(httpd_reg_page_callback("/api/events",mttl_sdk_sse_open)<0){httpd_clear_page_callbacks();return 0;}
    for(i=0;i<sizeof(mttl_assets)/sizeof(mttl_assets[0]);i++)if(httpd_reg_page_callback((char *)mttl_assets[i].path,asset)<0){httpd_clear_page_callbacks();return 0;}
    if(httpd_reg_page_callback("/api/device",device_page)<0 || httpd_reg_page_callback("/api/config",config_page)<0 || httpd_reg_page_callback("/api/version",info)<0 || httpd_reg_page_callback("/api/wifi",configure)<0 || httpd_reg_page_callback("/api/status",status_page)<0 || httpd_reg_page_callback("/api/scan",scan_page)<0 || httpd_reg_page_callback("/api/memory",memory_page)<0 || httpd_reg_page_callback("/api/energy",energy_page)<0){httpd_clear_page_callbacks();return 0;}
    if(httpd_start(80,2,4096,HTTPD_THREAD_SINGLE,HTTPD_SECURE_NONE)<0){httpd_clear_page_callbacks();return 0;}
    started=1;return 1;
}
void mttl_sdk_web_stop(void){if(started){httpd_stop();started=0;}}
int mttl_sdk_web_stopped(void){return mttl_httpd_stopped();}
