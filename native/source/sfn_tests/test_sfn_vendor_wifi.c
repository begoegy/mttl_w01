#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../standalone/firmware/sfn_vendor_wifi.h"
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static void profile(uint8_t *r,unsigned slot,const char *ssid,const char *pw){uint8_t *p=r+slot*SFN_VENDOR_WIFI_PROFILE_SIZE;size_t a=strlen(ssid),b=strlen(pw);p[0]=(uint8_t)a;memcpy(p+1,ssid,a);p[1+a]=0;memcpy(p+0x2c,pw,b);p[0x2c+b]=0;put32(p+0x50,(uint32_t)b);}
typedef struct{int commits;char ssid[33],password[65];} capture;
static int commit(void *ctx,const char *ssid,const char *password){capture *c=ctx;c->commits++;strcpy(c->ssid,ssid);strcpy(c->password,password);return 1;}
int main(void){uint8_t r[SFN_VENDOR_WIFI_RECORD_SIZE];sfn_vendor_wifi_import s;mttl_network n;capture c;uint32_t g;int f=0;
 memset(r,0,sizeof(r));put32(r+0xb0,SFN_VENDOR_WIFI_MARKER);profile(r,0,"Home-A","password1");profile(r,1,"Home-B","password2");if(sfn_vendor_wifi_parse(&s,r,sizeof(r))!=2)f++;
 mttl_network_init(&n);mttl_provision(&n,0);g=n.generation;if(!mttl_stopped(&n,g,0)||!sfn_vendor_wifi_submit_next(&s,&n,1)||n.state!=MTTL_NET_JOINING)f++;
 /* First stored profile is absent: recovery returns to AP without committing it. */
 mttl_provision(&n,2);g=n.generation;if(!mttl_stopped(&n,g,2)||!sfn_vendor_wifi_submit_next(&s,&n,3))f++;
 memset(&c,0,sizeof(c));g=n.generation;if(!mttl_associated(&n,g,4)||!mttl_got_ip(&n,g,0xc000020au,5,commit,&c))f++;
 if(c.commits!=1||strcmp(c.ssid,"Home-B")||strcmp(c.password,"password2")||sfn_vendor_wifi_pending(&s))f++;
 memset(r,0xff,sizeof(r));if(sfn_vendor_wifi_parse(&s,r,sizeof(r))!=0||sfn_vendor_wifi_pending(&s))f++;
 memset(r,0,sizeof(r));put32(r+0xb0,SFN_VENDOR_WIFI_MARKER);profile(r,1,"Second","password2");put32(r,0xffffffffu);if(sfn_vendor_wifi_parse(&s,r,sizeof(r))!=0)f++;
 memset(r,0,sizeof(r));put32(r+0xb0,SFN_VENDOR_WIFI_MARKER);profile(r,0,"Same","password1");profile(r,1,"Same","password1");if(sfn_vendor_wifi_parse(&s,r,sizeof(r))!=1)f++;
 memset(r,0,sizeof(r));put32(r+0xb0,SFN_VENDOR_WIFI_MARKER);profile(r,0,"Open","");if(sfn_vendor_wifi_parse(&s,r,sizeof(r))!=1||s.profiles[0].password[0])f++;
 memset(r,0,sizeof(r));put32(r+0xb0,SFN_VENDOR_WIFI_MARKER);profile(r,0,"Bad","short");if(sfn_vendor_wifi_parse(&s,r,sizeof(r))!=0)f++;
 memset(r,0,sizeof(r));put32(r+0xb0,SFN_VENDOR_WIFI_MARKER);profile(r,1,"Second","password2");if(sfn_vendor_wifi_parse(&s,r,sizeof(r))!=1||strcmp(s.profiles[0].ssid,"Second"))f++;
 puts(f?"SFN_VENDOR_WIFI_TESTS=FAIL":"SFN_VENDOR_WIFI_TESTS=PASS");return f?1:0;}
