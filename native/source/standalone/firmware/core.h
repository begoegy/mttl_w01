#ifndef MTTL_CORE_H
#define MTTL_CORE_H
#include <stdint.h>
#include <stddef.h>

/* All APIs belong to one worker. ISR/network adapters enqueue external events;
 * they must not call these functions concurrently. Tick intervals < 2^31 ms. */
#define MTTL_CHANNELS 4
#define MTTL_QUEUE_CAP 8
#define MTTL_ALL 4
#define MTTL_FRAME_CAP 512
#define MTTL_SCAN_CAP 16
/* Single-worker cache. SDK callbacks must enqueue/copy under a separate
 * synchronization adapter, not race HTTP readers. No allocations or blocking. */
typedef struct { char ssid[33]; int32_t rssi; uint32_t security; } mttl_access_point;
typedef struct {
    mttl_access_point entries[MTTL_SCAN_CAP];
    uint32_t generation;
    uint8_t count, pending, truncated;
} mttl_scan;
void mttl_scan_begin(mttl_scan *scan, uint32_t generation);
int mttl_scan_add(mttl_scan *scan, uint32_t generation, const uint8_t *ssid,
                  size_t length, int32_t rssi, uint32_t security);
int mttl_scan_finish(mttl_scan *scan, uint32_t generation);

typedef struct { uint8_t channel, on; } mttl_command;
typedef enum { MTTL_OK, MTTL_START_FAULT, MTTL_END_FAULT } mttl_fault;
typedef struct {
    mttl_command queue[MTTL_QUEUE_CAP];
    uint8_t head, count, active, active_channel, active_on;
    uint8_t known_mask, on_mask;
    mttl_fault fault;
    uint32_t deadline, pulse_ms;
} mttl_control;
typedef int (*mttl_begin_fn)(void *, uint8_t, uint8_t);
typedef int (*mttl_end_fn)(void *, uint8_t);
typedef struct { void *context; mttl_begin_fn begin; mttl_end_fn end; } mttl_output;

void mttl_control_init(mttl_control *c, uint32_t pulse_ms);
int mttl_request(mttl_control *c, uint8_t channel, uint8_t on);
void mttl_control_tick(mttl_control *c, const mttl_output *output, uint32_t now);
typedef struct { uint32_t pressed_at; uint8_t down; } mttl_button;
/* Return 0/no event, 1/short release, 2/provision release. */
int mttl_button_edge(mttl_button *button, uint8_t down, uint32_t now);

typedef struct { int32_t value; uint32_t at; uint8_t valid; } mttl_sample;
void mttl_sample_update(mttl_sample *s, int32_t value, uint8_t valid, uint32_t now);
int mttl_sample_read(const mttl_sample *s, uint32_t now, uint32_t stale_ms, int32_t *value);
int mttl_current_sum(const mttl_sample channels[4], uint32_t now, uint32_t stale_ms, int32_t *value);

typedef struct {
    const uint8_t *topic, *payload;
    uint16_t topic_size, payload_size;
    uint8_t retained;
} mttl_publish;
/* Complete QoS0 frames only. Views stay valid only while input buffer lives. */
int mttl_parse_publish(const uint8_t *frame, size_t size, mttl_publish *view);
int mttl_content_length(const uint8_t *digits, size_t size, uint16_t *value);
typedef struct { uint16_t total, sent; uint32_t deadline; } mttl_send;
int mttl_send_init(mttl_send *s, uint16_t total, uint32_t now, uint32_t timeout);
/* Return -1/failure, 0/pending, 1/complete; actual is socket's byte count. */
int mttl_send_progress(mttl_send *s, int32_t actual, uint32_t now);

typedef enum { MTTL_NET_OFF, MTTL_NET_STOPPING, MTTL_NET_AP,
    MTTL_NET_JOINING, MTTL_NET_DHCP, MTTL_NET_ONLINE, MTTL_NET_FAULT } mttl_net_state;
typedef struct {
    mttl_net_state state;
    uint32_t generation, deadline, connected_at, ip;
    char ssid[33], password[65];
} mttl_network;
void mttl_network_init(mttl_network *n);
uint32_t mttl_provision(mttl_network *n, uint32_t now);
void mttl_network_tick(mttl_network *n, uint32_t now);
int mttl_stopped(mttl_network *n, uint32_t generation, uint32_t now);
int mttl_wifi_submit(mttl_network *n, const uint8_t *ssid, size_t ssid_size,
                    const uint8_t *password, size_t password_size, uint32_t now);
int mttl_associated(mttl_network *n, uint32_t generation, uint32_t now);
/* ipv4 is numeric network-order value, e.g. 0xC000020A. Commit adapter must
 * preserve last valid config on failure. Called only after DHCP succeeds. */
typedef int (*mttl_commit_fn)(void *, const char *, const char *);
int mttl_got_ip(mttl_network *n, uint32_t generation, uint32_t ipv4, uint32_t now,
               mttl_commit_fn commit, void *context);
#endif
