#ifndef MTTL_DEVICE_APP_H
#define MTTL_DEVICE_APP_H
#include "config_store.h"
#include "hardware_port.h"
#include "mqtt_engine.h"
#include "ntp_client.h"
typedef struct {
    mttl_settings settings;
    mttl_control control;mttl_hardware_port hardware;mttl_hardware_samples samples;
    mttl_mqtt_engine mqtt;mttl_ntp_client ntp;mttl_button main_button,channel_button[4];
    uint32_t revision;uint8_t persistent,provision_requested;
    uint32_t publish_at;uint8_t publish_index;
    int (*relay_request)(unsigned,unsigned); /* Optional external relay owner. */
    uint8_t mqtt_known,mqtt_on,mqtt_all,mqtt_was_online;
    mttl_protection_state protection;
} mttl_device_app;
/* Queued state is not yet transmitted. Keep the socket owner ahead of
 * slow meter/NOR work until channel and aggregate confirmations drain. */
static inline int mttl_device_mqtt_pending(const mttl_device_app *d){
    return d->mqtt.tx_size ||
        (d->control.known_mask&~d->mqtt_known) ||
        ((d->control.on_mask^d->mqtt_on)&d->control.known_mask) ||
        (d->control.known_mask==15&&(!d->mqtt_all||d->mqtt_was_online!=(d->control.on_mask==15)));
}
void mttl_device_init(mttl_device_app *,uint64_t reference_time);
/* Flash workspace supplied only by a future serialized persistence owner. */
int mttl_device_load(mttl_device_app *,const mttl_settings_io *,mttl_config_workspace *);
int mttl_device_configure(mttl_device_app *,const uint8_t *,size_t,const mttl_settings_io *,mttl_config_workspace *);
int mttl_device_command(void *,uint8_t,uint8_t);
void mttl_device_button(mttl_device_app *,unsigned button,int down,uint32_t now);
void mttl_device_tick(mttl_device_app *,const char *id,int network,uint32_t now,const mttl_mqtt_io *);
/* JSON projection includes no password. Raw sample contract:
 * power mW, meter Wh, current mA, temperature/voltage tenths. */
size_t mttl_device_status(const mttl_device_app *,uint32_t now,char *,size_t);
size_t mttl_device_settings_json(const mttl_device_app *,char *,size_t);
size_t mttl_settings_json(const mttl_settings *,int persistent,char *,size_t);
#endif
