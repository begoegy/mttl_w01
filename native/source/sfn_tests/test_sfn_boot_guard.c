#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_boot_guard_core.h"
#include "../standalone/firmware/ota_hash.h"
#define FLASH_SIZE 0x100000u
#define RESCUE_BYTES 8192u
static uint8_t flashmem[FLASH_SIZE],canonical[RESCUE_BYTES],sector0[4096],full_sha[32],tail_sha[32];
static int writes,erases,fail_write;
static const uint8_t marker[32]="SFN-NATIVE-GUARD-TEST-A1";
static int rd(void *p,uint32_t a,uint8_t *b,size_t n){(void)p;if(a>FLASH_SIZE||n>FLASH_SIZE-a)return 0;memcpy(b,flashmem+a,n);return 1;}
static int er(void *p,uint32_t a){(void)p;if((a&4095u)||a>FLASH_SIZE-4096)return 0;++erases;memset(flashmem+a,255,4096);return 1;}
static int wr(void *p,uint32_t a,const uint8_t *b,size_t n){size_t i;(void)p;if(a>FLASH_SIZE||n>FLASH_SIZE-a)return 0;++writes;if(fail_write&&writes==fail_write)return 0;for(i=0;i<n;i++)if((flashmem[a+i]&b[i])!=b[i])return 0;for(i=0;i<n;i++)flashmem[a+i]&=b[i];return 1;}
static void hash(const uint8_t *p,size_t n,uint8_t out[32]){mttl_ota_hash h;mttl_ota_hash_init(&h);assert(mttl_ota_hash_update(&h,p,n));assert(mttl_ota_hash_finish(&h,out));}
static sfn_boot_guard_io io={0,rd,er,wr};
static sfn_boot_guard_descriptor desc={0xb000,0x80000,0xed000,0x100000,RESCUE_BYTES,0x200,marker,sizeof(marker),sector0,full_sha,tail_sha};
static int sig(uint32_t slot){return mttl_ota_signature(flashmem+slot+8);}
static void prepare_material(void){uint32_t i;for(i=0;i<RESCUE_BYTES;i++)canonical[i]=(uint8_t)((i*37u+11u)^(i>>3));canonical[0]=0xe0;canonical[1]=0x1f;canonical[2]=0;canonical[3]=0;memset(canonical+8,255,8);memcpy(sector0,canonical,4096);hash(canonical,RESCUE_BYTES,full_sha);hash(canonical+4096,RESCUE_BYTES-4096,tail_sha);}
static void base(void){memset(flashmem,255,sizeof(flashmem));writes=erases=fail_write=0;memcpy(flashmem+0x80000,canonical,RESCUE_BYTES);memcpy(flashmem+0x80008,"01958711",8);memcpy(flashmem+0xb008,"81958711",8);memcpy(flashmem+0xb000+0x200,marker,sizeof(marker));}
static void ready(uint8_t *out){sfn_boot_guard_state s;base();assert(sfn_boot_guard_start(&s,&desc,&io,1)==SFN_BOOT_GUARD_ARMED);assert(sfn_boot_guard_start(&s,&desc,&io,2)==SFN_BOOT_GUARD_ARMED);assert(sfn_boot_guard_start(&s,&desc,&io,3)==SFN_BOOT_GUARD_ROLLBACK_READY);memcpy(out,flashmem,FLASH_SIZE);}
int main(void){sfn_boot_guard_state s,t;static uint8_t snapshot[FLASH_SIZE];unsigned cut;prepare_material();
 base();assert(sfn_boot_guard_start(&s,&desc,&io,100)==SFN_BOOT_GUARD_ARMED);assert(sfn_boot_guard_mark_trusted(&s,&desc,&io));assert(sfn_boot_guard_start(&t,&desc,&io,200)==SFN_BOOT_GUARD_TRUSTED);
 ready(snapshot);memcpy(flashmem,snapshot,sizeof(flashmem));writes=erases=0;assert(sfn_boot_guard_restore(&s,&desc,&io));assert(sig(0x80000)==MTTL_OTA_NEW&&sig(0xb000)==MTTL_OTA_OLD);
 for(cut=1;cut<=19;cut++){memcpy(flashmem,snapshot,sizeof(flashmem));writes=erases=0;fail_write=(int)cut;assert(!sfn_boot_guard_restore(&s,&desc,&io));assert(sig(0xb000)==MTTL_OTA_NEW);fail_write=0;writes=erases=0;assert(sfn_boot_guard_start(&t,&desc,&io,4)==SFN_BOOT_GUARD_ROLLBACK_READY);assert(sfn_boot_guard_restore(&t,&desc,&io));assert(sig(0x80000)==MTTL_OTA_NEW&&sig(0xb000)==MTTL_OTA_OLD);}
 base();flashmem[0xed000]=0;assert(sfn_boot_guard_start(&s,&desc,&io,1)==SFN_BOOT_GUARD_ROLLBACK_READY);assert(sfn_boot_guard_restore(&s,&desc,&io));
 base();flashmem[0x80000+5000]^=1;assert(sfn_boot_guard_start(&s,&desc,&io,1)==SFN_BOOT_GUARD_UNAVAILABLE);assert(sig(0xb000)==MTTL_OTA_NEW);
 base();assert(sfn_boot_guard_start(&s,&desc,&io,100)==SFN_BOOT_GUARD_ARMED);assert(!sfn_boot_guard_should_timeout(&s,900099,900000));assert(sfn_boot_guard_should_timeout(&s,900100,900000));
 puts("SFN_BOOT_GUARD_POWER_CUT_TESTS=PASS");return 0;}
