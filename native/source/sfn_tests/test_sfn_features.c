#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../standalone/firmware/sfn_voltage_guard.h"
#include "../standalone/firmware/sfn_child_lock.h"

static void voltage_tests(void){
    sfn_voltage_guard_config c; sfn_voltage_guard_state s; unsigned e,i; uint32_t t;
    sfn_voltage_guard_default(&c); assert(sfn_voltage_guard_valid(&c)); sfn_voltage_guard_init(&s);
    assert(c.trip_hold_ds==100&&c.recovery_seconds==20&&c.protected_mask==15);
    assert(!c.enabled&&sfn_voltage_guard_step(&s,&c,1,1000,1000)==0&&!s.fault);

    c.enabled=1;
    /* Ten-second low-voltage hold: 9.999 s is not enough; 10.000 s is. */
    assert(sfn_voltage_guard_step(&s,&c,1,1799,1000)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,1799,10999)==0&&!s.fault);
    e=sfn_voltage_guard_step(&s,&c,1,1799,11000);
    assert(e==SFN_VOLTAGE_EVENT_TRIPPED_LOW&&s.fault);
    assert(!sfn_voltage_guard_allows_on(&s));
    assert(sfn_voltage_guard_blocked_mask(&s,&c)==15);

    /* Invalid samples do not clear an existing fault. Recovery needs 20 s continuously in-band. */
    assert(sfn_voltage_guard_step(&s,&c,0,2200,12000)==0&&s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,1899,13000)==0&&s.fault); /* below 190 V recovery floor */
    assert(sfn_voltage_guard_step(&s,&c,1,2200,20000)==0&&s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,2200,39999)==0&&s.fault);
    e=sfn_voltage_guard_step(&s,&c,1,2200,40000);
    assert(e==SFN_VOLTAGE_EVENT_RECOVERED&&!s.fault);
    assert(sfn_voltage_guard_blocked_mask(&s,&c)==0);

    /* High-voltage path and strict threshold semantics. */
    assert(sfn_voltage_guard_step(&s,&c,1,2500,50000)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,2501,51000)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,2501,60999)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,2501,61000)==SFN_VOLTAGE_EVENT_TRIPPED_HIGH&&s.fault);
    c.enabled=0;
    assert(sfn_voltage_guard_step(&s,&c,1,2200,62000)==SFN_VOLTAGE_EVENT_RECOVERED&&!s.fault);

    /* Changing low -> high reason restarts the hold timer instead of inheriting elapsed time. */
    sfn_voltage_guard_init(&s);c.enabled=1;
    assert(sfn_voltage_guard_step(&s,&c,1,1700,1000)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,2600,6000)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,2600,15999)==0&&!s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,2600,16000)==SFN_VOLTAGE_EVENT_TRIPPED_HIGH&&s.fault);

    /* Invalid sample resets a pending (not-yet-tripped) hold. */
    sfn_voltage_guard_init(&s);
    assert(sfn_voltage_guard_step(&s,&c,1,1700,1000)==0);
    assert(sfn_voltage_guard_step(&s,&c,0,0,9000)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,1700,10000)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,1700,19999)==0&&!s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,1700,20000)==SFN_VOLTAGE_EVENT_TRIPPED_LOW&&s.fault);

    /* Protected mask is independent of the fault latch. */
    c.protected_mask=0x05;assert(sfn_voltage_guard_blocked_mask(&s,&c)==0x05);
    c.enabled=0;(void)sfn_voltage_guard_step(&s,&c,1,2200,21000);assert(!s.fault);

    /* Validation rejects promises faster than the physically unverified ~10 s meter cadence. */
    sfn_voltage_guard_default(&c);c.enabled=1;
    c.trip_hold_ds=99;assert(!sfn_voltage_guard_valid(&c));
    c.trip_hold_ds=100;c.recovery_seconds=9;assert(!sfn_voltage_guard_valid(&c));
    c.recovery_seconds=10;c.protected_mask=0;assert(!sfn_voltage_guard_valid(&c));
    c.protected_mask=16;assert(!sfn_voltage_guard_valid(&c));
    c.protected_mask=15;c.hysteresis_dv=0;assert(!sfn_voltage_guard_valid(&c));
    sfn_voltage_guard_default(&c);c.enabled=1;

    /* uint32 millisecond wrap must not break elapsed-time comparisons. */
    sfn_voltage_guard_init(&s);t=0xffffff00u;
    assert(sfn_voltage_guard_step(&s,&c,1,1700,t)==0);
    assert(sfn_voltage_guard_step(&s,&c,1,1700,t+9999u)==0&&!s.fault);
    assert(sfn_voltage_guard_step(&s,&c,1,1700,t+10000u)==SFN_VOLTAGE_EVENT_TRIPPED_LOW&&s.fault);

    /* Deterministic stress: mixed samples, wrap-safe monotonic timestamps, state invariants. */
    {uint32_t x=0x5f3759dfu,now=0xffff0000u;sfn_voltage_guard_init(&s);
     for(i=0;i<200000;i++){
        int valid;int32_t v;int before=s.fault;
        x=x*1664525u+1013904223u;now+=(x&0x7ffu)+1u;valid=(x&31u)!=0;v=1400+(int32_t)((x>>8)%1401u);
        e=sfn_voltage_guard_step(&s,&c,valid,v,now);
        assert(sfn_voltage_guard_allows_on(&s)==!s.fault);
        assert(sfn_voltage_guard_blocked_mask(&s,&c)==(s.fault?c.protected_mask:0));
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
int main(void){voltage_tests();child_lock_tests();puts("SFN_NATIVE_FEATURE_TIME_GUARD_TESTS=PASS");return 0;}
