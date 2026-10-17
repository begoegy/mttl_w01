#include "ota_readback.h"
#include "ota_hash.h"
#include "ota_stream.h"
#include <string.h>
int mttl_ota_readback(const mttl_ota_layout *layout,uint32_t active,uint32_t target,uint32_t bytes,
                      const uint8_t expected[32],mttl_ota_read_fn read,void *context){
    mttl_ota_plan plan;mttl_ota_stream stream;mttl_ota_hash hash;uint8_t data[256],digest[32];uint32_t offset=0;int ok=0;
    if(!expected||!read||!mttl_ota_plan_make(layout,active,bytes,&plan)||target!=plan.target||!mttl_ota_stream_init(&stream,bytes))return 0;
    mttl_ota_hash_init(&hash);
    while(offset<bytes){uint32_t n=bytes-offset;if(n>sizeof(data))n=sizeof(data);
        if(read(context,target+offset,data,n)!=1||!mttl_ota_stream_feed(&stream,data,n)||!mttl_ota_hash_update(&hash,data,n))goto done;
        offset+=n;
    }
    if(!mttl_ota_stream_finish(&stream))goto done;
    if(!mttl_ota_hash_finish(&hash,digest))return 0;
    return !memcmp(digest,expected,32);
done:mttl_ota_hash_finish(&hash,digest);return ok;
}
