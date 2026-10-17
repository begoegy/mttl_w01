#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../standalone/firmware/sfn_update_guard.h"
#define FLASH_SIZE 0x100000u
#define FIRST 0xb000u
#define SECOND 0x80000u
static uint8_t flashmem[FLASH_SIZE],rescue0[4096],sha[32],tail[32],marker[32];
static int writes,erases;
static int rd(void *p,uint32_t a,uint8_t *b,size_t n){(void)p;if(a>FLASH_SIZE||n>FLASH_SIZE-a)return 0;memcpy(b,flashmem+a,n);return 1;}
static int er(void *p,uint32_t a){(void)p;if((a&4095u)||a>FLASH_SIZE-4096)return 0;erases++;memset(flashmem+a,255,4096);return 1;}
static int wr(void *p,uint32_t a,const uint8_t *b,size_t n){size_t i;(void)p;if(a>FLASH_SIZE||n>FLASH_SIZE-a)return 0;writes++;for(i=0;i<n;i++)if((flashmem[a+i]&b[i])!=b[i])return 0;for(i=0;i<n;i++)flashmem[a+i]&=b[i];return 1;}
static sfn_boot_guard_io io={0,rd,er,wr};
static uint8_t *load(const char *path,size_t *n){FILE *f=fopen(path,"rb");uint8_t *p;long z;if(!f)return 0;fseek(f,0,SEEK_END);z=ftell(f);rewind(f);if(z<44||z>0x70000){fclose(f);return 0;}p=malloc((size_t)z);if(!p){fclose(f);return 0;}if(fread(p,1,(size_t)z,f)!=(size_t)z){free(p);fclose(f);return 0;}fclose(f);*n=(size_t)z;return p;}
static size_t find_marker(const uint8_t *p,size_t n,const uint8_t *m){size_t i;for(i=0;i+32<=n;i++)if(!memcmp(p+i,m,32))return i;return 0;}
static void patch_sig(uint8_t *p,const char *sig){memcpy(p+8,sig,8);}
static void cycle(const uint8_t *stock,size_t stock_n,const uint8_t *native,size_t native_n,uint32_t previous,uint32_t candidate,size_t marker_off){sfn_boot_guard_descriptor d={FIRST,SECOND,0xed000,0x100000,8192,(uint32_t)marker_off,marker,sizeof(marker),rescue0,sha,tail};sfn_update_guard_state s;uint8_t *expect=malloc(stock_n);assert(expect);memcpy(expect,stock,stock_n);patch_sig(expect,"81958711");memset(flashmem,255,sizeof(flashmem));memcpy(flashmem+previous,stock,stock_n);memcpy(flashmem+candidate,native,native_n);patch_sig(flashmem+previous,"01958711");patch_sig(flashmem+candidate,"81958711");writes=erases=0;assert(sfn_update_guard_adopt_first_flash(&d,&io,candidate));assert(sfn_update_guard_start(&s,&d,&io,candidate,1)==SFN_UPDATE_GUARD_ARMED);assert(sfn_update_guard_start(&s,&d,&io,candidate,2)==SFN_UPDATE_GUARD_ARMED);assert(sfn_update_guard_start(&s,&d,&io,candidate,3)==SFN_UPDATE_GUARD_ROLLBACK_READY);assert(sfn_update_guard_rollback(&s,&d,&io));assert(!memcmp(flashmem+previous,expect,stock_n));assert(!memcmp(flashmem+candidate+8,"01958711",8));free(expect);}
int main(int argc,char **argv){uint8_t *stock,*native;size_t stock_n,native_n,marker_off;if(argc!=4){fprintf(stderr,"usage: %s STOCK.bin NATIVE.bin MARKER\n",argv[0]);return 2;}memset(marker,0,sizeof(marker));if(strlen(argv[3])>=sizeof(marker))return 2;memcpy(marker,argv[3],strlen(argv[3]));stock=load(argv[1],&stock_n);native=load(argv[2],&native_n);assert(stock&&native);marker_off=find_marker(native,native_n,marker);assert(marker_off>=40);cycle(stock,stock_n,native,native_n,FIRST,SECOND,marker_off);cycle(stock,stock_n,native,native_n,SECOND,FIRST,marker_off);printf("SFN_FIRST_FLASH_ACTUAL=PASS stock_bytes=%zu native_bytes=%zu orientations=2\n",stock_n,native_n);free(stock);free(native);return 0;}
