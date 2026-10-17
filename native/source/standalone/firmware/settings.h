#ifndef MTTL_SETTINGS_H
#define MTTL_SETTINGS_H
#include <stddef.h>
#include <stdint.h>
#include "protection.h"
#include "scheduler.h"
#include "sfn_voltage_guard.h"
#include "sfn_child_lock.h"
#include "sfn_schedule_meta.h"
typedef struct {
    char device[65],channel[4][65],ssid[33],wifi_password[65];
    char broker[254],mqtt_user[65],mqtt_password[65],topic[65];
    uint16_t port;
    uint8_t language,mqtt_enabled;
    /* Device-owned checkpoint; excluded from web configuration encoding. */
    uint32_t energy_raw[4],energy_wh[4];
    uint8_t energy_valid;
    uint8_t relay_known,relay_on; /* Last completed command, NOT contact feedback. */
    mttl_protection_config protection;
    mttl_schedule_config schedule;
    sfn_voltage_guard_config voltage_guard;
    uint8_t child_lock_mask;
    uint32_t sfn_backend_ipv4;
    uint16_t sfn_backend_port;
    sfn_schedule_meta sfn_schedule;
} mttl_settings;
/* Versioned length-prefixed encoding, never raw C structure/padding. */
#define MTTL_SETTINGS_BODY_MAX 1024
void mttl_settings_default(mttl_settings *s);
int mttl_settings_valid(const mttl_settings *s);
int mttl_settings_utf8(const char *,size_t capacity);
size_t mttl_settings_pack(const mttl_settings *s,uint8_t *out,size_t capacity);
int mttl_settings_unpack(const uint8_t *body,size_t size,mttl_settings *out);
size_t mttl_settings_storage_pack(const mttl_settings *,uint8_t *,size_t);
int mttl_settings_storage_unpack(const uint8_t *,size_t,mttl_settings *);
void mttl_settings_keep_energy(mttl_settings *,const mttl_settings *);
#endif
