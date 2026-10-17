#include "ota_session.h"
#include <string.h>
extern unsigned mttl_test_count,mttl_test_failed,mttl_failed_ids[64];
static void check(int ok){++mttl_test_count;if(!ok&&mttl_test_failed<64)mttl_failed_ids[mttl_test_failed++]=mttl_test_count;}
static uint8_t image[4097],memory[8192];
typedef struct {unsigned calls,cut,admissions;int deny,bad;} mock;
static mock f;
static int range(uint32_t a,size_t n){if(a<0x80000||a>=0x82000||n>0x82000-a){f.bad=1;return 0;}return ++f.calls!=f.cut;}
static int erase(void *p,uint32_t a){(void)p;if((a&4095)||!range(a,4096))return 0;memset(memory+a-0x80000,255,4096);return 1;}
static int write(void *p,uint32_t a,const uint8_t *d,size_t n){(void)p;if(n>256||!range(a,n))return 0;memcpy(memory+a-0x80000,d,n);return 1;}
static int read(void *p,uint32_t a,uint8_t *d,size_t n){(void)p;if(!range(a,n))return 0;memcpy(d,memory+a-0x80000,n);return 1;}
static int admit(void *p,const mttl_ota_layout *l,uint32_t active,const mttl_ota_plan *plan,uint32_t n,const uint8_t sha[32]){
 mock *m=p;(void)sha;m->admissions++;return !m->deny&&l->first==0xb000&&l->second==0x80000&&active==0xb000&&plan->target==0x80000&&plan->erase_end==0x82000&&n==4097;
}
static void put(uint8_t *p,uint32_t v){unsigned k;for(k=0;k<4;k++)p[k]=(uint8_t)(v>>(8*k));}
static int init(mttl_ota_session *s,const uint8_t sha[32]){
 mttl_ota_layout l={0xb000,0x80000,0xf0000,0x100000};mttl_ota_stage_io io={0,erase,write,read};
 return mttl_ota_session_init(s,&l,l.first,sizeof(image),sha,&io,admit,&f);
}
static int transfer(mttl_ota_session *s,const uint8_t sha[32],size_t chunk){size_t pos=0;if(!init(s,sha))return 0;
 while(pos<sizeof(image)){size_t n=sizeof(image)-pos;if(n>chunk)n=chunk;if(!mttl_ota_session_feed(s,image+pos,n))return 0;pos+=n;}
 return mttl_ota_session_finish(s);
}
void mttl_ota_session_test(void){mttl_ota_session s;mttl_ota_hash h;uint8_t sha[32];uint32_t sum=0;unsigned k,total;
 const size_t chunks[]={1,7,39,40,41,255,4097};const unsigned corrupt[]={0,4,8,16,20};
 static const uint8_t magic[20]={0x52,0x54,0x4b,0x57,0x69,0x6e,0,0xff,0,1,1,0,0x95,0x81,1,1,0,0,0,0};
 for(k=0;k<sizeof(image);k++)image[k]=(uint8_t)(k*23);
 put(image,sizeof(image)-20);put(image+4,0x10006000);memset(image+8,255,8);put(image+16,0x10006019);memcpy(image+20,magic,20);
 for(k=0;k<sizeof(image)-4;k++)sum+=image[k];put(image+sizeof(image)-4,sum);
 mttl_ota_hash_init(&h);mttl_ota_hash_update(&h,image,sizeof(image));mttl_ota_hash_finish(&h,sha);
 for(k=0;k<sizeof(chunks)/sizeof(chunks[0]);k++){memset(&f,0,sizeof(f));check(transfer(&s,sha,chunks[k])&&s.state==MTTL_SESSION_VERIFIED&&f.admissions==1&&!f.bad);check(!memcmp(image,memory,sizeof(image)));check(!mttl_ota_session_finish(&s)&&!mttl_ota_session_feed(&s,image,1));}
 memset(&f,0,sizeof(f));check(transfer(&s,sha,4097));total=f.calls;
 for(k=1;k<=total;k++){memset(&f,0,sizeof(f));f.cut=k;check(!transfer(&s,sha,4097)&&s.state==MTTL_SESSION_FAILED&&!f.bad);}
 for(k=0;k<sizeof(corrupt)/sizeof(corrupt[0]);k++){memset(&f,0,sizeof(f));image[corrupt[k]]^=1;check(!transfer(&s,sha,4097)&&!f.calls&&!f.admissions);image[corrupt[k]]^=1;}
 memset(&f,0,sizeof(f));f.deny=1;check(!transfer(&s,sha,4097)&&!f.calls&&f.admissions==1);
 memset(&f,0,sizeof(f));check(init(&s,sha));check(mttl_ota_session_feed(&s,image,39)&&!f.calls&&!f.admissions);check(!mttl_ota_session_finish(&s)&&s.state==MTTL_SESSION_FAILED);check(!mttl_ota_session_feed(&s,image,40));
 memset(&f,0,sizeof(f));check(init(&s,sha));check(!mttl_ota_session_feed(&s,image,4098)&&!f.calls);
 memset(&f,0,sizeof(f));check(init(&s,sha));check(mttl_ota_session_feed(&s,image,40)&&f.admissions==1);mttl_ota_session_abort(&s);check(!mttl_ota_session_feed(&s,image+40,1));
 memset(&f,0,sizeof(f));sha[0]^=1;check(!transfer(&s,sha,4097)&&s.state==MTTL_SESSION_FAILED);
}
