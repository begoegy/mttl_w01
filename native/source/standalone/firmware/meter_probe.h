#ifndef MTTL_METER_PROBE_H
#define MTTL_METER_PROBE_H
#include <stddef.h>
#include <stdint.h>
/* Raw diagnostic registers, NOT calibrated physical-unit sensor values.
 * Single worker owns this object. No writes/initialization/relay callbacks. */
typedef int (*mttl_meter_read)(void *,unsigned channel,uint8_t reg,uint8_t *,size_t);
typedef struct {
    uint32_t temperature_raw,measurement_raw[3],energy_raw,at;
    uint8_t valid_mask;
} mttl_meter_raw;
typedef struct { mttl_meter_raw channels[4];unsigned next;uint32_t polled_at;uint8_t started; } mttl_meter_probe;
void mttl_meter_probe_init(mttl_meter_probe *);
/* One channel per call, minimum 250 ms spacing. Callback success is 1.
 * A failed block becomes invalid immediately; previous raw data retained only
 * for diagnostics. No callback means no IO and no valid fabricated samples. */
int mttl_meter_probe_tick(mttl_meter_probe *,mttl_meter_read,void *,uint32_t now);
int mttl_meter_probe_snapshot(const mttl_meter_probe *,unsigned channel,
                             uint32_t now,uint32_t stale_ms,mttl_meter_raw *);
#endif
