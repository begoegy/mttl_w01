#ifndef SFN_VOLTAGE_POLICY_H
#define SFN_VOLTAGE_POLICY_H
#include <stdint.h>
#include "sfn_voltage_guard.h"
/* Decode the production SFN packed vguard fields into the native durable policy.
 * Native currently rejects auto-restore and hold/recovery promises faster than
 * the physically unverified ~10 s meter cadence. */
int sfn_voltage_policy_decode(uint32_t flags,uint32_t thresholds,uint32_t timing,uint32_t recovery_seconds,sfn_voltage_guard_config *out);
#endif
