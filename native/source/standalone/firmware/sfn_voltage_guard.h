#ifndef SFN_VOLTAGE_GUARD_H
#define SFN_VOLTAGE_GUARD_H
#include <stdint.h>

enum {
    SFN_VOLTAGE_EVENT_NONE = 0,
    SFN_VOLTAGE_EVENT_TRIPPED_LOW = 1,
    SFN_VOLTAGE_EVENT_TRIPPED_HIGH = 2,
    SFN_VOLTAGE_EVENT_RECOVERED = 4
};

/* Native MTTL-W01 metering supplies a fresh line-voltage sample about every
 * ten seconds. Keep the policy time-based so a future, physically verified
 * faster sampler can lower the capability floor without changing storage/wire
 * semantics. trip_hold_ds is deciseconds; recovery_seconds is wall duration. */
typedef struct {
    uint16_t low_trip_dv;
    uint16_t high_trip_dv;
    uint16_t hysteresis_dv;
    uint16_t trip_hold_ds;
    uint16_t recovery_seconds;
    uint8_t enabled;
    uint8_t protected_mask;
} sfn_voltage_guard_config;

typedef struct {
    uint32_t pending_since_ms;
    uint32_t recovery_since_ms;
    uint8_t fault;
    uint8_t pending_reason;
    uint8_t last_trip_reason;
    uint8_t pending_active;
    uint8_t recovery_active;
} sfn_voltage_guard_state;

void sfn_voltage_guard_default(sfn_voltage_guard_config *);
int sfn_voltage_guard_valid(const sfn_voltage_guard_config *);
void sfn_voltage_guard_init(sfn_voltage_guard_state *);
unsigned sfn_voltage_guard_step(sfn_voltage_guard_state *,const sfn_voltage_guard_config *,int sample_valid,int32_t voltage_dv,uint32_t sample_at_ms);
int sfn_voltage_guard_allows_on(const sfn_voltage_guard_state *);
uint8_t sfn_voltage_guard_blocked_mask(const sfn_voltage_guard_state *,const sfn_voltage_guard_config *);
#endif
