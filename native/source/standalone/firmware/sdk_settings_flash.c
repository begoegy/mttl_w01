#include "sdk_settings_flash.h"
#include "config_store.h"
#include "flash_api.h"
#include "device_lock.h"
#include "FreeRTOS.h"
#include "sdk_memory.h"
#include <string.h>
extern flash_t flash;
/* Separate 4 KiB erase units, excluded from OTA by protected_start=EE000.
 * Never reuse vendor F0000..FFFFF factory/calibration data. */
static const uint32_t sectors[2]={0xee000,0xef000};
static int geometry(void){
 uint8_t a[3],b[3];uint32_t first,second;int ok;
 device_mutex_lock(RT_DEV_LOCK_FLASH);
 ok=flash_read_id(&flash,a,3)==3&&flash_read_id(&flash,b,3)==3&&!memcmp(a,b,3)&&a[0]&&a[0]!=255&&a[2]==20&&
 flash_stream_read(&flash,0x18,4,(uint8_t *)&first)==1&&flash_stream_read(&flash,0x9000,4,(uint8_t *)&second)==1&&
 (first&0xffffu)==0x2c&&second==0x80000;
 device_mutex_unlock(RT_DEV_LOCK_FLASH);return ok;
}
static int read_slot(void *p,unsigned slot,size_t offset,uint8_t *out,size_t size){
 mttl_sdk_memory_sample();
 size_t n;int ok=1;uint8_t tail[256];unsigned i;uint32_t a;(void)p;if(slot>1||!out||!size||offset>=MTTL_SETTINGS_SLOT||size>MTTL_SETTINGS_SLOT-offset)return 0;
 device_mutex_lock(RT_DEV_LOCK_FLASH);
 /* Unrecognized data in the unused half means this is not our partition. */
 for(a=MTTL_SETTINGS_SLOT;a<4096;a+=256){if(flash_stream_read(&flash,sectors[slot]+a,256,tail)!=1){ok=0;break;}for(i=0;i<256;i++)if(tail[i]!=255)ok=0;if(!ok)break;}
 if(!ok){device_mutex_unlock(RT_DEV_LOCK_FLASH);return 0;}
 while(size){n=size>256?256:size;if(flash_stream_read(&flash,sectors[slot]+offset,n,out)!=1){ok=0;break;}offset+=n;out+=n;size-=n;}
 device_mutex_unlock(RT_DEV_LOCK_FLASH);return ok;
}
static int erase_slot(void *p,unsigned slot){
 uint8_t data[256];uint32_t offset;unsigned i;int ok=1;(void)p;if(slot>1)return 0;
 device_mutex_lock(RT_DEV_LOCK_FLASH);flash_erase_sector(&flash,sectors[slot]);
 for(offset=0;offset<4096;offset+=256){if(flash_stream_read(&flash,sectors[slot]+offset,256,data)!=1){ok=0;break;}for(i=0;i<256;i++)if(data[i]!=255)ok=0;if(!ok)break;}
 device_mutex_unlock(RT_DEV_LOCK_FLASH);return ok;
}
static int program_slot(void *p,unsigned slot,size_t offset,const uint8_t *data,size_t size){
 mttl_sdk_memory_sample();
 uint8_t before[256],after[256];size_t n,i;int ok=1;(void)p;
 if(slot>1||!data||!size||offset>=MTTL_SETTINGS_SLOT||size>MTTL_SETTINGS_SLOT-offset)return 0;
 device_mutex_lock(RT_DEV_LOCK_FLASH);
 while(size){uint32_t a=sectors[slot]+offset;n=size>256?256:size;
  if(flash_stream_read(&flash,a,n,before)!=1){ok=0;break;}
  for(i=0;i<n;i++)if((before[i]&data[i])!=data[i])ok=0;
  if(!ok||flash_stream_write(&flash,a,n,(uint8_t *)data)<0||flash_stream_read(&flash,a,n,after)!=1||memcmp(after,data,n)){ok=0;break;}
  offset+=n;data+=n;size-=n;
 }
 device_mutex_unlock(RT_DEV_LOCK_FLASH);return ok;
}
static const mttl_settings_io io={0,read_slot,erase_slot,program_slot};
static int operation(mttl_settings *out,const mttl_settings *in){
 mttl_config_workspace *w;int result=-1;
 if(!geometry())return -1;
 w=pvPortMalloc(sizeof(*w));if(!w)return -1;
 memset(w,0,sizeof(*w));result=in?mttl_config_save(&io,w,in):mttl_config_load(&io,w,out);
 mttl_settings_workspace_clear(&w->records);memset(w,0,sizeof(*w));vPortFree(w);return result;
}
int mttl_sdk_settings_load(mttl_settings *out){return out?operation(out,0):-1;}
/* Runtime heap is fragmented by sockets. One record plus a codec body avoids
 * the old 5 KiB contiguous allocation, without changing the on-flash format. */
