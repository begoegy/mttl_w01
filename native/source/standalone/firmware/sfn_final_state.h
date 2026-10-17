#ifndef SFN_FINAL_STATE_H
#define SFN_FINAL_STATE_H
#include <stdint.h>

#define SFN_FINAL_STATE_CAP 4u

typedef struct {
    uint32_t epoch;
    uint8_t channel;
    uint8_t on;
} sfn_final_state_ack;

typedef struct {
    sfn_final_state_ack queue[SFN_FINAL_STATE_CAP];
    uint8_t head;
    uint8_t count;
    uint8_t remote_outstanding;
} sfn_final_state;

void sfn_final_state_init(sfn_final_state *state);
int sfn_final_state_reserve_remote(sfn_final_state *state);
void sfn_final_state_cancel_remote(sfn_final_state *state);
int sfn_final_state_complete_remote(sfn_final_state *state,uint32_t epoch,unsigned channel,int on,int success);
int sfn_final_state_peek_current(sfn_final_state *state,uint32_t epoch,sfn_final_state_ack *ack);
void sfn_final_state_consume(sfn_final_state *state);
unsigned sfn_final_state_outstanding(const sfn_final_state *state);
unsigned sfn_final_state_pending(const sfn_final_state *state);

#endif
