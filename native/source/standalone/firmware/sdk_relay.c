#include "sdk_relay.h"
#include "meter_bus.h"
#include "gpio_api.h"
#include "wait_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#ifdef MTTL_OTA_TEST
#include "ota_test_config.h"
#endif
#ifdef MTTL_RELAY_TEST
static gpio_t clock_pin,data_pin;
static uint8_t fault;
static uint8_t neutral_valid,neutral[4];
/* This owner alone drives PB2/PB3. Preserve open-drain edges without
 * reconfiguring direction/writing zero for an already identical level. */
static uint8_t released_clock,released_data;
static gpio_t *pin(uint8_t n){return n==0x12?&clock_pin:n==0x13?&data_pin:0;}
/* SDK GPIO initialization is required once for mux/pulls. Its repeated
 * direction API re-reads eFuse. Change only this pin's DR/DDR afterwards.
 * Input = open-drain release; output latch is always zero before driving.
 * Short critical RMW preserves other tasks' pins on the same GPIO port. */
static __attribute__((noinline)) int edge(gpio_t *g,int release){
 volatile uint32_t *data,*dir;uint32_t mask;
 if(g->hal_port_num>2||g->hal_pin_num>31)return 0;
 data=(volatile uint32_t *)(0x40001000u+12u*g->hal_port_num);dir=data+1;mask=1u<<g->hal_pin_num;
 taskENTER_CRITICAL();
 if(release)*dir&=~mask;
 else{*data&=~mask;*dir|=mask;}
 taskEXIT_CRITICAL();return 1;
}
static int line(void *p,uint8_t n,int release){gpio_t *g=pin(n);(void)p;if(!g)return 0;
 uint8_t *state=n==0x12?&released_clock:&released_data;if(*state==(uint8_t)release)return 1;
 if(!edge(g,release))return 0;
 *state=(uint8_t)release;
 return 1;}
static int sense(void *p,uint8_t n){gpio_t *g=pin(n);(void)p;return g?gpio_read(g):-1;}
static void delay(void *p){(void)p;wait_us(5);}
static mttl_meter_bus bus={0,line,sense,delay,3};
/* Dedicated relay owner. Complete release BEFORE returning to its caller.
 * Storage interlock prevents flash operations across coil pulses.
 * 100 ms is a trial OEM-equivalent pulse, not a certified maximum. */
static int write_output(void *p,const uint8_t *bytes,size_t n){
 uint8_t released[4];unsigned i;int started,ended=0,coil;(void)p;
 if(n!=4)return 0;coil=(bytes[1]&0x30)||(bytes[2]&0x3f);
 if(coil&&fault)return 0;
 /* Only elide an identical neutral frame already ACKed by this owner.
  * Never elide a coil start or a release following uncertain output. */
 if(!coil&&!fault&&neutral_valid&&!memcmp(neutral,bytes,4))return 1;
 neutral_valid=0;
 started=mttl_output_bus_write(&bus,bytes,n);
 if(!coil){if(!started)fault=1;else{memcpy(neutral,bytes,4);neutral_valid=1;}return started;}
 if(started)vTaskDelay(100/portTICK_RATE_MS+1);
 /* Also clear after a partially ACKed start: failure does not prove no output. */
 memcpy(released,bytes,4);released[1]&=0xcf;released[2]&=0xc0;
 for(i=0;i<3&&!ended;i++)ended=mttl_output_bus_write(&bus,released,4);
 if(!started||!ended)fault=1;
 else{memcpy(neutral,released,4);neutral_valid=1;}
 return started&&ended;
}
#endif
void mttl_sdk_relay_init(mttl_hardware_port *p){
#ifdef MTTL_RELAY_TEST
 unsigned i;uint8_t zero[4]={0};gpio_t *g[2]={&clock_pin,&data_pin};
 fault=neutral_valid=0;
 gpio_init(g[0],(PinName)0x12);gpio_init(g[1],(PinName)0x13);
 for(i=0;i<2;i++){gpio_mode(g[i],PullNone);gpio_dir(g[i],PIN_INPUT);}
 released_clock=released_data=1;
 /* Neutral coil release only; no startup ON/OFF or guessed contact state. */
 /* OEM 1001A78C/1001A796: register 00, 19 FF bytes, BEFORE output 13.
  * Config registers 00..12 exclude output registers 13..16. */
 if(mttl_output_bus_initialize(&bus)&&mttl_output_bus_write(&bus,zero,4)){memcpy(neutral,zero,4);neutral_valid=1;p->context=0;p->write=write_output;p->verified=1;}
#else
 (void)p;
#endif
}
