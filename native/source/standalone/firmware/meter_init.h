#ifndef MTTL_METER_INIT_H
#define MTTL_METER_INIT_H
#include <stdint.h>
#include <stddef.h>
/* Image-derived configuration; no claim of full per-device calibration.
 * Not linked to physical GPIO. Returns success only if every write succeeds. */
typedef int (*mttl_meter_write)(void *,unsigned,uint8_t,const uint8_t *,size_t);
int mttl_meter_initialize(unsigned channel,mttl_meter_write,void *);
#endif
