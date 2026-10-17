#include "settings_store.h"
static int erased(const uint8_t *s){size_t i;for(i=0;i<MTTL_SETTINGS_SLOT;i++)if(s[i]!=255)return 0;return 1;}
static int read_slots(const mttl_settings_io *io,mttl_settings_workspace *w){
    return io&&w&&io->read&&io->read(io->context,0,0,w->slots[0],MTTL_SETTINGS_SLOT)&&io->read(io->context,1,0,w->slots[1],MTTL_SETTINGS_SLOT);
}
void mttl_settings_workspace_clear(mttl_settings_workspace *w){volatile uint8_t *p=(volatile uint8_t *)w;size_t i;if(w)for(i=0;i<sizeof(*w);i++)p[i]=0;}
int mttl_settings_load(const mttl_settings_io *io,mttl_settings_workspace *w,uint8_t *out,size_t cap,size_t *size){
    int chosen,result=-1;uint32_t seq;size_t n;
    if(!out||!size||!read_slots(io,w))goto done;
    chosen=mttl_settings_choose(w->slots[0],w->slots[1]);
    if(chosen==-1&&erased(w->slots[0])&&erased(w->slots[1])){*size=0;result=0;goto done;}
    if(chosen<0||!mttl_settings_validate(w->slots[chosen],&seq,&n)||n>cap)goto done;
    if(!mttl_settings_extract(w->slots[chosen],out,cap,size))goto done;result=1;
done:mttl_settings_workspace_clear(w);return result;
}
int mttl_settings_save(const mttl_settings_io *io,mttl_settings_workspace *w,const uint8_t *body,size_t size){
    int old,target,result=-1;uint32_t seq=0,verified;size_t n,i;uint8_t commit=0,*staged,*readback;
    if(!body||!size||size>MTTL_SETTINGS_SLOT-17||!io||!io->erase||!io->program||!read_slots(io,w))goto done;
    old=mttl_settings_choose(w->slots[0],w->slots[1]);
    if(old<0){if(old!=-1||!erased(w->slots[0])||!erased(w->slots[1]))goto done;target=0;}
    else{target=1-old;if(!mttl_settings_validate(w->slots[old],&seq,&n))goto done;++seq;}
    staged=w->slots[target];readback=w->slots[1-target];
    if(!mttl_settings_encode(staged,body,size,seq)||!io->erase(io->context,target))goto done;
    if(!io->program(io->context,target,0,staged,MTTL_SETTINGS_SLOT-1)||!io->read(io->context,target,0,readback,MTTL_SETTINGS_SLOT))goto done;
    for(i=0;i<MTTL_SETTINGS_SLOT;i++)if(readback[i]!=staged[i])goto done;
    if(!io->program(io->context,target,MTTL_SETTINGS_SLOT-1,&commit,1)||!io->read(io->context,target,0,readback,MTTL_SETTINGS_SLOT))goto done;
    if(!mttl_settings_validate(readback,&verified,&n)||verified!=seq)goto done;
    for(i=0;i<MTTL_SETTINGS_SLOT-1;i++)if(readback[i]!=staged[i])goto done;
    result=1;
done:mttl_settings_workspace_clear(w);return result;
}
