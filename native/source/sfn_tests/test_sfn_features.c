#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../standalone/firmware/sfn_voltage_guard.h"
#include "../standalone/firmware/sfn_child_lock.h"

static void voltage_tests(void){
    sfn_voltage_guard_config c; sfn_voltage_guard_state s; unsigned e; unsigned i;
    sfn_voltage_guard_default(&c); assert(sfn_voltage_guard_valid(&c)); sfn_voltage_guard_init(&s);
    for(i=0;i<20;i++)assert(sfn_voltage_guard_step(&s,&c,1,1000)==0);
    assert(!s.fault&&sfn_voltage_guard_allows_on(&s));

    c.enabled=1;
    assert(sfn_voltage_guard_step(&s,&c,1,1799)==0);
    assert(!s.fault);
    e=sfn_voltage_guard_step(&s,&c,1,1799); assert(e==SFN_VOLTAGE_EVENT_TRIPPED_LOW); assert(s.fault); assert(!sfn_voltage_guard_allows_on(&s));
    assert(sfn_voltage_guard_step(&s,&c,0,2200)==0&&s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,1899)==0&&s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,2200)==0&&s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,2200)==0&&s.fault);
    e=sfn_voltage_guard_step(&s,&c,1,2200); assert(e==SFN_VOLTAGE_EVENT_RECOVERED); assert(!s.fault);

    assert(sfn_voltage_guard_step(&s,&c,1,2500)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,2501)==0);
    e=sfn_voltage_guard_step(&s,&c,1,2501); assert(e==SFN_VOLTAGE_EVENT_TRIPPED_HIGH); assert(s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,2451)==0&&s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,2450)==0&&s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,2450)==0&&s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,2450)==SFN_VOLTAGE_EVENT_RECOVERED&&!s.fault);

    assert(sfn_voltage_guard_step(&s,&c,1,1700)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,2600)==0); /* reason changed: counter restarts */
    assert(!s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,2600)==SFN_VOLTAGE_EVENT_TRIPPED_HIGH&&s.fault);
    c.enabled=0; assert(sfn_voltage_guard_step(&s,&c,1,2200)==SFN_VOLTAGE_EVENT_RECOVERED); assert(!s.fault);

    c.enabled=1;c.recover_low_dv=c.low_trip_dv;assert(!sfn_voltage_guard_valid(&c));
    sfn_voltage_guard_default(&c);c.enabled=1;

    /* Deterministic stress: transition invariants across 200k mixed valid/invalid samples. */
    {uint32_t x=0x5f3759dfu; sfn_voltage_guard_init(&s);
     for(i=0;i<200000;i++){
        int valid; int32_t v; int before=s.fault;
        x=x*1664525u+1013904223u; valid=(x&31u)!=0; v=1400+(int32_t)((x>>8)%1401u);
        e=sfn_voltage_guard_step(&s,&c,valid,v);
        assert(sfn_voltage_guard_allows_on(&s)==!s.fault);
        if(e==SFN_VOLTAGE_EVENT_TRIPPED_LOW||e==SFN_VOLTAGE_EVENT_TRIPPED_HIGH)assert(s.fault);
        if(e==SFN_VOLTAGE_EVENT_RECOVERED)assert(!s.fault);
        if(before&&!valid)assert(s.fault);
     }}
}

static void child_lock_tests(void){
    unsigned mask,b;
    assert(!sfn_child_lock_valid(32));
    for(mask=0;mask<=31;mask++)for(b=0;b<5;b++){
        int locked=(mask>>b)&1u;
        assert(sfn_child_lock_button_locked(mask,b)==locked);
        assert(sfn_child_lock_event_allowed(mask,b,1)==!locked);
        if(b==0)assert(sfn_child_lock_event_allowed(mask,b,2)==1);
        else assert(sfn_child_lock_event_allowed(mask,b,2)==!locked);
    }
    assert(!sfn_child_lock_event_allowed(0,5,1));
    assert(!sfn_child_lock_event_allowed(0,0,0));
}

int main(void){voltage_tests();child_lock_tests();puts("SFN_NATIVE_FEATURE_TESTS=PASS");return 0;}
