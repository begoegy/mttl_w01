#include "sfn_child_lock.h"
int sfn_child_lock_valid(unsigned mask){return mask<=SFN_CHILD_LOCK_MASK_MAX;}
int sfn_child_lock_button_locked(unsigned mask,unsigned button){return sfn_child_lock_valid(mask)&&button<5?((mask>>button)&1u):1;}
int sfn_child_lock_event_allowed(unsigned mask,unsigned button,unsigned event){
    if(!sfn_child_lock_valid(mask)||button>=5||event==0)return 0;
    /* Preserve master-button long hold for setup/recovery even while short press is locked. */
    if(button==0&&event==2)return 1;
    return !sfn_child_lock_button_locked(mask,button);
}
