#ifndef MTTL_SETTINGS_STORE_H
#define MTTL_SETTINGS_STORE_H
#include "settings_record.h"
typedef struct {
    void *context;
    int (*read)(void *,unsigned,size_t,uint8_t *,size_t);
    int (*erase)(void *,unsigned);
    int (*program)(void *,unsigned,size_t,const uint8_t *,size_t);
} mttl_settings_io;
/* Logical independent slots only. Never supplies a physical flash address.
 * Caller-owned workspace, single worker; do not share between operations. */
/* After choosing the target, its RAM slot is the staged record and the
 * other RAM slot is reusable readback scratch. Old NOR slot stays intact. */
typedef struct { uint8_t slots[2][MTTL_SETTINGS_SLOT]; } mttl_settings_workspace;
/* 1 success, 0 empty on load, -1 I/O/corruption/ambiguity/capacity failure. */
/* A failed save may have committed before final read failed: reload to resolve
 * outcome. Callbacks may not retain buffers. Body/output must not alias workspace.
 * CRC does not provide encryption or authentication. */
int mttl_settings_load(const mttl_settings_io *,mttl_settings_workspace *,uint8_t *,size_t,size_t *);
int mttl_settings_save(const mttl_settings_io *,mttl_settings_workspace *,const uint8_t *,size_t);
void mttl_settings_workspace_clear(mttl_settings_workspace *);
#endif
