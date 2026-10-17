#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_update_guard.h"
#include "../standalone/firmware/ota_hash.h"
#define FLASH_SIZE 0x100000u
#define PREVIOUS 0xb000u
#define CANDIDATE 0x80000u
static uint8_t flashmem[FLASH_SIZE];
static int writes,erases,fail_write;
static const uint8_t marker[32]="SFN-NATIVE-GUARD-2.0.15-A1";
static int rd(void *p,uint32_t a,uint8_t *b,size_t n){(void)p;if(a>FLASH_SIZE||n>FLASH_SIZE-a)return 0;memcpy(b,flashmem+a,n);return 1;}
static int er(void *p,uint32_t a){(void)p;if((a&4095u)||a>FLASH_SIZE-4096)return 0;++erases;memset(flashmem+a,255,4096);return 1;}
static int wr(void *p,uint32_t a,const uint8_t *b,size_t n){size_t i;(void)p;if(a>FLASH_SIZE||n>FLASH_SIZE-a)return 0;++writes;if(fail_write&&writes==fail_write)return 0;for(i=0;i<n;i++)if((flashmem[a+i]&b[i])!=b[i])return 0;for(i=0;i<n;i++)flashmem[a+i]&=b[i];return 1;}
static sfn_boot_guard_io io={0,rd,er,wr};
static uint8_t rescue0[4096],sha[32],tail[32];
static sfn_boot_guard_descriptor desc={PREVIOUS,CANDIDATE,0xed000,0x100000,8192,0x200,marker,sizeof(marker),rescue0,sha,tail};
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static void image(uint32_t slot,const char *signature,int current_marker){static const uint8_t magic[20]={0x52,0x54,0x4b,0x57,0x69,0x6e,0,0xff,0,1,1,0,0x95,0x81,1,1,0,0,0,0};uint32_t bytes=12288,body=bytes-20,sum=0,i;uint8_t *p=flashmem+slot;memset(p,0xa5,bytes);put32(p,body);put32(p+4,0x10006000);memcpy(p+8,signature,8);put32(p+16,0x10006101);memcpy(p+20,magic,20);if(current_marker)memcpy(p+0x200,marker,sizeof(marker));for(i=0;i<bytes-4;i++)sum+=(i>=8&&i<16)?255u:p[i];put32(p+bytes-4,sum);}
static void clean(void){memset(flashmem,255,sizeof(flashmem));writes=erases=fail_write=0;image(PREVIOUS,"81958711",1);image(CANDIDATE,"01958711",0);}
static int sig(uint32_t slot,const char *x){return !memcmp(flashmem+slot+8,x,8);}
static void activate_candidate(void){memcpy(flashmem+CANDIDATE+8,"81958711",8);memcpy(flashmem+PREVIOUS+8,"01958711",8);}
int main(void){sfn_update_guard_state s,t;static uint8_t prepared[FLASH_SIZE],rollback_ready[FLASH_SIZE];uint32_t current;unsigned cut;
 clean();assert(sfn_update_guard_locate_current(&desc,&io,&current)&&current==PREVIOUS);
 /* Interrupted preparation never touches the running image or activates target. */
 for(cut=1;cut<=17;cut++){int r;clean();fail_write=(int)cut;writes=0;assert(!sfn_update_guard_prepare(&desc,&io,PREVIOUS,CANDIDATE));fail_write=0;assert(sig(PREVIOUS,"81958711")&&sig(CANDIDATE,"01958711"));r=sfn_update_guard_start(&s,&desc,&io,PREVIOUS,1);assert(r==SFN_UPDATE_GUARD_NONE||r==SFN_UPDATE_GUARD_TRUSTED);}
 clean();writes=0;assert(sfn_update_guard_prepare(&desc,&io,PREVIOUS,CANDIDATE));assert(writes==17);memcpy(prepared,flashmem,sizeof(flashmem));
 assert(sfn_update_guard_start(&s,&desc,&io,PREVIOUS,10)==SFN_UPDATE_GUARD_TRUSTED);assert(sig(CANDIDATE,"01958711"));
 /* Candidate activation enters a bounded trial and can become trusted. */
 memcpy(flashmem,prepared,sizeof(flashmem));activate_candidate();assert(sfn_update_guard_start(&s,&desc,&io,CANDIDATE,20)==SFN_UPDATE_GUARD_ARMED);assert(!sfn_update_guard_should_timeout(&s,900019,900000));assert(sfn_update_guard_should_timeout(&s,900020,900000));assert(sfn_update_guard_mark_trusted(&s,&desc,&io));assert(sfn_update_guard_start(&t,&desc,&io,CANDIDATE,30)==SFN_UPDATE_GUARD_TRUSTED);
 /* Fresh untrusted candidate reaches rollback-ready after two trial boots. */
 memcpy(flashmem,prepared,sizeof(flashmem));activate_candidate();assert(sfn_update_guard_start(&s,&desc,&io,CANDIDATE,1)==SFN_UPDATE_GUARD_ARMED);assert(sfn_update_guard_start(&s,&desc,&io,CANDIDATE,2)==SFN_UPDATE_GUARD_ARMED);assert(sfn_update_guard_start(&s,&desc,&io,CANDIDATE,3)==SFN_UPDATE_GUARD_ROLLBACK_READY);memcpy(rollback_ready,flashmem,sizeof(flashmem));
 /* Every write interruption during rollback remains retryable from candidate. */
 for(cut=1;cut<=17;cut++){memcpy(flashmem,rollback_ready,sizeof(flashmem));fail_write=(int)cut;writes=0;assert(!sfn_update_guard_rollback(&s,&desc,&io));fail_write=0;writes=0;assert(sfn_update_guard_start(&t,&desc,&io,CANDIDATE,4)==SFN_UPDATE_GUARD_ROLLBACK_READY);assert(sfn_update_guard_rollback(&t,&desc,&io));assert(sig(PREVIOUS,"81958711")&&sig(CANDIDATE,"01958711"));assert(sfn_update_guard_start(&t,&desc,&io,PREVIOUS,5)==SFN_UPDATE_GUARD_TRUSTED);}
 /* A torn/partly erased previous first sector is reconstructed from backup. */
 memcpy(flashmem,rollback_ready,sizeof(flashmem));memset(flashmem+PREVIOUS,0xff,1733);assert(sfn_update_guard_start(&s,&desc,&io,CANDIDATE,6)==SFN_UPDATE_GUARD_ROLLBACK_READY);writes=0;assert(sfn_update_guard_rollback(&s,&desc,&io));assert(writes==17);assert(sig(PREVIOUS,"81958711")&&sig(CANDIDATE,"01958711"));
 puts("SFN_UPDATE_GUARD_POWER_CUT_TESTS=PASS prepare_cuts=17 rollback_cuts=17");return 0;}
