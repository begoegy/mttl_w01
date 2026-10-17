#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_ota_http.h"
int main(void){uint32_t n;size_t off;uint8_t h[512];sfn_fwr_decoder d;uint8_t x[40],y[40];size_t i;const char *good="HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 316216\r\nConnection: close\r\n\r\n";assert(sfn_ota_http_header((const uint8_t *)good,20,&n,&off)==0);assert(sfn_ota_http_header((const uint8_t *)good,strlen(good),&n,&off)==1&&n==316216&&off==strlen(good));assert(sfn_ota_http_header((const uint8_t *)"HTTP/1.1 404 No\r\n\r\n",23,&n,&off)==-1);assert(sfn_ota_http_header((const uint8_t *)"HTTP/1.1 200 OK\r\nContent-Length: 10\r\nContent-Type: application/octet-stream\r\n\r\n",83,&n,&off)==-1);snprintf((char *)h,sizeof(h),"HTTP/1.1 200 OK\r\nContent-Length: 100\r\nContent-Length: 100\r\nContent-Type: application/octet-stream\r\n\r\n");assert(sfn_ota_http_header(h,strlen((char *)h),&n,&off)==-1);
 for(i=0;i<sizeof(x);i++)x[i]=(uint8_t)(i*7u+3u);
 memcpy(y,x,sizeof(x));sfn_fwr_decoder_init(&d);assert(sfn_fwr_decode(&d,y,7));assert(sfn_fwr_decode(&d,y+7,9));assert(sfn_fwr_decode(&d,y+16,sizeof(y)-16));/* decoding twice from a fresh state restores bytes >=16 when the same derived key is applied */{sfn_fwr_decoder e;sfn_fwr_decoder_init(&e);assert(sfn_fwr_decode(&e,y,sizeof(y)));for(i=16;i<sizeof(y);i++)assert(y[i]==x[i]);for(i=0;i<16;i++)assert(y[i]==x[i]);}
 puts("SFN_OTA_HTTP_TESTS=PASS");return 0;}
