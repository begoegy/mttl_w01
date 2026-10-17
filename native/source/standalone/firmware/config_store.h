#ifndef MTTL_CONFIG_STORE_H
#define MTTL_CONFIG_STORE_H
#include "settings.h"
#include "settings_store.h"
typedef struct { mttl_settings_workspace records;uint8_t body[MTTL_SETTINGS_BODY_MAX]; } mttl_config_workspace;
/* 1 loaded/saved, 0 defaults for genuinely erased storage, -1 failure.
 * Credentials exist in caller output; caller must protect and clear it.
 * Single worker, independent logical slots; still no hardware driver. */
int mttl_config_load(const mttl_settings_io *,mttl_config_workspace *,mttl_settings *);
int mttl_config_save(const mttl_settings_io *,mttl_config_workspace *,const mttl_settings *);
#endif
