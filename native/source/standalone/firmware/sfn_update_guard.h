#ifndef SFN_UPDATE_GUARD_H
#define SFN_UPDATE_GUARD_H
#include <stdint.h>
#include "sfn_boot_guard_core.h"
typedef struct {
 uint32_t previous_slot,candidate_slot,metadata_sector,backup_sector,started_ms;
 uint8_t armed,trusted,available,current_is_candidate;
} sfn_update_guard_state;
enum {SFN_UPDATE_GUARD_NONE=0,SFN_UPDATE_GUARD_ARMED=1,SFN_UPDATE_GUARD_TRUSTED=2,SFN_UPDATE_GUARD_ROLLBACK_READY=3,SFN_UPDATE_GUARD_FAILED=-1};
int sfn_update_guard_locate_current(const sfn_boot_guard_descriptor *,const sfn_boot_guard_io *,uint32_t *current_slot);
int sfn_update_guard_prepare(const sfn_boot_guard_descriptor *,const sfn_boot_guard_io *,uint32_t previous_slot,uint32_t candidate_slot);
int sfn_update_guard_adopt_first_flash(const sfn_boot_guard_descriptor *,const sfn_boot_guard_io *,uint32_t candidate_slot);
int sfn_update_guard_start(sfn_update_guard_state *,const sfn_boot_guard_descriptor *,const sfn_boot_guard_io *,uint32_t current_slot,uint32_t now_ms);
int sfn_update_guard_mark_trusted(sfn_update_guard_state *,const sfn_boot_guard_descriptor *,const sfn_boot_guard_io *);
int sfn_update_guard_should_timeout(const sfn_update_guard_state *,uint32_t now_ms,uint32_t timeout_ms);
int sfn_update_guard_rollback(sfn_update_guard_state *,const sfn_boot_guard_descriptor *,const sfn_boot_guard_io *);
#endif
