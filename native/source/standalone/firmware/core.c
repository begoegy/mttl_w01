#include "core.h"
#include <limits.h>

static void clear_bytes(void *ptr, size_t size) {
    uint8_t *p = ptr;
    while (size--) *p++ = 0;
}
static int due(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

void mttl_control_init(mttl_control *c, uint32_t pulse_ms) {
    clear_bytes(c, sizeof(*c));
    c->pulse_ms = pulse_ms;
    if (!pulse_ms || pulse_ms > 1000) c->fault = MTTL_START_FAULT;
}
int mttl_request(mttl_control *c, uint8_t channel, uint8_t on) {
    uint8_t i, needed = channel == MTTL_ALL ? MTTL_CHANNELS : 1;
    if (channel > MTTL_ALL || on > 1 || c->fault || needed > MTTL_QUEUE_CAP-c->count) return 0;
    for (i = 0; i < needed; ++i) {
        uint8_t tail = (c->head + c->count) % MTTL_QUEUE_CAP;
        c->queue[tail].channel = channel == MTTL_ALL ? i : channel;
        c->queue[tail].on = on;
        ++c->count;
    }
    return 1;
}
void mttl_control_tick(mttl_control *c, const mttl_output *output, uint32_t now) {
    mttl_command cmd;
    if (c->fault) return;
    if (c->active) {
        if (!due(now, c->deadline)) return;
        if (!output->end(output->context, c->active_channel)) {
            c->known_mask &= ~(1u << c->active_channel);
            c->fault = MTTL_END_FAULT;
            c->count = 0;
            return;
        }
        c->known_mask |= 1u << c->active_channel;
        if (c->active_on) c->on_mask |= 1u << c->active_channel;
        else c->on_mask &= ~(1u << c->active_channel);
        c->active = 0;
    }
    if (!c->count) return;
    cmd = c->queue[c->head];
    c->head = (c->head+1) % MTTL_QUEUE_CAP;
    --c->count;
    if (!output->begin(output->context, cmd.channel, cmd.on)) {
        c->known_mask &= ~(1u << cmd.channel);
        output->end(output->context, cmd.channel);
        c->fault = MTTL_START_FAULT;
        c->count = 0;
        return;
    }
    c->active = 1;
    c->active_channel = cmd.channel;
    c->active_on = cmd.on;
    c->deadline = now+c->pulse_ms;
}
int mttl_button_edge(mttl_button *b, uint8_t down, uint32_t now) {
    if (down) {
        if (!b->down) b->pressed_at = now;
        b->down = 1;
        return 0;
    }
    if (!b->down) return 0;
    b->down = 0;
    return now-b->pressed_at >= 10000 ? 2 : 1;
}
void mttl_sample_update(mttl_sample *s, int32_t value, uint8_t valid, uint32_t now) {
    s->valid = valid != 0;
    if (valid) { s->value = value; s->at = now; }
}
int mttl_sample_read(const mttl_sample *s, uint32_t now, uint32_t stale_ms, int32_t *value) {
    if (!s->valid || now-s->at > stale_ms) return 0;
    *value = s->value;
    return 1;
}
int mttl_current_sum(const mttl_sample channels[4], uint32_t now, uint32_t stale_ms, int32_t *value) {
    int32_t total = 0, current;
    uint8_t i;
    for (i = 0; i < 4; ++i) {
        if (!mttl_sample_read(&channels[i], now, stale_ms, &current) || current < 0 || current > INT32_MAX-total) return 0;
        total += current;
    }
    *value = total;
    return 1;
}

int mttl_parse_publish(const uint8_t *frame, size_t size, mttl_publish *view) {
    uint32_t remaining = 0, factor = 1;
    size_t pos = 1, length, i;
    uint8_t byte = 0, count;
    clear_bytes(view, sizeof(*view));
    if (size < 2 || size > MTTL_FRAME_CAP || (frame[0] != 0x30 && frame[0] != 0x31)) return 0;
    for (count = 0; count < 4; ++count) {
        if (pos >= size) return 0;
        byte = frame[pos++];
        remaining += (byte & 127) * factor;
        if (!(byte & 128)) {
            if (count && !byte) return 0;
            break;
        }
        factor *= 128;
    }
    if ((byte & 128) || remaining != size-pos || remaining < 2) return 0;
    length = ((size_t)frame[pos] << 8) | frame[pos+1];
    pos += 2;
    if (!length || length > size-pos) return 0;
    /* This initial C adapter uses printable ASCII topics only. Python model
     * also permits UTF-8. Topic text itself is never copied to a stack array. */
    for (i = 0; i < length; ++i)
        if (frame[pos+i] < 0x20 || frame[pos+i] > 0x7e || frame[pos+i] == '+' || frame[pos+i] == '#') return 0;
    view->topic = frame+pos;
    view->topic_size = length;
    view->payload = frame+pos+length;
    view->payload_size = size-pos-length;
    view->retained = frame[0] & 1;
    return 1;
}
int mttl_content_length(const uint8_t *digits, size_t size, uint16_t *value) {
    uint32_t v = 0;
    size_t i;
    if (!size || size > 4) return 0;
    for (i = 0; i < size; ++i) {
        if (digits[i] < '0' || digits[i] > '9') return 0;
        v = v*10 + digits[i]-'0';
    }
    if (v > 1024) return 0;
    *value = v;
    return 1;
}
int mttl_send_init(mttl_send *s, uint16_t total, uint32_t now, uint32_t timeout) {
    if (!total || total > 2048 || !timeout || timeout >= 0x80000000u) return 0;
    s->total = total; s->sent = 0; s->deadline = now+timeout;
    return 1;
}
int mttl_send_progress(mttl_send *s, int32_t actual, uint32_t now) {
    if (s->sent == s->total) return 1;
    if (due(now, s->deadline) || actual < 0 || (uint32_t)actual > (uint32_t)(s->total-s->sent)) return -1;
    s->sent += actual;
    return s->sent == s->total ? 1 : 0;
}
static void clear_credentials(mttl_network *n) {
    clear_bytes(n->ssid, sizeof(n->ssid));
    clear_bytes(n->password, sizeof(n->password));
}
void mttl_network_init(mttl_network *n) { clear_bytes(n, sizeof(*n)); }
void mttl_scan_begin(mttl_scan *s, uint32_t generation) {
    clear_bytes(s, sizeof(*s)); s->generation = generation; s->pending = 1;
}
int mttl_scan_add(mttl_scan *s, uint32_t generation, const uint8_t *ssid,
                  size_t length, int32_t rssi, uint32_t security) {
    size_t i;
    mttl_access_point *entry;
    if (!s->pending || generation != s->generation || !ssid || !length || length > 32) return 0;
    for (i = 0; i < length; ++i) if (!ssid[i]) return 0;
    if (s->count == MTTL_SCAN_CAP) { s->truncated = 1; return 0; }
    entry = &s->entries[s->count++];
    for (i = 0; i < length; ++i) entry->ssid[i] = (char)ssid[i];
    entry->ssid[length] = 0; entry->rssi = rssi; entry->security = security;
    return 1;
}
int mttl_scan_finish(mttl_scan *s, uint32_t generation) {
    if (!s->pending || generation != s->generation) return 0;
    s->pending = 0; return 1;
}
uint32_t mttl_provision(mttl_network *n, uint32_t now) {
    if (n->state == MTTL_NET_STOPPING) return n->generation;
    ++n->generation;
    clear_credentials(n);
    n->ip = 0;
    n->state = MTTL_NET_STOPPING;
    n->deadline = now+5000;
    return n->generation;
}
void mttl_network_tick(mttl_network *n, uint32_t now) {
    if (n->state != MTTL_NET_STOPPING && n->state != MTTL_NET_JOINING && n->state != MTTL_NET_DHCP) return;
    if (!due(now, n->deadline)) return;
    if (n->state == MTTL_NET_STOPPING) n->state = MTTL_NET_FAULT;
    else mttl_provision(n, now);
}
int mttl_stopped(mttl_network *n, uint32_t generation, uint32_t now) {
    mttl_network_tick(n, now);
    if (n->state != MTTL_NET_STOPPING || generation != n->generation) return 0;
    n->state = MTTL_NET_AP;
    return 1;
}
int mttl_wifi_submit(mttl_network *n, const uint8_t *ssid, size_t ssid_size,
                    const uint8_t *password, size_t password_size, uint32_t now) {
    size_t i;
    if ((n->state != MTTL_NET_AP && n->state != MTTL_NET_ONLINE) || !ssid_size || ssid_size > 32 || password_size > 64) return 0;
    for (i = 0; i < ssid_size; ++i) if (!ssid[i]) return 0;
    for (i = 0; i < password_size; ++i) if (!password[i]) return 0;
    clear_credentials(n);
    for (i = 0; i < ssid_size; ++i) n->ssid[i] = ssid[i];
    for (i = 0; i < password_size; ++i) n->password[i] = password[i];
    ++n->generation;
    n->state = MTTL_NET_JOINING;
    n->deadline = now+15000;
    return 1;
}
int mttl_associated(mttl_network *n, uint32_t generation, uint32_t now) {
    mttl_network_tick(n, now);
    if (n->state != MTTL_NET_JOINING || generation != n->generation) return 0;
    n->state = MTTL_NET_DHCP;
    n->deadline = now+15000;
    return 1;
}
int mttl_got_ip(mttl_network *n, uint32_t generation, uint32_t ipv4, uint32_t now,
               mttl_commit_fn commit, void *context) {
    mttl_network_tick(n, now);
    if (n->state != MTTL_NET_DHCP || generation != n->generation) return 0;
    if (!ipv4 || (ipv4 >> 24) == 127 || (ipv4 >> 28) >= 14) return 0;
    if (!commit(context, n->ssid, n->password)) {
        clear_credentials(n); n->state = MTTL_NET_FAULT;
        return 0;
    }
    clear_credentials(n);
    n->state = MTTL_NET_ONLINE;
    n->ip = ipv4; n->connected_at = now;
    return 1;
}
