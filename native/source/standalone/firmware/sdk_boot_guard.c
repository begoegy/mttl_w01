#ifdef SFN_BOOT_GUARD
#include "sdk_boot_guard.h"
#include "sfn_boot_guard_core.h"
#include "sfn_rescue_image.h"
#include "flash_api.h"
#include "device_lock.h"
#include "sys_api.h"
#include <stdint.h>
#include <string.h>
extern flash_t flash;
extern unsigned char __ram_image2_text_start__[];
static const uint8_t sfn_native_guard_marker[32]="SFN-NATIVE-GUARD-2.0.14-A1";
static sfn_boot_guard_state guard_state;
static uint32_t online_since;
static uint8_t online_seen;
static int raw_read(void *p,uint32_t a,uint8_t *b,size_t n){int r;(void)p;if(!b||!n||a>=0x100000u||n>0x100000u-a)return 0;device_mutex_lock(RT_DEV_LOCK_FLASH);r=flash_stream_read(&flash,a,(uint32_t)n,b);device_mutex_unlock(RT_DEV_LOCK_FLASH);return r==1;}
static int erase_sector(void *p,uint32_t a){uint8_t b[256];uint32_t off;unsigned i;int ok=1;(void)p;if((a&4095u)||a>=0x100000u||0x100000u-a<4096u)return 0;device_mutex_lock(RT_DEV_LOCK_FLASH);flash_erase_sector(&flash,a);for(off=0;off<4096;off+=sizeof(b)){if(flash_stream_read(&flash,a+off,sizeof(b),b)!=1){ok=0;break;}for(i=0;i<sizeof(b);i++)if(b[i]!=255){ok=0;break;}if(!ok)break;}device_mutex_unlock(RT_DEV_LOCK_FLASH);return ok;}
static int raw_write(void *p,uint32_t a,const uint8_t *b,size_t n){uint8_t before[256],after[256];size_t pos=0,k,i;int ok=1;(void)p;if(!b||!n||a>=0x100000u||n>0x100000u-a)return 0;device_mutex_lock(RT_DEV_LOCK_FLASH);while(pos<n){k=n-pos;if(k>sizeof(before))k=sizeof(before);if(flash_stream_read(&flash,a+pos,(uint32_t)k,before)!=1){ok=0;break;}for(i=0;i<k;i++)if((before[i]&b[pos+i])!=b[pos+i]){ok=0;break;}if(!ok||flash_stream_write(&flash,a+pos,(uint32_t)k,(uint8_t *)b+pos)<0||flash_stream_read(&flash,a+pos,(uint32_t)k,after)!=1||memcmp(after,b+pos,k)){ok=0;break;}pos+=k;}device_mutex_unlock(RT_DEV_LOCK_FLASH);return ok;}
static const sfn_boot_guard_io io={0,raw_read,erase_sector,raw_write};
static int hardware_geometry(void){uint8_t a[3],b[3];uint32_t first,second;int x,y,r;device_mutex_lock(RT_DEV_LOCK_FLASH);x=flash_read_id(&flash,a,3);y=flash_read_id(&flash,b,3);r=x==3&&y==3&&!memcmp(a,b,3)&&a[0]&&a[0]!=255&&a[2]==20&&flash_stream_read(&flash,0x18,4,(uint8_t *)&first)==1&&flash_stream_read(&flash,0x9000,4,(uint8_t *)&second)==1&&(first&0xffffu)==0x2cu&&second==0x80000u;device_mutex_unlock(RT_DEV_LOCK_FLASH);return r;}
static sfn_boot_guard_descriptor descriptor(void){sfn_boot_guard_descriptor d;uintptr_t marker=(uintptr_t)sfn_native_guard_marker,base=(uintptr_t)__ram_image2_text_start__;memset(&d,0,sizeof(d));d.first=0xb000u;d.second=0x80000u;d.guard_sector=0xed000u;d.flash_size=0x100000u;d.rescue_bytes=SFN_RESCUE_FILE_BYTES;d.marker_offset=marker>=base?(uint32_t)(marker-base):0;d.marker=sfn_native_guard_marker;d.marker_size=sizeof(sfn_native_guard_marker);d.rescue_sector0=sfn_rescue_sector0;d.rescue_sha256=sfn_rescue_normalized_sha256;d.rescue_tail_sha256=sfn_rescue_tail_sha256;return d;}
static void rollback_now(void){sfn_boot_guard_descriptor d=descriptor();if(guard_state.available&&hardware_geometry()&&sfn_boot_guard_restore(&guard_state,&d,&io)){sys_reset();for(;;){}}}
int sfn_sdk_boot_guard_start(uint32_t now){sfn_boot_guard_descriptor d;int r;memset(&guard_state,0,sizeof(guard_state));online_seen=0;online_since=0;if(!hardware_geometry())return 0;d=descriptor();r=sfn_boot_guard_start(&guard_state,&d,&io,now);if(r==SFN_BOOT_GUARD_ROLLBACK_READY)rollback_now();return r==SFN_BOOT_GUARD_ARMED||r==SFN_BOOT_GUARD_TRUSTED;}
void sfn_sdk_boot_guard_tick(uint32_t now,int online){sfn_boot_guard_descriptor d;if(guard_state.trusted||!guard_state.available)return;if(online){if(!online_seen){online_seen=1;online_since=now;}else if((uint32_t)(now-online_since)>=60000u){d=descriptor();if(sfn_boot_guard_mark_trusted(&guard_state,&d,&io))return;}}else{online_seen=0;online_since=0;}if(sfn_boot_guard_should_timeout(&guard_state,now,900000u))rollback_now();}
int sfn_sdk_boot_guard_available(void){return guard_state.available!=0;}
#endif
