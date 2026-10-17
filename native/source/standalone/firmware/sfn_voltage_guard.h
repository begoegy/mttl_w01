#ifndef SFN_VOLTAGE_GUARD_H
#define SFN_VOLTAGE_GUARD_H
#include <stdint.h>

enum {
    SFN_VOLTAGE_EVENT_NONE = 0,
    SFN_VOLTAGE_EVENT_TRIPPED_LOW = 1,
    SFN_VOLTAGE_EVENT_TRIPPED_HIGH = 2,
    SFN_VOLTAGE_EVENT_RECOVERED = 4
};

typedef struct {
    uint16_t low_trip_dv;
    uint16_t high_trip_dv;
    uint16_t recover_low_dv;
    uint16_t recover_high_dv;
    uint8_t enabled;
    uint8_t trip_samples;
    uint8_t recover_samples;
} sfn_voltage_guard_config;

typedef struct {
    uint8_t fault;
    uint8_t trip_count;
    uint8_t recover_count;
    uint8_t pending_reason;
    uint8_t last_trip_reason;
} sfn_voltage_guard_state;

void sfn_voltage_guard_default(sfn_voltage_guard_config *);
int sfn_voltage_guard_valid(const sfn_voltage_guard_config *);
void sfn_voltage_guard_init(sfn_voltage_guard_state *);
unsigned sfn_voltage_guard_step(sfn_voltage_guard_state *, const sfn_voltage_guard_config *, int sample_valid, int32_t voltage_dv);
int sfn_voltage_guard_allows_on(const sfn_voltage_guard_state *);

#endif
