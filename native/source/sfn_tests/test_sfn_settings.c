#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/settings.h"
#include "../standalone/firmware/settings_record.h"

static void same_sfn(const mttl_settings *a,const mttl_settings *b){
 assert(a->child_lock_mask==b->child_lock_mask);
 assert(!memcmp(&a->voltage_guard,&b->voltage_guard,sizeof(a->voltage_guard)));
}
int main(void){
 mttl_settings a,b,c;uint8_t body[1024],v5[1024],storage[1200],slot[MTTL_SETTINGS_SLOT],out[1200];size_t n,n5,sn,on;uint32_t seq;size_t physical;
 mttl_settings_default(&a);a.child_lock_mask=21;a.voltage_guard.enabled=1;a.voltage_guard.low_trip_dv=1750;a.voltage_guard.high_trip_dv=2550;a.voltage_guard.recover_low_dv=1900;a.voltage_guard.recover_high_dv=2450;a.voltage_guard.trip_samples=3;a.voltage_guard.recover_samples=4;
 a.schedule.timezone=120;a.schedule.rules[0].minute=123;a.schedule.rules[0].mode=129;a.schedule.rules[0].channel=2;a.schedule.rules[0].on=1;
 assert(mttl_settings_valid(&a));n=mttl_settings_pack(&a,body,sizeof(body));assert(n&&body[0]==6);assert(mttl_settings_unpack(body,n,&b));same_sfn(&a,&b);assert(!memcmp(&a.schedule,&b.schedule,sizeof(a.schedule)));

 /* A legacy v5 body is still accepted and gets safe SFN defaults. */
 memcpy(v5,body,n-12);n5=n-12;v5[0]=5;assert(mttl_settings_unpack(v5,n5,&c));assert(c.child_lock_mask==0);assert(c.voltage_guard.enabled==0);assert(!memcmp(&a.schedule,&c.schedule,sizeof(a.schedule)));

 sn=mttl_settings_storage_pack(&a,storage,sizeof(storage));assert(sn&&storage[0]==3&&storage[3]==6);
 memset(slot,0xaa,sizeof(slot));assert(mttl_settings_encode(slot,storage,sn,77));
 /* Atomic storage driver commits last byte after staging/readback. */
 slot[MTTL_SETTINGS_SLOT-1]=0;
 assert(slot[16]==4);assert(!memcmp(slot+1640,"SC01",4));assert(!memcmp(slot+1720,"SFN1",4));
 assert(mttl_settings_validate(slot,&seq,&physical)&&seq==77);
 assert(mttl_settings_extract(slot,out,sizeof(out),&on));assert(mttl_settings_storage_unpack(out,on,&b));same_sfn(&a,&b);assert(!memcmp(&a.schedule,&b.schedule,sizeof(a.schedule)));

 /* Sidecar corruption must invalidate the complete atomic record. */
 slot[1724]^=1;assert(!mttl_settings_validate(slot,&seq,&physical));slot[1724]^=1;assert(mttl_settings_validate(slot,&seq,&physical));

 /* Legacy v5 storage remains extractable with no SFN sidecar. */
 memset(&c,0,sizeof(c));assert(mttl_settings_unpack(v5,n5,&c));sn=mttl_settings_storage_pack(&c,storage,sizeof(storage));assert(sn);storage[3]=5; /* pack currently emits v6; rebuild exact logical v5 envelope below */
 /* Build a v3 storage wrapper around the preserved v5 body and zero energy/relay. */
 storage[0]=3;storage[1]=(uint8_t)n5;storage[2]=(uint8_t)(n5>>8);memcpy(storage+3,v5,n5);memset(storage+3+n5,0,35);storage[n5+36]=0;storage[n5+37]=0;sn=n5+38;
 memset(slot,0xff,sizeof(slot));assert(mttl_settings_encode(slot,storage,sn,78));slot[MTTL_SETTINGS_SLOT-1]=0;assert(slot[16]==4);assert(!memcmp(slot+1640,"SC01",4));assert(slot[1720]==0xff);assert(mttl_settings_extract(slot,out,sizeof(out),&on));assert(mttl_settings_storage_unpack(out,on,&b));assert(b.child_lock_mask==0&&b.voltage_guard.enabled==0);
 puts("SFN_SETTINGS_SIDECAR_TESTS=PASS");return 0;
}
