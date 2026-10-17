#include "sdk_ota_flash.h"
#ifdef MTTL_SDK_FLASH_TEST
#include "sdk_flash_mock.h"
#else
#include "flash_api.h"
#include "device_lock.h"
#endif
#include <string.h>
void mttl_sdk_ota_flash_init(mttl_sdk_ota_flash *p){if(p)memset(p,0,sizeof(*p));}
static int range(const mttl_sdk_ota_flash *p,uint32_t a,size_t n){
    return p&&p->verified==1&&n>0&&n<=256&&p->plan.target<p->plan.erase_end&&
        a>=p->plan.target&&a<p->plan.erase_end&&n<=p->plan.erase_end-a;
}
int mttl_sdk_ota_flash_read(mttl_sdk_ota_flash *p,uint32_t a,uint8_t *out,size_t n){
    int r;if(!out||!range(p,a,n))return 0;device_mutex_lock(RT_DEV_LOCK_FLASH);
    r=flash_stream_read(&flash,a,(uint32_t)n,out);device_mutex_unlock(RT_DEV_LOCK_FLASH);if(r!=1)p->verified=0;return r==1;
}
int mttl_sdk_ota_flash_write(mttl_sdk_ota_flash *p,uint32_t a,const uint8_t *data,size_t n){
    uint8_t before[256],after[256];int r;size_t i;if(!data||!range(p,a,n))return 0;
    device_mutex_lock(RT_DEV_LOCK_FLASH);
    r=flash_stream_read(&flash,a,(uint32_t)n,before);if(r!=1)goto fail;
    for(i=0;i<n;i++)if((before[i]&data[i])!=data[i])goto fail;
    r=flash_stream_write(&flash,a,(uint32_t)n,(uint8_t *)data);if(r<0)goto fail;
    r=flash_stream_read(&flash,a,(uint32_t)n,after);if(r!=1||memcmp(after,data,n))goto fail;
    device_mutex_unlock(RT_DEV_LOCK_FLASH);return 1;
fail:device_mutex_unlock(RT_DEV_LOCK_FLASH);p->verified=0;return 0;
}
int mttl_sdk_ota_flash_erase(mttl_sdk_ota_flash *p,uint32_t a){
    uint8_t data[256];uint32_t offset;unsigned i;
    if(!p||p->verified!=1||(a&4095)||a<p->plan.target||a>=p->plan.erase_end||p->plan.erase_end-a<4096)return 0;
    device_mutex_lock(RT_DEV_LOCK_FLASH);flash_erase_sector(&flash,a);
    for(offset=0;offset<4096;offset+=256){if(flash_stream_read(&flash,a+offset,256,data)!=1)goto fail;for(i=0;i<256;i++)if(data[i]!=255)goto fail;}
    device_mutex_unlock(RT_DEV_LOCK_FLASH);return 1;
fail:device_mutex_unlock(RT_DEV_LOCK_FLASH);p->verified=0;return 0;
}
