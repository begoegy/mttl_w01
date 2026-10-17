#include "config_store.h"
static void clear(void *p,size_t size){volatile uint8_t *q=p;while(size--)*q++=0;}
int mttl_config_load(const mttl_settings_io *io,mttl_config_workspace *w,mttl_settings *out){
    size_t size=0;int result;if(!w||!out)return -1;
    result=mttl_settings_load(io,&w->records,w->body,sizeof(w->body),&size);
    if(result==0)mttl_settings_default(out);
    else if(result==1&&!mttl_settings_storage_unpack(w->body,size,out))result=-1;
    clear(w,sizeof(*w));return result;
}
int mttl_config_save(const mttl_settings_io *io,mttl_config_workspace *w,const mttl_settings *s){
    mttl_settings existing;size_t size;int result=-1;
    if(!w||!mttl_settings_valid(s))return -1;
    /* Preserve unknown schema/corrupt evidence rather than silently reset. */
    if(mttl_config_load(io,w,&existing)<0)goto done;
    size=mttl_settings_storage_pack(s,w->body,sizeof(w->body));
    if(size)result=mttl_settings_save(io,&w->records,w->body,size);
done:clear(&existing,sizeof(existing));clear(w,sizeof(*w));return result;
}
