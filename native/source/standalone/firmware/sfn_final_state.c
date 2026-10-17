#include "sfn_final_state.h"
#include <string.h>

void sfn_final_state_init(sfn_final_state *s){if(s)memset(s,0,sizeof(*s));}
int sfn_final_state_reserve_remote(sfn_final_state *s){
    if(!s||s->remote_outstanding>=SFN_FINAL_STATE_CAP)return 0;
    ++s->remote_outstanding;return 1;
}
void sfn_final_state_cancel_remote(sfn_final_state *s){if(s&&s->remote_outstanding)--s->remote_outstanding;}
int sfn_final_state_complete_remote(sfn_final_state *s,uint32_t epoch,unsigned channel,int on,int success){
    unsigned tail;
    if(!s||channel<1||channel>4||on<0||on>1||!s->remote_outstanding)return 0;
    if(!success){--s->remote_outstanding;return 0;}
    if(s->count>=SFN_FINAL_STATE_CAP){--s->remote_outstanding;return 0;}
    tail=(s->head+s->count)%SFN_FINAL_STATE_CAP;
    s->queue[tail].epoch=epoch;s->queue[tail].channel=(uint8_t)channel;s->queue[tail].on=(uint8_t)on;++s->count;return 1;
}
int sfn_final_state_peek_current(sfn_final_state *s,uint32_t epoch,sfn_final_state_ack *ack){
    if(!s||!ack)return 0;
    while(s->count&&s->queue[s->head].epoch!=epoch){
        s->head=(uint8_t)((s->head+1u)%SFN_FINAL_STATE_CAP);--s->count;if(s->remote_outstanding)--s->remote_outstanding;
    }
    if(!s->count)return 0;
    *ack=s->queue[s->head];return 1;
}
void sfn_final_state_consume(sfn_final_state *s){
    if(!s||!s->count)return;
    s->head=(uint8_t)((s->head+1u)%SFN_FINAL_STATE_CAP);--s->count;if(s->remote_outstanding)--s->remote_outstanding;
}
unsigned sfn_final_state_outstanding(const sfn_final_state *s){return s?s->remote_outstanding:0;}
unsigned sfn_final_state_pending(const sfn_final_state *s){return s?s->count:0;}
