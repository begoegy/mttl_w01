/* SFN native runtime heap profile. This is independent of OTA support. */
#include "FreeRTOS.h"
#undef configTOTAL_HEAP_SIZE
#ifdef MTTL_SENSOR_TEST
#define configTOTAL_HEAP_SIZE (39 * 1024)
#else
#define configTOTAL_HEAP_SIZE (48 * 1024)
#endif
#include "component/os/freertos/freertos_v8.1.2/Source/portable/MemMang/heap_5.c"
