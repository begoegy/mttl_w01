#include "sdk_flash_mock.h"
#include "sdk_ota_flash.h"
#include "ota_stage.h"
#include <string.h>
flash_t flash;
static uint8_t memory[8192];
static unsigned calls,cut,locked,bad,erase_bad,write_bad;
extern unsigned mttl_test_count,mttl_test_failed,mttl_failed_ids[64];
static void check(int ok){++mttl_test_count;if(!ok&&mttl_test_failed<64)mttl_failed_ids[mttl_test_failed++]=mttl_test_count;}
void device_mutex_lock(int id){if(id!=1||locked)bad=1;locked=1;}
void device_mutex_unlock(int id){if(id!=1||!locked)bad=1;locked=0;}
static int range(uint32_t a,uint32_t n){if(!locked||a<0x80000||a>=0x82000||n>0x82000-a){bad=1;return 0;}return ++calls!=cut;}
int flash_stream_read(flash_t *p,uint32_t a,uint32_t n,uint8_t *d){(void)p;if(!range(a,n))return 0;memcpy(d,memory+a-0x80000,n);return 1;}
int flash_stream_write(flash_t *p,uint32_t a,uint32_t n,uint8_t *d){unsigned k;(void)p;if(!range(a,n))return -1;for(k=0;k<n;k++)memory[a-0x80000+k]&=d[k];if(write_bad&&n)memory[a-0x80000]^=1;return 0;}
void flash_erase_sector(flash_t *p,uint32_t a){(void)p;if(!range(a,4096))return;memset(memory+a-0x80000,255,4096);if(erase_bad)memory[a-0x80000+4095]=0;}
static void reset(mttl_sdk_ota_flash *p){memset(memory,0,sizeof(memory));calls=cut=locked=bad=erase_bad=write_bad=0;mttl_sdk_ota_flash_init(p);p->plan.target=0x80000;p->plan.erase_end=0x82000;p->verified=1;}
static int stage_erase(void *p,uint32_t a){return mttl_sdk_ota_flash_erase(p,a);}
static int stage_write(void *p,uint32_t a,const uint8_t *d,size_t n){return mttl_sdk_ota_flash_write(p,a,d,n);}
static int stage_read(void *p,uint32_t a,uint8_t *d,size_t n){return mttl_sdk_ota_flash_read(p,a,d,n);}
static uint8_t upload[4097];
static void put(uint8_t *p,uint32_t v){unsigned k;for(k=0;k<4;k++)p[k]=(uint8_t)(v>>(8*k));}
static int transfer(mttl_sdk_ota_flash *p,mttl_ota_stage *s,const uint8_t expected[32]){
 mttl_ota_layout l={0xb000,0x80000,0xf0000,0x100000};mttl_ota_stage_io io={p,stage_erase,stage_write,stage_read};size_t pos=0;
 if(!mttl_ota_stage_begin(s,&l,0xb000,sizeof(upload),expected,&io))return 0;
 while(pos<sizeof(upload)){size_t n=sizeof(upload)-pos;if(n>257)n=257;if(!mttl_ota_stage_feed(s,upload+pos,n))return 0;pos+=n;}
 return mttl_ota_stage_finish(s);
}
void mttl_sdk_flash_test(void){mttl_sdk_ota_flash p;uint8_t d[256],out[256];unsigned k;
 reset(&p);p.verified=0;check(!mttl_sdk_ota_flash_erase(&p,0x80000)&&!calls);
 reset(&p);check(!mttl_sdk_ota_flash_erase(&p,0x80001)&&!calls);check(!mttl_sdk_ota_flash_erase(&p,0x7f000)&&!calls);check(!mttl_sdk_ota_flash_erase(&p,0x82000)&&!calls);
 check(mttl_sdk_ota_flash_erase(&p,0x80000)&&calls==17&&!bad&&!locked);
 memset(d,0xa5,sizeof(d));check(mttl_sdk_ota_flash_write(&p,0x80000,d,256)&&!locked&&!bad);check(mttl_sdk_ota_flash_read(&p,0x80000,out,256)&&!memcmp(d,out,256));
 memset(d,255,sizeof(d));check(!mttl_sdk_ota_flash_write(&p,0x80000,d,256)&&!p.verified&&!locked);
 for(k=1;k<=17;k++){reset(&p);cut=k;check(!mttl_sdk_ota_flash_erase(&p,0x80000)&&!p.verified&&!locked&&!bad);}
 reset(&p);erase_bad=1;check(!mttl_sdk_ota_flash_erase(&p,0x80000)&&!p.verified&&!locked);
 for(k=1;k<=3;k++){reset(&p);memset(memory,255,sizeof(memory));memset(d,0xa5,sizeof(d));cut=k;check(!mttl_sdk_ota_flash_write(&p,0x80000,d,256)&&!p.verified&&!locked&&!bad);}
 reset(&p);memset(memory,255,sizeof(memory));write_bad=1;check(!mttl_sdk_ota_flash_write(&p,0x80000,d,256)&&!p.verified&&!locked);
 reset(&p);check(!mttl_sdk_ota_flash_write(&p,0x81fff,d,2)&&!calls);check(!mttl_sdk_ota_flash_write(&p,0x80000,d,257)&&!calls);check(!mttl_sdk_ota_flash_read(&p,0x80000,out,0)&&!calls);
 cut=1;check(!mttl_sdk_ota_flash_read(&p,0x80000,out,256)&&!p.verified&&!locked);
 /* Full staging uses the SDK adapter, with fake NOR backing only. */
 {mttl_ota_stage s;mttl_ota_hash h;uint8_t expected[32];unsigned total;uint32_t sum=0;
 static const uint8_t magic[20]={0x52,0x54,0x4b,0x57,0x69,0x6e,0,0xff,0,1,1,0,0x95,0x81,1,1,0,0,0,0};
 for(k=0;k<sizeof(upload);k++)upload[k]=(uint8_t)(k*19);
 put(upload,sizeof(upload)-20);put(upload+4,0x10006000);memset(upload+8,255,8);put(upload+16,0x10006019);memcpy(upload+20,magic,20);
 for(k=0;k<sizeof(upload)-4;k++)sum+=upload[k];put(upload+sizeof(upload)-4,sum);
 mttl_ota_hash_init(&h);mttl_ota_hash_update(&h,upload,sizeof(upload));mttl_ota_hash_finish(&h,expected);
 reset(&p);check(transfer(&p,&s,expected)&&s.state==MTTL_STAGE_VERIFIED&&!locked&&!bad);
 check(!memcmp(memory,upload,sizeof(upload)));total=calls;
 for(k=1;k<=total;k++){reset(&p);cut=k;check(!transfer(&p,&s,expected)&&s.state==MTTL_STAGE_FAILED&&!locked&&!bad);}
 reset(&p);p.verified=0;check(!transfer(&p,&s,expected)&&!calls);
 reset(&p);write_bad=1;check(!transfer(&p,&s,expected)&&!p.verified&&!locked);
 reset(&p);expected[0]^=1;check(!transfer(&p,&s,expected)&&s.state==MTTL_STAGE_FAILED&&!locked);
 }
}
