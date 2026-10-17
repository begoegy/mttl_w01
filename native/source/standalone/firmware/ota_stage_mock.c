#include "ota_stage.h"
#include "http_framing.h"
#include <string.h>
extern unsigned mttl_test_count,mttl_test_failed,mttl_failed_ids[64];
static void check(int ok){++mttl_test_count;if(!ok&&mttl_test_failed<64)mttl_failed_ids[mttl_test_failed++]=mttl_test_count;}
#define CAP 0x70000u
static uint8_t image[CAP],storage[CAP];
typedef struct {uint32_t base,end;unsigned calls,cut;int partial,corrupt,bad;} fake;
static fake f;
static int access_range(uint32_t a,size_t n){
 if(a<f.base||a>=f.end||n>f.end-a){f.bad=1;return 0;}
 return ++f.calls!=f.cut;
}
static int erase(void *p,uint32_t a){(void)p;if((a&4095)||!access_range(a,4096))return 0;memset(storage+a-f.base,255,4096);return 1;}
static int write(void *p,uint32_t a,const uint8_t *d,size_t n){size_t k;(void)p;
 if(!n||n>256||!access_range(a,n))return 0;
 for(k=0;k<n;k++)if((storage[a-f.base+k]&d[k])!=d[k])return 0;
 if(f.partial){memcpy(storage+a-f.base,d,n/2);return 0;}
 memcpy(storage+a-f.base,d,n);return 1;
}
static int read(void *p,uint32_t a,uint8_t *d,size_t n){(void)p;if(!access_range(a,n))return 0;memcpy(d,storage+a-f.base,n);if(f.corrupt&&n)d[n-1]^=1;return 1;}
static void put(uint8_t *p,uint32_t v){unsigned k;for(k=0;k<4;k++)p[k]=(uint8_t)(v>>(8*k));}
static void make(uint32_t n,uint8_t digest[32]){
 static const uint8_t magic[20]={0x52,0x54,0x4b,0x57,0x69,0x6e,0,0xff,0,1,1,0,0x95,0x81,1,1,0,0,0,0};
 uint32_t k,sum=0;mttl_ota_hash h;
 for(k=0;k<n;k++)image[k]=(uint8_t)(k*37u);
 put(image,n-20);put(image+4,0x10006000);memset(image+8,255,8);put(image+16,0x10006019);memcpy(image+20,magic,20);
 for(k=0;k<n-4;k++)sum+=image[k];put(image+n-4,sum);
 mttl_ota_hash_init(&h);mttl_ota_hash_update(&h,image,n);mttl_ota_hash_finish(&h,digest);
}
static int run(mttl_ota_stage *s,const mttl_ota_layout *l,uint32_t active,uint32_t n,size_t chunk,const uint8_t hash[32]){
 mttl_ota_stage_io io={0,erase,write,read};uint32_t pos=0;
 if(!mttl_ota_stage_begin(s,l,active,n,hash,&io))return 0;
 while(pos<n){size_t take=n-pos;if(take>chunk)take=chunk;if(!mttl_ota_stage_feed(s,image+pos,take))return 0;pos+=(uint32_t)take;}
 return mttl_ota_stage_finish(s);
}
void mttl_ota_stage_test(void){
 mttl_ota_layout l={0xb000,0x80000,0xf0000,0x100000};mttl_ota_stage s;uint8_t hash[32];unsigned a,b,cut,total;
 const uint32_t sizes[]={46,4095,4096,4097,8193,65536,0x6a014};const size_t chunks[]={1,255,256,257,4096};
 for(a=0;a<sizeof(sizes)/sizeof(sizes[0]);a++){uint32_t n=sizes[a];make(n,hash);
 for(b=0;b<2;b++){memset(&f,0,sizeof(f));f.base=b?l.first:l.second;f.end=f.base+((n+4095)&~4095u);
 check(run(&s,&l,b?l.second:l.first,n,n>8193?4096:chunks[a%5],hash));
 check(s.state==MTTL_STAGE_VERIFIED&&!f.bad&&!memcmp(image,storage,n));check(!mttl_ota_stage_finish(&s));}}
 make(8193,hash);memset(&f,0,sizeof(f));f.base=l.second;f.end=f.base+12288;
 check(run(&s,&l,l.first,8193,257,hash));total=f.calls;
 /* Cut every erase/write/read callback separately, including last readback. */
 for(cut=1;cut<=total;cut++){memset(&f,0,sizeof(f));f.base=l.second;f.end=f.base+12288;f.cut=cut;
 check(!run(&s,&l,l.first,8193,257,hash));check(s.state==MTTL_STAGE_FAILED&&!f.bad);check(!mttl_ota_stage_feed(&s,image,1));}
 memset(&f,0,sizeof(f));f.base=l.second;f.end=f.base+12288;f.partial=1;
 check(!run(&s,&l,l.first,8193,4096,hash)&&s.state==MTTL_STAGE_FAILED);
 memset(&f,0,sizeof(f));f.base=l.second;f.end=f.base+12288;image[100]^=1;
 check(!run(&s,&l,l.first,8193,4096,hash)&&s.state==MTTL_STAGE_FAILED);image[100]^=1;
 memset(&f,0,sizeof(f));f.base=l.second;f.end=f.base+12288;f.corrupt=1;
 check(!run(&s,&l,l.first,8193,4096,hash)&&s.state==MTTL_STAGE_FAILED);
 {uint32_t length;unsigned k;static const char *bad[]={
 "POST / HTTP/1.1\r\nContent-Length: -1\r\n\r\n",
 "POST / HTTP/1.1\r\nContent-Length: +1\r\n\r\n",
 "POST / HTTP/1.1\r\nContent-Length: 1x\r\n\r\n",
 "POST / HTTP/1.1\r\nContent-Length: 2147483648\r\n\r\n",
 "POST / HTTP/1.1\r\nContent-Length: 999999999999999999999999\r\n\r\n",
 "POST / HTTP/1.1\r\nContent-Length: \r\n\r\n",
 "POST / HTTP/1.1\r\nContent-Length: 1\r\ncontent-length: 1\r\n\r\n",
 "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n",
 "POST / HTTP/1.1\r\nContent-Length : 1\r\n\r\n",
 "POST / HTTP/1.1\r\n Content-Length: 1\r\n\r\n",
 "POST / HTTP/1.1\r\nContent-Length: 1\r\n",
 "POST / HTTP/1.1\r\nContent-Length: 1\r\n\r\nX"};
 const char *good="POST / HTTP/1.1\r\ncOnTeNt-LeNgTh:\t4097 \t\r\n\r\n";
 check(mttl_http_framing((const uint8_t *)good,strlen(good),&length)&&length==4097);
 good="GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n";check(mttl_http_framing((const uint8_t *)good,strlen(good),&length)&&length==0);
 good="POST / HTTP/1.1\r\nContent-Length: 0\r\n\r\n";check(mttl_http_framing((const uint8_t *)good,strlen(good),&length)&&length==0);
 good="POST / HTTP/1.1\r\nContent-Length: 2147483647\r\n\r\n";check(mttl_http_framing((const uint8_t *)good,strlen(good),&length)&&length==2147483647u);
 for(k=0;k<sizeof(bad)/sizeof(bad[0]);k++)check(!mttl_http_framing((const uint8_t *)bad[k],strlen(bad[k]),&length));
 for(k=0;k<strlen(good);k++)check(!mttl_http_framing((const uint8_t *)good,k,&length));
 {const uint8_t *value;size_t n;
 good="GET / HTTP/1.1\r\nX-Origin: fake\r\noRiGiN:\t http://192.168.4.1 \t\r\n\r\n";
 check(mttl_http_field((const uint8_t *)good,strlen(good),"origin",&value,&n)&&n==18&&!memcmp(value,"http://192.168.4.1",18));
 check(mttl_http_field((const uint8_t *)good,strlen(good),"Origin",&value,&n)&&n==18);
 good="GET / HTTP/1.1\r\nX-Origin: fake\r\n\r\n";check(!mttl_http_field((const uint8_t *)good,strlen(good),"origin",&value,&n)&&!value&&!n);
 good="GET / HTTP/1.1\r\nOrigin: one\r\norigin: two\r\n\r\n";check(!mttl_http_field((const uint8_t *)good,strlen(good),"origin",&value,&n)&&!value&&!n);
 }
 }
}
