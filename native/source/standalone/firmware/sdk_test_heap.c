/* Profile-specific RTOS heap with boot RAM protection: sensor builds use
 * 39 KiB; network-only builds use 48 KiB. */
#include "FreeRTOS.h"
#undef configTOTAL_HEAP_SIZE
#ifdef MTTL_SENSOR_TEST
#define configTOTAL_HEAP_SIZE (39 * 1024)
#else
#define configTOTAL_HEAP_SIZE (48 * 1024)
#endif
#include "../../work/reference/ameba-arduino-1/Arduino_package/hardware/system/component/os/freertos/freertos_v8.1.2/Source/portable/MemMang/heap_5.c"
