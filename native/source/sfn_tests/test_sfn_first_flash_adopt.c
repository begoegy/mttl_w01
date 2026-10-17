#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_update_guard.h"
#define FLASH_SIZE 0x100000u
#define FIRST 0xb000u
#define SECOND 0x80000u
static uint8_t flashmem[FLASH_SIZE];
static int writes,erases,fail_write;
static const uint8_t marker[32]="SFN-NATIVE-GUARD-2.0.19-A1";
static uint8_t rescue0[4096],sha[32],tail[32];
static sfn_boot_guard_descriptor desc={FIRST,SECOND,0xed000,0x100000,8192,0x200,marker,sizeof(marker),rescue0,sha,tail};
static int rd(void *p,uint32_t a,uint8_t *b,size_t n){(void)p;if(a>FLASH_SIZE||n>FLASH_SIZE-a)return 0;memcpy(b,flashmem+a,n);return 1;}
static int er(void *p,uint32_t a){(void)p;if((a&4095u)||a>FLASH_SIZE-4096)return 0;++erases;memset(flashmem+a,255,4096);return 1;}
static int wr(void *p,uint32_t a,const uint8_t *b,size_t n){size_t i;(void)p;if(a>FLASH_SIZE||n>FLASH_SIZE-a)return 0;++writes;if(fail_write&&writes==fail_write)return 0;for(i=0;i<n;i++)if((flashmem[a+i]&b[i])!=b[i])return 0;for(i=0;i<n;i++)flashmem[a+i]&=b[i];return 1;}
static sfn_boot_guard_io io={0,rd,er,wr};
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static void image(uint32_t slot,const char *signature,int native_marker){static const uint8_t magic[20]={0x52,0x54,0x4b,0x57,0x69,0x6e,0,0xff,0,1,1,0,0x95,0x81,1,1,0,0,0,0};static const uint8_t hw[32]="LGU+-TAP-HW002";uint32_t bytes=12288,body=bytes-20,sum=0,i;uint8_t *p=flashmem+slot;memset(p,0xa5,bytes);put32(p,body);put32(p+4,0x10006000);memcpy(p+8,signature,8);put32(p+16,0x10006101);memcpy(p+20,magic,20);memcpy(p+0x28,hw,sizeof(hw));if(native_marker)memcpy(p+0x200,marker,sizeof(marker));for(i=0;i<bytes-4;i++)sum+=(i>=8&&i<16)?255u:p[i];put32(p+bytes-4,sum);}
static void setup(uint32_t previous,uint32_t candidate){memset(flashmem,255,sizeof(flashmem));writes=erases=fail_write=0;image(previous,"01958711",0);image(candidate,"81958711",1);}
static int sig(uint32_t slot,const char *x){return !memcmp(flashmem+slot+8,x,8);}
static void full_cycle(uint32_t previous,uint32_t candidate){sfn_update_guard_state s;setup(previous,candidate);assert(sfn_update_guard_adopt_first_flash(&desc,&io,candidate));assert(sfn_update_guard_start(&s,&desc,&io,candidate,1)==SFN_UPDATE_GUARD_ARMED);assert(sfn_update_guard_start(&s,&desc,&io,candidate,2)==SFN_UPDATE_GUARD_ARMED);assert(sfn_update_guard_start(&s,&desc,&io,candidate,3)==SFN_UPDATE_GUARD_ROLLBACK_READY);assert(sfn_update_guard_rollback(&s,&desc,&io));assert(sig(previous,"81958711")&&sig(candidate,"01958711"));}
static void cut_cycle(uint32_t previous,uint32_t candidate){unsigned cut;for(cut=1;cut<=17;cut++){sfn_update_guard_state s;setup(previous,candidate);fail_write=(int)cut;writes=0;assert(!sfn_update_guard_adopt_first_flash(&desc,&io,candidate));assert(sig(previous,"01958711")&&sig(candidate,"81958711"));fail_write=0;writes=0;assert(sfn_update_guard_adopt_first_flash(&desc,&io,candidate));assert(sfn_update_guard_start(&s,&desc,&io,candidate,1)==SFN_UPDATE_GUARD_ARMED);assert(sfn_update_guard_start(&s,&desc,&io,candidate,2)==SFN_UPDATE_GUARD_ARMED);assert(sfn_update_guard_start(&s,&desc,&io,candidate,3)==SFN_UPDATE_GUARD_ROLLBACK_READY);assert(sfn_update_guard_rollback(&s,&desc,&io));assert(sig(previous,"81958711")&&sig(candidate,"01958711"));}}
int main(void){uint32_t previous=FIRST,candidate=SECOND;int e;
 full_cycle(FIRST,SECOND);full_cycle(SECOND,FIRST);cut_cycle(FIRST,SECOND);cut_cycle(SECOND,FIRST);
 setup(previous,candidate);flashmem[0x7e000u+200u]=0;erases=0;assert(!sfn_update_guard_adopt_first_flash(&desc,&io,candidate)&&erases==0);
 setup(previous,candidate);flashmem[previous+0x28u]='X';erases=0;assert(!sfn_update_guard_adopt_first_flash(&desc,&io,candidate)&&erases==0);
 setup(previous,candidate);flashmem[previous+100u]^=1;erases=0;assert(!sfn_update_guard_adopt_first_flash(&desc,&io,candidate)&&erases==0);
 setup(previous,candidate);memcpy(flashmem+candidate+8,"01958711",8);erases=0;assert(!sfn_update_guard_adopt_first_flash(&desc,&io,candidate)&&erases==0);
 e=0;printf("SFN_FIRST_FLASH_ADOPT_TESTS=PASS orientations=2 adoption_write_cuts=%d rollback=PASS fail_closed=%d\n",34,4);return e;}
