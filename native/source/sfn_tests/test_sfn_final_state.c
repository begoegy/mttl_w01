#include <assert.h>
#include <stdio.h>
#include "../standalone/firmware/sfn_final_state.h"
#include "../standalone/firmware/sfn_child_lock.h"
#include "../standalone/firmware/sfn_voltage_guard.h"
#include "../standalone/firmware/sfn_voltage_policy.h"

static void single_delayed(void){
    sfn_final_state s; sfn_final_state_ack a;
    sfn_final_state_init(&s);
    assert(sfn_final_state_reserve_remote(&s));
    assert(sfn_final_state_outstanding(&s)==1 && sfn_final_state_pending(&s)==0);
    assert(sfn_final_state_complete_remote(&s,7,1,0,1));
    assert(sfn_final_state_peek_current(&s,7,&a));
    assert(a.channel==1 && a.on==0);
    sfn_final_state_consume(&s);
    assert(!sfn_final_state_outstanding(&s) && !sfn_final_state_pending(&s));
}

static void four_queued(void){
    sfn_final_state s; sfn_final_state_ack a; unsigned i;
    sfn_final_state_init(&s);
    for(i=0;i<4;i++) assert(sfn_final_state_reserve_remote(&s));
    assert(!sfn_final_state_reserve_remote(&s));
    for(i=0;i<4;i++) assert(sfn_final_state_complete_remote(&s,11,i+1,1,1));
    assert(sfn_final_state_pending(&s)==4 && sfn_final_state_outstanding(&s)==4);
    for(i=0;i<4;i++){
        assert(sfn_final_state_peek_current(&s,11,&a));
        assert(a.channel==i+1 && a.on==1);
        sfn_final_state_consume(&s);
    }
    assert(!sfn_final_state_pending(&s) && !sfn_final_state_outstanding(&s));
}

static void fault_has_no_success(void){
    sfn_final_state s; sfn_final_state_ack a;
    sfn_final_state_init(&s); assert(sfn_final_state_reserve_remote(&s));
    assert(!sfn_final_state_complete_remote(&s,3,2,1,0));
    assert(!sfn_final_state_peek_current(&s,3,&a));
    assert(!sfn_final_state_outstanding(&s));
}

static void reconnect_drops_stale(void){
    sfn_final_state s; sfn_final_state_ack a;
    sfn_final_state_init(&s); assert(sfn_final_state_reserve_remote(&s));
    assert(sfn_final_state_complete_remote(&s,20,3,1,1));
    assert(!sfn_final_state_peek_current(&s,21,&a));
    assert(!sfn_final_state_pending(&s) && !sfn_final_state_outstanding(&s));
}

static void rapid_no_overflow(void){
    sfn_final_state s; sfn_final_state_ack a; unsigned round,i;
    sfn_final_state_init(&s);
    for(round=1;round<=2500;round++){
        for(i=0;i<4;i++) assert(sfn_final_state_reserve_remote(&s));
        for(i=0;i<4;i++) assert(sfn_final_state_complete_remote(&s,round,i+1,(round+i)&1u,1));
        assert(!sfn_final_state_reserve_remote(&s));
        for(i=0;i<4;i++){assert(sfn_final_state_peek_current(&s,round,&a));sfn_final_state_consume(&s);}
        assert(!sfn_final_state_pending(&s) && !sfn_final_state_outstanding(&s));
    }
}

static void policy_guards(void){
    sfn_voltage_guard_state state; sfn_voltage_guard_config cfg;
    sfn_voltage_guard_init(&state); sfn_voltage_guard_default(&cfg);
    cfg.enabled=1; cfg.protected_mask=1; cfg.low_trip_dv=1800; cfg.high_trip_dv=2500; cfg.trip_hold_ds=100;
    assert(sfn_voltage_guard_step(&state,&cfg,1,1700,100)==0);
    assert(sfn_voltage_guard_step(&state,&cfg,1,1700,10100)!=0);
    assert(sfn_voltage_guard_blocked_mask(&state,&cfg)&1u);
    /* Child lock blocks physical short presses only; remote final-state admission has no lock-mask input. */
    assert(!sfn_child_lock_event_allowed(31,1,1));
    {sfn_final_state s;sfn_final_state_init(&s);assert(sfn_final_state_reserve_remote(&s));sfn_final_state_cancel_remote(&s);}
}

int main(void){
    single_delayed(); four_queued(); fault_has_no_success(); reconnect_drops_stale(); rapid_no_overflow(); policy_guards();
    puts("SFN_FINAL_STATE_TESTS=PASS"); return 0;
}
