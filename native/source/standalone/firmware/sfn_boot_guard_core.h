#ifndef SFN_BOOT_GUARD_CORE_H
#define SFN_BOOT_GUARD_CORE_H
#include <stddef.h>
#include <stdint.h>
#include "ota_guard.h"
typedef struct {
 void *context;
 int (*read)(void *,uint32_t,uint8_t *,size_t);
 int (*erase_sector)(void *,uint32_t);
 int (*write)(void *,uint32_t,const uint8_t *,size_t);
} sfn_boot_guard_io;
typedef struct {
 uint32_t first,second,guard_sector,flash_size;
 uint32_t rescue_bytes,marker_offset;
 const uint8_t *marker;size_t marker_size;
 const uint8_t *rescue_sector0;
 const uint8_t *rescue_sha256;
 const uint8_t *rescue_tail_sha256;
} sfn_boot_guard_descriptor;
typedef struct {
 uint32_t native_slot,rescue_slot,started_ms;
 uint8_t armed,trusted,available;
} sfn_boot_guard_state;
enum {SFN_BOOT_GUARD_FAILED=-1,SFN_BOOT_GUARD_UNAVAILABLE=0,SFN_BOOT_GUARD_ARMED=1,SFN_BOOT_GUARD_TRUSTED=2,SFN_BOOT_GUARD_ROLLBACK_READY=3};
int sfn_boot_guard_start(sfn_boot_guard_state *,const sfn_boot_guard_descriptor *,const sfn_boot_guard_io *,uint32_t now_ms);
int sfn_boot_guard_mark_trusted(sfn_boot_guard_state *,const sfn_boot_guard_descriptor *,const sfn_boot_guard_io *);
int sfn_boot_guard_should_timeout(const sfn_boot_guard_state *,uint32_t now_ms,uint32_t timeout_ms);
int sfn_boot_guard_restore(sfn_boot_guard_state *,const sfn_boot_guard_descriptor *,const sfn_boot_guard_io *);
#endif
