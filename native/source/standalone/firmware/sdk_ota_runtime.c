#ifdef MTTL_OTA_TEST
#include "sdk_ota_runtime.h"
#include "ota_identity.h"
#include "ota_session.h"
#include "sdk_ota_flash.h"
#include "flash_api.h"
#include "device_lock.h"
#include "ota_test_config.h"
#include <string.h>
flash_t flash;
extern unsigned char __ram_image2_text_start__[],__data_start__[];
static const mttl_ota_layout layout={0xb000,0x80000,0xee000,0x100000};
static mttl_ota_session session;
static mttl_sdk_ota_flash adapter;
static uint32_t active;
static uint8_t authorized;
static uint8_t restored_version;
/* Identify the supported OEM Image2 family, not an exact file digest.
 * These fixed-layout model/version strings survive our IP-dependent patch.
 * Format/checksum/SHA receive+readback checks are enforced by the session.
 * This is compatibility identification, not cryptographic authenticity. */
static int legacy_image(void){
 uint8_t model[9],version[7];uint32_t target=session.stage.plan.target;
 if(session.bytes<132418u)return 0;
 if(!mttl_sdk_ota_flash_read(&adapter,target+736,model,9)||memcmp(model,"MTTL-W01",9))return 0;
 if(!mttl_sdk_ota_flash_read(&adapter,target+70036,model,9)||memcmp(model,"MTTL-W01",9))return 0;
 if(!mttl_sdk_ota_flash_read(&adapter,target+101988,version,7))return 0;
 if(!memcmp(version,"1.0.66",7))return 66;
 if(!memcmp(version,"1.0.68",7))return 68;
 return 0;
}
int mttl_sdk_ota_restore_version(void){return restored_version;}
static int raw_read(void *context,uint32_t a,uint8_t *b,size_t n){
 int r;(void)context;if(!b||!n||n>256||a>=layout.protected_start||n>layout.protected_start-a)return 0;
 device_mutex_lock(RT_DEV_LOCK_FLASH);r=flash_stream_read(&flash,a,n,b);device_mutex_unlock(RT_DEV_LOCK_FLASH);return r==1;
}
static int boot_compatible(void){
 uint8_t bytes[256],digest[32];uint32_t pos=0,first,second;mttl_ota_hash hash;
 static const uint8_t expected[32]=MTTL_OTA_BOOT_SHA;
 /* Captured boot table stores the sector index in the low 16 bits;
  * high bits are loader flags, not part of the Image2 address. */
 if(!raw_read(0,0x18,(uint8_t *)&first,4)||!raw_read(0,0x9000,(uint8_t *)&second,4)||(first&0xffffu)!=layout.first/1024||second!=layout.second)return 0;
 mttl_ota_hash_init(&hash);
 while(pos<MTTL_OTA_BOOT_BYTES){size_t n=MTTL_OTA_BOOT_BYTES-pos;if(n>sizeof(bytes))n=sizeof(bytes);
  if(!raw_read(0,32+pos,bytes,n)){mttl_ota_hash_finish(&hash,digest);return 0;}
  mttl_ota_hash_update(&hash,bytes,n);pos+=(uint32_t)n;
 }
 return mttl_ota_hash_finish(&hash,digest)&&!memcmp(digest,expected,32);
}
static int identity_read(void *context,uint32_t a,uint8_t *b,size_t n){
 const uint8_t *eligible=context;
 /* A staged FF/invalid signature is not boot-selectable by the fingerprinted
  * loader, and therefore cannot be the executing slot. Ignore its body. */
 if(n==8&&((a==layout.first&&!eligible[0])||(a==layout.second&&!eligible[1]))){memset(b,0,n);return 1;}
 return raw_read(0,a,b,n);
}
uint32_t mttl_sdk_ota_probe(void){
 uint8_t id[3]={0},again[3]={0},signature[8],eligible[2];uint32_t found;unsigned i;int first_id,second_id;
 const uint8_t *text=__ram_image2_text_start__+24;
 /* Vendor text_end is after .data, which changes during normal runtime.
  * Fingerprint immutable text/rodata only, ending before writable .data. */
 size_t n=(size_t)(__data_start__-text);
 /* This test profile supports only the captured 1 MiB geometry. Fail closed
  * for unknown capacity, ambiguous duplicate images or modified running code. */
 device_mutex_lock(RT_DEV_LOCK_FLASH);first_id=flash_read_id(&flash,id,3);second_id=flash_read_id(&flash,again,3);device_mutex_unlock(RT_DEV_LOCK_FLASH);
 /* Linked SDK flash_read_id returns the requested count (3), or -1. */
 if(first_id!=3||second_id!=3||memcmp(id,again,3)||!id[0]||id[0]==255||id[2]!=20||!boot_compatible())return 0;
 for(i=0;i<2;i++){int kind;if(!raw_read(0,(i?layout.second:layout.first)+8,signature,8))return 0;kind=mttl_ota_signature(signature);eligible[i]=kind==MTTL_OTA_NEW||kind==MTTL_OTA_OLD;}
 found=mttl_ota_identity(&layout,text,n,24,1,identity_read,eligible);
 if(!found||!raw_read(0,found+8,signature,8))return 0;
 if(mttl_ota_signature(signature)!=MTTL_OTA_NEW&&mttl_ota_signature(signature)!=MTTL_OTA_OLD)return 0;
 return found;
}
static int admit(void *p,const mttl_ota_layout *l,uint32_t a,const mttl_ota_plan *plan,uint32_t n,const uint8_t sha[32]){
 (void)p;(void)sha;(void)n;
 if(!authorized||a!=active||memcmp(l,&layout,sizeof(layout))||mttl_sdk_ota_probe()!=active)return 0;
 adapter.plan=*plan;adapter.verified=1;return 1;
}
static int read_io(void *p,uint32_t a,uint8_t *b,size_t n){return mttl_sdk_ota_flash_read(p,a,b,n);}
static int write_io(void *p,uint32_t a,const uint8_t *b,size_t n){return mttl_sdk_ota_flash_write(p,a,b,n);}
static int erase_io(void *p,uint32_t a){return mttl_sdk_ota_flash_erase(p,a);}
void mttl_sdk_ota_abort(void){mttl_ota_session_abort(&session);authorized=0;adapter.verified=0;active=0;restored_version=0;}
static int same_image(uint32_t bytes,const uint8_t expected[32]){
 uint32_t body,pos=0;mttl_ota_hash hash;uint8_t chunk[256],digest[32];
 if(!raw_read(0,active,(uint8_t *)&body,4))return -1;
 if(body+20!=bytes)return 0;
 mttl_ota_hash_init(&hash);
 while(pos<bytes){size_t n=bytes-pos,i;if(n>256)n=256;
  if(!raw_read(0,active+pos,chunk,n)){mttl_ota_hash_finish(&hash,digest);return -1;}
  for(i=0;i<n;i++)if(pos+i>=8&&pos+i<16)chunk[i]=255;
  mttl_ota_hash_update(&hash,chunk,n);pos+=(uint32_t)n;
 }
 return !mttl_ota_hash_finish(&hash,digest)?-1:!memcmp(digest,expected,32);
}
int mttl_sdk_ota_begin(uint32_t bytes,const uint8_t sha[32]){
 mttl_ota_stage_io io={&adapter,erase_io,write_io,read_io};
 mttl_sdk_ota_abort();active=mttl_sdk_ota_probe();if(!active||!sha||same_image(bytes,sha)!=0){mttl_sdk_ota_abort();return 0;}
 mttl_sdk_ota_flash_init(&adapter);authorized=1;
 if(!mttl_ota_session_init(&session,&layout,active,bytes,sha,&io,admit,0)){mttl_sdk_ota_abort();return 0;}return 1;
}
int mttl_sdk_ota_feed(const uint8_t *b,size_t n){return authorized&&mttl_ota_session_feed(&session,b,n);}
static int commit_word(uint32_t address,uint32_t value,uint32_t *after){
 uint32_t before;int ok;
 /* Activation touches only the three words selected by the pure commit model. */
 if(address!=session.stage.plan.target+8&&address!=session.stage.plan.target+12&&address!=active+8)return 0;
 device_mutex_lock(RT_DEV_LOCK_FLASH);
 ok=flash_stream_read(&flash,address,4,(uint8_t *)&before)==1&&(before&value)==value;
 if(ok){flash_stream_write(&flash,address,4,(uint8_t *)&value);ok=flash_stream_read(&flash,address,4,(uint8_t *)after)==1&&*after==value;}
 device_mutex_unlock(RT_DEV_LOCK_FLASH);return ok;
}
static int ota_profile_present(void){
 static const char marker[]="NETWORK_OTA_TEST_RAM_SETTINGS_";
 uint8_t window[256+sizeof(marker)];size_t keep=0;uint32_t pos=16;
 while(pos<session.bytes-4){size_t n=session.bytes-4-pos,i,total;if(n>256)n=256;
  if(!mttl_sdk_ota_flash_read(&adapter,session.stage.plan.target+pos,window+keep,n))return 0;
  total=keep+n;for(i=0;i+sizeof(marker)-1<=total;i++)if(!memcmp(window+i,marker,sizeof(marker)-1))return 1;
  keep=total<sizeof(marker)-2?total:sizeof(marker)-2;memmove(window,window+total-keep,keep);pos+=(uint32_t)n;
 }
 return 0;
}
int mttl_sdk_ota_finish(void){
 mttl_ota_commit commit;uint32_t a,v,r;int legacy;
 if(!authorized||!mttl_ota_session_finish(&session)){mttl_sdk_ota_abort();return 0;}
 legacy=legacy_image();
 if(same_image(session.bytes,session.stage.expected)!=0||(!legacy&&!ota_profile_present())||mttl_sdk_ota_probe()!=active||
    !mttl_ota_commit_init(&commit,&layout,active,session.stage.plan.target,1)){mttl_sdk_ota_abort();return 0;}
 while(mttl_ota_commit_action(&commit,&a,&v)){int ok;r=0;ok=commit_word(a,v,&r);if(!mttl_ota_commit_ack(&commit,ok,r)){mttl_sdk_ota_abort();return 0;}}
 restored_version=(uint8_t)legacy;authorized=0;adapter.verified=0;return mttl_ota_commit_done(&commit);
}
#endif
