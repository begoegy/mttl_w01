#ifndef SFN_CHILD_LOCK_H
#define SFN_CHILD_LOCK_H
#include <stdint.h>
#define SFN_CHILD_LOCK_MASK_MAX 31u
int sfn_child_lock_valid(unsigned mask);
int sfn_child_lock_button_locked(unsigned mask,unsigned button_index);
int sfn_child_lock_event_allowed(unsigned mask,unsigned button_index,unsigned event);
#endif
