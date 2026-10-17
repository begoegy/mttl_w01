#ifndef MTTL_METER_BUS_H
#define MTTL_METER_BUS_H
#include "meter_probe.h"
#include "meter_init.h"
/* Single owner. line(0) drives low, line(1) releases to input/high-Z.
 * sense returns 0/1 or -1/error; delay is a verified platform delay callback.
 * No SDK GPIO callback installed yet. Never drives the relay/output bus. */
typedef struct {
 void *context;
 int (*line)(void *,uint8_t pin,int release);
 int (*sense)(void *,uint8_t pin);
 void (*delay)(void *);
 unsigned high_attempts;
} mttl_meter_bus;
int mttl_meter_bus_read(void *,unsigned,uint8_t,uint8_t *,size_t);
int mttl_meter_bus_write(void *,unsigned,uint8_t,const uint8_t *,size_t);
/* Explicit opt-in output bus: PB2/PB3, wire address A8, register 13. */
int mttl_output_bus_write(mttl_meter_bus *,const uint8_t *,size_t);
int mttl_output_bus_initialize(mttl_meter_bus *);
#endif
