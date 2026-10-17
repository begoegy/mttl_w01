#ifndef MTTL_HARDWARE_PORT_H
#define MTTL_HARDWARE_PORT_H
#include "core.h"
/* Four staggered polls per 10-second channel cycle; allow scheduling margin. */
#define MTTL_SENSOR_MAX_AGE_MS 30000u
typedef struct { uint8_t shadow[4],verified,active;void *context;int (*write)(void *,const uint8_t *,size_t); } mttl_hardware_port;
/* Output-byte model. verified enables the explicitly opt-in SDK trial adapter;
 * it denotes adapter readiness, NOT electrical safety/contact verification.
 * Default profiles leave it zero. No GPIO/ROM calls in this module. */
void mttl_hardware_init(mttl_hardware_port *);
int mttl_hardware_begin(void *,uint8_t channel,uint8_t on);
int mttl_hardware_end(void *,uint8_t channel);
/* LED-only frame: all coil bits cleared; unknown channels remain dark. */
int mttl_hardware_indicators(mttl_hardware_port *,uint8_t known,uint8_t on);
typedef struct { mttl_sample power[4],meter[4],current[4],temperature[4],voltage,total_power,total_meter,total_current; } mttl_hardware_samples;
/* Invalid register read never turns into a fabricated zero sample. */
void mttl_hardware_sample(mttl_sample *,int read_ok,int32_t value,uint32_t now);
/* Normalized pressed input only; pin/polarity/bus adapter is not implemented.
 * Must observe a stable release before accepting a press (including after
 * failed reads). debounce_ms must be nonzero and less than 2^31. */
typedef struct { mttl_button button;uint32_t changed_at,debounce_ms;uint8_t initialized,candidate,stable,armed,hold_notified; } mttl_button_input;
void mttl_button_input_init(mttl_button_input *,uint32_t debounce_ms);
int mttl_button_input_poll(mttl_button_input *,int read_ok,uint8_t pressed,uint32_t now);
/* Main button only: event 2 once while held >=10s, then suppress release.
 * Channel callers retain the existing release-based input API. No GPIO IO. */
int mttl_main_button_input_poll(mttl_button_input *,int read_ok,uint8_t pressed,uint32_t now);
#endif
