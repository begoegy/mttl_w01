#ifndef MTTL_SDK_FLASH_MOCK_H
#define MTTL_SDK_FLASH_MOCK_H
#include <stdint.h>
typedef struct {int unused;} flash_t;
extern flash_t flash;
#define RT_DEV_LOCK_FLASH 1
void device_mutex_lock(int);
void device_mutex_unlock(int);
int flash_stream_read(flash_t *,uint32_t,uint32_t,uint8_t *);
int flash_stream_write(flash_t *,uint32_t,uint32_t,uint8_t *);
void flash_erase_sector(flash_t *,uint32_t);
#endif
