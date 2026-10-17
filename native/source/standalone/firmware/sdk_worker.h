#ifndef MTTL_SDK_WORKER_H
#define MTTL_SDK_WORKER_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t state,ip,uptime_ms; int32_t rssi; uint8_t rssi_valid; char ssid[33]; char mac[18]; uint64_t unix_ms,connected_unix_ms; uint8_t time_valid,meter_initialized,meter_reads,energy_reads; uint32_t energy_raw[4]; uint8_t energy_restore_pending; uint32_t heap_free_bytes,heap_min_free_bytes,worker_observed_stack_descent_bytes; uint32_t energy_save_count,energy_save_failures; uint8_t energy_saved_mask,energy_loaded_mask,hardware_ready; } mttl_worker_status;
#ifdef __cplusplus
extern "C" {
#endif
int mttl_sdk_worker_start(void); /* Started by opt-in test setup; offline default stays idle. */
void mttl_sdk_watchdog_tick(void);
void mttl_sdk_watchdog_http(int active);
int mttl_sdk_settings_persistent(void);
/* HTTP task requests owner-task checkpoint and waits; held until release. */
int mttl_sdk_energy_barrier(void);
void mttl_sdk_energy_release(void);
int mttl_sdk_wifi_reserve(const char *ssid,const char *password);
void mttl_sdk_wifi_release(int accepted); /* after response close */
int mttl_sdk_scan_request(void);
int mttl_sdk_scan_queued(void);
int mttl_sdk_scan_failed(void);
void mttl_sdk_status_snapshot(mttl_worker_status *status);
size_t mttl_sdk_device_json(char *,size_t,int settings);
int mttl_sdk_config_reserve(const uint8_t *,size_t,int keep_password);
void mttl_sdk_config_release(int accepted);
int mttl_sdk_control_request(unsigned channel,unsigned on);
void mttl_sdk_control_timing(uint32_t out[4]);
uint32_t mttl_sdk_relay_snapshot(void);
void mttl_sdk_buttons_poll(void);
void mttl_sdk_button_event(unsigned index,int event);
#ifdef __cplusplus
}
#endif
#endif
