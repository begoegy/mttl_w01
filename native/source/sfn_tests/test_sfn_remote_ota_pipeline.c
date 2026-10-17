#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../standalone/firmware/sfn_ota_http.h"
#include "../standalone/firmware/sfn_update_guard.h"
#include "../standalone/firmware/ota_session.h"
#include "../standalone/firmware/ota_hash.h"
#include "../standalone/firmware/ota_guard.h"

#define FLASH_SIZE 0x100000u
#define PREVIOUS 0x0000b000u
#define CANDIDATE 0x00080000u
#define PROTECTED 0x000ee000u
static uint8_t flashmem[FLASH_SIZE];
static uint8_t baseline[FLASH_SIZE];
static uint8_t staged_snapshot[FLASH_SIZE];
static const uint8_t marker15[32]="SFN-NATIVE-GUARD-2.0.15-A1";
static const uint8_t marker16[32]="SFN-NATIVE-GUARD-2.0.16-A1";
static uint8_t dummy_sector[4096],dummy_sha[32];

typedef struct { mttl_ota_plan plan; int admitted; } stage_mem;
static int raw_read(void *p,uint32_t a,uint8_t *b,size_t n){(void)p;if(!b||a>FLASH_SIZE||n>FLASH_SIZE-a)return 0;memcpy(b,flashmem+a,n);return 1;}
static int raw_erase(void *p,uint32_t a){(void)p;if((a&4095u)||a>FLASH_SIZE-4096u)return 0;memset(flashmem+a,255,4096);return 1;}
static int raw_write(void *p,uint32_t a,const uint8_t *b,size_t n){size_t i;(void)p;if(!b||a>FLASH_SIZE||n>FLASH_SIZE-a)return 0;for(i=0;i<n;i++)if((flashmem[a+i]&b[i])!=b[i])return 0;for(i=0;i<n;i++)flashmem[a+i]&=b[i];return 1;}
static sfn_boot_guard_io guard_io={0,raw_read,raw_erase,raw_write};
static int st_read(void *p,uint32_t a,uint8_t *b,size_t n){stage_mem *m=p;if(!m->admitted||a<m->plan.target||a>=m->plan.erase_end||n>m->plan.erase_end-a)return 0;return raw_read(0,a,b,n);}
static int st_write(void *p,uint32_t a,const uint8_t *b,size_t n){stage_mem *m=p;if(!m->admitted||a<m->plan.target||a>=m->plan.erase_end||n>m->plan.erase_end-a)return 0;return raw_write(0,a,b,n);}
static int st_erase(void *p,uint32_t a){stage_mem *m=p;if(!m->admitted||a<m->plan.target||a>=m->plan.erase_end)return 0;return raw_erase(0,a);}
static int admit(void *p,const mttl_ota_layout *l,uint32_t active,const mttl_ota_plan *plan,uint32_t bytes,const uint8_t sha[32]){stage_mem *m=p;(void)sha;if(!l||active!=PREVIOUS||bytes<44u||plan->target!=CANDIDATE)return 0;m->plan=*plan;m->admitted=1;return 1;}
static uint8_t *load(const char *path,size_t *n){FILE *f=fopen(path,"rb");long z;uint8_t *p;if(!f)return 0;if(fseek(f,0,SEEK_END)||((z=ftell(f))<0)||fseek(f,0,SEEK_SET)){fclose(f);return 0;}p=malloc((size_t)z);if(!p){fclose(f);return 0;}if(fread(p,1,(size_t)z,f)!=(size_t)z){free(p);fclose(f);return 0;}fclose(f);*n=(size_t)z;return p;}
static uint32_t find_marker(const uint8_t *p,size_t n,const uint8_t marker[32]){size_t i;for(i=0;i+32<=n;i++)if(!memcmp(p+i,marker,32))return (uint32_t)i;return 0;}
static int hex32(const char *s,uint8_t out[32]){unsigned i;if(!s||strlen(s)!=64)return 0;for(i=0;i<32;i++){char a=s[i*2],b=s[i*2+1];unsigned x=(a>='0'&&a<='9')?a-'0':(a>='a'&&a<='f')?a-'a'+10:99,y=(b>='0'&&b<='9')?b-'0':(b>='a'&&b<='f')?b-'a'+10:99;if(x>15||y>15)return 0;out[i]=(uint8_t)((x<<4)|y);}return 1;}
static void file_sha(const uint8_t *p,size_t n,uint8_t out[32]){mttl_ota_hash h;mttl_ota_hash_init(&h);assert(mttl_ota_hash_update(&h,p,n));assert(mttl_ota_hash_finish(&h,out));}
static void activate_steps(unsigned count){mttl_ota_layout l={PREVIOUS,CANDIDATE,PROTECTED,FLASH_SIZE};mttl_ota_commit c;uint32_t a,v,r;unsigned i;assert(mttl_ota_commit_init(&c,&l,PREVIOUS,CANDIDATE,1));for(i=0;i<count;i++){assert(mttl_ota_commit_action(&c,&a,&v));assert(raw_read(0,a,(uint8_t *)&r,4));assert((r&v)==v);assert(raw_write(0,a,(uint8_t *)&v,4));assert(raw_read(0,a,(uint8_t *)&r,4));assert(mttl_ota_commit_ack(&c,1,r));}}
static int stage_fwr(const uint8_t *fwr,size_t bytes,size_t limit){mttl_ota_layout l={PREVIOUS,CANDIDATE,PROTECTED,FLASH_SIZE};mttl_ota_session session;stage_mem mem; mttl_ota_stage_io io; sfn_fwr_decoder dec;uint8_t zero[32]={0},buf[512];size_t pos=0;memset(&mem,0,sizeof(mem));io.context=&mem;io.erase=st_erase;io.write=st_write;io.read=st_read;assert(mttl_ota_session_init(&session,&l,PREVIOUS,(uint32_t)bytes,zero,&io,admit,&mem));sfn_fwr_decoder_init(&dec);if(limit>bytes)limit=bytes;while(pos<limit){size_t n=limit-pos;if(n>sizeof(buf))n=sizeof(buf);memcpy(buf,fwr+pos,n);assert(sfn_fwr_decode(&dec,buf,n));assert(mttl_ota_session_feed(&session,buf,n));pos+=n;}if(limit!=bytes){mttl_ota_session_abort(&session);return 0;}return mttl_ota_session_finish(&session);}
int main(int argc,char **argv){uint8_t *prev,*cand_fwr,*cand_bin,expected[32],actual[32];size_t prev_n,fwr_n,bin_n;uint32_t marker_off; sfn_boot_guard_descriptor d15,d16; sfn_update_guard_state st; unsigned i;static const size_t cuts[]={1,15,16,40,512,4096,65536};
 if(argc!=5){fprintf(stderr,"usage: %s previous.bin candidate.fwr candidate.bin candidate_fwr_sha256\n",argv[0]);return 2;}
 prev=load(argv[1],&prev_n);cand_fwr=load(argv[2],&fwr_n);cand_bin=load(argv[3],&bin_n);assert(prev&&cand_fwr&&cand_bin&&fwr_n==bin_n&&prev_n<0x70000u&&fwr_n<0x70000u&&hex32(argv[4],expected));file_sha(cand_fwr,fwr_n,actual);assert(!memcmp(actual,expected,32));marker_off=find_marker(prev,prev_n,marker15);assert(marker_off&&find_marker(cand_bin,bin_n,marker16)==marker_off);assert(memmem(prev,prev_n,"SFN_NATIVE_REMOTE_OTA_SAFE;SEQ=20015",36));assert(memmem(cand_bin,bin_n,"SFN_NATIVE_REMOTE_OTA_SAFE;SEQ=20016",36));assert(!memmem(cand_bin,bin_n,"SFN_NATIVE_REMOTE_OTA_SAFE;SEQ=20015",36));
 memset(&d15,0,sizeof(d15));d15.first=PREVIOUS;d15.second=CANDIDATE;d15.guard_sector=0xed000u;d15.flash_size=FLASH_SIZE;d15.rescue_bytes=8192;d15.marker_offset=marker_off;d15.marker=marker15;d15.marker_size=32;d15.rescue_sector0=dummy_sector;d15.rescue_sha256=dummy_sha;d15.rescue_tail_sha256=dummy_sha;d16=d15;d16.marker=marker16;
 memset(flashmem,255,sizeof(flashmem));memcpy(flashmem+PREVIOUS,prev,prev_n);memcpy(flashmem+PREVIOUS+8,"81958711",8);memcpy(baseline,flashmem,sizeof(flashmem));
 assert(sfn_update_guard_prepare(&d15,&guard_io,PREVIOUS,CANDIDATE));assert(stage_fwr(cand_fwr,fwr_n,fwr_n));assert(!memcmp(flashmem+CANDIDATE,cand_bin,bin_n));memcpy(staged_snapshot,flashmem,sizeof(flashmem));
 /* Incomplete downloads never deactivate the known-good previous image. */
 for(i=0;i<sizeof(cuts)/sizeof(cuts[0]);i++){size_t cut=cuts[i];memcpy(flashmem,baseline,sizeof(flashmem));assert(sfn_update_guard_prepare(&d15,&guard_io,PREVIOUS,CANDIDATE));assert(!stage_fwr(cand_fwr,fwr_n,cut));assert(!memcmp(flashmem+PREVIOUS+8,"81958711",8));assert(sfn_update_guard_start(&st,&d15,&guard_io,PREVIOUS,1)==SFN_UPDATE_GUARD_TRUSTED);}
 memcpy(flashmem,baseline,sizeof(flashmem));assert(sfn_update_guard_prepare(&d15,&guard_io,PREVIOUS,CANDIDATE));assert(!stage_fwr(cand_fwr,fwr_n,fwr_n-1));assert(!memcmp(flashmem+PREVIOUS+8,"81958711",8));
 /* Activation cut 0/1 => previous boots safely. */
 for(i=0;i<=1;i++){memcpy(flashmem,staged_snapshot,sizeof(flashmem));activate_steps(i);assert(sfn_update_guard_start(&st,&d15,&guard_io,PREVIOUS,2)==SFN_UPDATE_GUARD_TRUSTED);}
 /* After target signature is complete but before old slot is demoted, either boot choice is safe. */
 memcpy(flashmem,staged_snapshot,sizeof(flashmem));activate_steps(2);{uint8_t both[FLASH_SIZE];memcpy(both,flashmem,sizeof(both));assert(sfn_update_guard_start(&st,&d15,&guard_io,PREVIOUS,3)==SFN_UPDATE_GUARD_TRUSTED);memcpy(flashmem,both,sizeof(flashmem));assert(sfn_update_guard_start(&st,&d16,&guard_io,CANDIDATE,3)==SFN_UPDATE_GUARD_ARMED);}
 /* Normal activation enters candidate trial; third failed boot requests rollback. */
 memcpy(flashmem,staged_snapshot,sizeof(flashmem));activate_steps(3);assert(sfn_update_guard_start(&st,&d16,&guard_io,CANDIDATE,4)==SFN_UPDATE_GUARD_ARMED);assert(sfn_update_guard_start(&st,&d16,&guard_io,CANDIDATE,5)==SFN_UPDATE_GUARD_ARMED);assert(sfn_update_guard_start(&st,&d16,&guard_io,CANDIDATE,6)==SFN_UPDATE_GUARD_ROLLBACK_READY);assert(sfn_update_guard_rollback(&st,&d16,&guard_io));assert(!memcmp(flashmem+PREVIOUS,baseline+PREVIOUS,prev_n));assert(!memcmp(flashmem+PREVIOUS+8,"81958711",8));assert(!memcmp(flashmem+CANDIDATE+8,"01958711",8));assert(sfn_update_guard_start(&st,&d15,&guard_io,PREVIOUS,7)==SFN_UPDATE_GUARD_TRUSTED);
 puts("SFN_REMOTE_OTA_PIPELINE=PASS partial_download_cuts=8 activation_cut_states=4 rollback=PASS");free(prev);free(cand_fwr);free(cand_bin);return 0;
}
