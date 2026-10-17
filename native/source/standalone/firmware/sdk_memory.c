#include "sdk_memory.h"
#include "FreeRTOS.h"
#include "task.h"
/* Linked SDK exports this even though its config omits the INCLUDE macro. */
extern TaskHandle_t xTaskGetCurrentTaskHandle(void);
static TaskHandle_t owner;
static uint32_t reference,lowest;
static uint32_t stack_pointer(void){uint32_t value;__asm volatile("mov %0, sp":"=r"(value));return value;}
void mttl_sdk_memory_init(void){owner=xTaskGetCurrentTaskHandle();reference=lowest=stack_pointer();}
void mttl_sdk_memory_sample(void){uint32_t sp;if(!owner||xTaskGetCurrentTaskHandle()!=owner)return;sp=stack_pointer();if(sp<lowest)lowest=sp;}
uint32_t mttl_sdk_memory_stack_descent(void){return reference>=lowest?reference-lowest:0;}
