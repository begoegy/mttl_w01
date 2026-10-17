#include "ota_identity.h"
#include <string.h>
extern unsigned mttl_test_count,mttl_test_failed,mttl_failed_ids[64];
static void check(int ok){++mttl_test_count;if(!ok&&mttl_test_failed<64)mttl_failed_ids[mttl_test_failed++]=mttl_test_count;}
static uint8_t slots[2][600],running[512];
static unsigned calls,cut,bad;
static int read(void *p,uint32_t a,uint8_t *out,size_t n){uint32_t base;unsigned slot;(void)p;
 slot=a>=0x80000;base=slot?0x80000:0xb000;
 if(a<base||a-base>600||n>600-(a-base)||n>256){bad=1;return 0;}
 if(++calls==cut)return 0;memcpy(out,slots[slot]+a-base,n);return 1;
}
void mttl_ota_identity_test(void){mttl_ota_layout l={0xb000,0x80000,0xf0000,0x100000};unsigned i,total;
 memset(slots,0,sizeof(slots));for(i=0;i<512;i++)running[i]=(uint8_t)(i*13);
 for(i=0;i<2;i++){slots[i][0]=0x18;slots[i][1]=2;slots[i][5]=0x60;slots[i][7]=0x10;}
 memcpy(slots[0]+40,running,512);
 check(mttl_ota_identity(&l,running,512,24,1,read,0)==l.first&&!bad);total=calls;
 for(i=1;i<=total;i++){calls=0;cut=i;check(!mttl_ota_identity(&l,running,512,24,1,read,0));}cut=0;
 memcpy(slots[1]+40,running,512);check(!mttl_ota_identity(&l,running,512,24,1,read,0));
 slots[0][40]^=1;check(mttl_ota_identity(&l,running,512,24,1,read,0)==l.second);
 slots[1][40]^=1;check(!mttl_ota_identity(&l,running,512,24,1,read,0));
 calls=0;check(!mttl_ota_identity(&l,running,512,24,0,read,0)&&!calls);check(!mttl_ota_identity(&l,running,255,24,1,read,0)&&!calls);
 check(!mttl_ota_identity(&l,running,512,UINT32_MAX,1,read,0)&&!calls);
 l.second=l.first;check(!mttl_ota_identity(&l,running,512,24,1,read,0)&&!calls);
}