int mttl_sdk_settings_save(const mttl_settings *in){
 uint8_t *record,*body,check[256],commit=0;uint32_t seq[2]={0,0},delta,newseq=0;
 size_t sizes[2]={0,0},n,offset,chunk;int valid[2],blank[2],old=-1,target,result=-1;unsigned slot,i;
 mttl_settings existing;
 if(!in||!mttl_settings_valid(in)||!geometry())return -1;
 record=pvPortMalloc(MTTL_SETTINGS_SLOT+MTTL_SETTINGS_BODY_MAX);if(!record)return -1;
 body=record+MTTL_SETTINGS_SLOT;
 for(slot=0;slot<2;slot++){
  if(!read_slot(0,slot,0,record,MTTL_SETTINGS_SLOT))goto done;
  blank[slot]=1;for(i=0;i<MTTL_SETTINGS_SLOT;i++)if(record[i]!=255)blank[slot]=0;
  valid[slot]=mttl_settings_validate(record,&seq[slot],&sizes[slot]);
 }
 if(valid[0]&&valid[1]){delta=seq[1]-seq[0];if(!delta||delta==0x80000000u)goto done;old=delta<0x80000000u?1:0;}
 else if(valid[0])old=0;else if(valid[1])old=1;
 else if(!blank[0]||!blank[1])goto done;
 if(old>=0){
  if(!read_slot(0,old,0,record,MTTL_SETTINGS_SLOT)||!mttl_settings_extract(record,body,MTTL_SETTINGS_BODY_MAX,&n)||!mttl_settings_storage_unpack(body,n,&existing))goto done;
  newseq=seq[old]+1;
 }
 target=old<0?0:1-old;
 n=mttl_settings_storage_pack(in,body,MTTL_SETTINGS_BODY_MAX);
 if(!n||!mttl_settings_encode(record,body,n,newseq)||!erase_slot(0,target)||!program_slot(0,target,0,record,MTTL_SETTINGS_SLOT-1))goto done;
 for(offset=0;offset<MTTL_SETTINGS_SLOT;offset+=chunk){
  chunk=MTTL_SETTINGS_SLOT-offset;if(chunk>sizeof(check))chunk=sizeof(check);
  if(!read_slot(0,target,offset,check,chunk)||memcmp(check,record+offset,chunk))goto done;
 }
 if(!program_slot(0,target,MTTL_SETTINGS_SLOT-1,&commit,1)||!read_slot(0,target,0,record,MTTL_SETTINGS_SLOT))goto done;
 if(!mttl_settings_validate(record,&seq[0],&n)||seq[0]!=newseq)goto done;
 result=1;
done:
 memset(&existing,0,sizeof(existing));memset(check,0,sizeof(check));memset(record,0,MTTL_SETTINGS_SLOT+MTTL_SETTINGS_BODY_MAX);vPortFree(record);return result;
}
