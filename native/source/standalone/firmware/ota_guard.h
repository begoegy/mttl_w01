#ifndef MTTL_OTA_GUARD_H
#define MTTL_OTA_GUARD_H
#include <stdint.h>
#include <stddef.h>
typedef struct {uint32_t first,second,protected_start,flash_size;} mttl_ota_layout;
typedef struct {uint32_t target,erase_end,capacity;} mttl_ota_plan;
/* Signature classification only. NOT proof of execution slot or boot order. */
enum {MTTL_OTA_INVALID,MTTL_OTA_EMPTY,MTTL_OTA_NEW,MTTL_OTA_OLD};
int mttl_ota_signature(const uint8_t signature[8]);
int mttl_ota_plan_make(const mttl_ota_layout *,uint32_t active,uint32_t bytes,mttl_ota_plan *);
/* Decoded BIN only; checksum is integrity, not authenticity. SHA256 and
 * target-side readback must be verified separately before any commit. */
int mttl_ota_bin_validate(const uint8_t *,size_t);
/* Prefix structural check only, without payload checksum/hash. */
int mttl_ota_header_validate(const uint8_t *,size_t prefix_bytes,uint32_t file_bytes);
/* Pure action/readback state machine; no IO. Caller must independently prove
 * active slot and complete target image/hash readback before init. */
typedef struct {uint32_t active,target;uint8_t step,failed;} mttl_ota_commit;
int mttl_ota_commit_init(mttl_ota_commit *,const mttl_ota_layout *,uint32_t active,uint32_t target,int image_verified);
int mttl_ota_commit_action(const mttl_ota_commit *,uint32_t *address,uint32_t *value);
int mttl_ota_commit_ack(mttl_ota_commit *,int write_ok,uint32_t readback);
int mttl_ota_commit_done(const mttl_ota_commit *);
#endif
