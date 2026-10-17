#ifndef MTTL_METER_CONVERT_H
#define MTTL_METER_CONVERT_H
#include <stdint.h>
/* OEM table result in tenths of degree C. Software equivalence, not a
 * laboratory calibration proof. Failure does not fabricate a zero value. */
int mttl_meter_temperature(uint32_t raw,int32_t *tenths);
typedef struct {int32_t history[4][4],power[4];uint8_t index[4];} mttl_meter_power_state;
typedef struct {uint32_t raw[4],current[4];uint8_t read_mask,on_mask,known_mask,factor;} mttl_meter_power_input;
typedef struct {int32_t power[4];uint32_t current[4];uint8_t power_valid,current_valid;} mttl_meter_power_result;
void mttl_meter_power_init(mttl_meter_power_state *);
int mttl_meter_power_step(mttl_meter_power_state *,const mttl_meter_power_input *,mttl_meter_power_result *);
/* Latest sensor value without averaging: do not force zero using an unknown relay state. Known
 * OFF still gates power; this does not invent or change relay knowledge. */
int mttl_meter_measured_power_step(mttl_meter_power_state *,const mttl_meter_power_input *,mttl_meter_power_result *);
/* Energy remains OEM internal units. Reset request must be acknowledged by a
 * future hardware adapter; no register write happens in these pure functions. */
typedef struct {uint32_t previous,restart,count,base,value;} mttl_meter_energy_state;
void mttl_meter_energy_init(mttl_meter_energy_state *);
int mttl_meter_energy_step(mttl_meter_energy_state *,int read_ok,uint32_t raw,int *reset_requested);
/* Persistent counter planner. A decrease is treated as counter reset and
 * adds the new raw value to the retained total; otherwise add only the delta.
 * Caller owns persistence; save success is never inferred from this result. */
typedef struct {uint32_t raw,value;uint8_t valid;} mttl_meter_checkpoint;
int mttl_meter_checkpoint_step(const mttl_meter_checkpoint *,int read_ok,
                              uint32_t raw,mttl_meter_checkpoint *next);
#endif
