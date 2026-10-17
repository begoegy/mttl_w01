#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/settings.h"
#include "../standalone/firmware/settings_record.h"

static void same_features(const mttl_settings *a,const mttl_settings *b){
 assert(a->child_lock_mask==b->child_lock_mask);
 assert(!memcmp(&a->voltage_guard,&b->voltage_guard,sizeof(a->voltage_guard)));
}
static void same_sfn7(const mttl_settings *a,const mttl_settings *b){
 same_features(a,b);assert(a->sfn_backend_ipv4==b->sfn_backend_ipv4);assert(a->sfn_backend_port==b->sfn_backend_port);
}
static size_t wrap_v3(uint8_t *storage,size_t cap,const uint8_t *body,size_t n){
 assert(cap>=n+38);storage[0]=3;storage[1]=(uint8_t)n;storage[2]=(uint8_t)(n>>8);memcpy(storage+3,body,n);memset(storage+3+n,0,35);storage[n+36]=0;storage[n+37]=0;return n+38;
}
int main(void){
 mttl_settings a,b,c;uint8_t body[1024],v6[1024],v5[1024],storage[1200],slot[MTTL_SETTINGS_SLOT],out[1200];size_t n,n6,n5,sn,on;uint32_t seq;size_t physical;
 mttl_settings_default(&a);a.child_lock_mask=21;a.voltage_guard.enabled=1;a.voltage_guard.low_trip_dv=1750;a.voltage_guard.high_trip_dv=2550;a.voltage_guard.recover_low_dv=1900;a.voltage_guard.recover_high_dv=2450;a.voltage_guard.trip_samples=3;a.voltage_guard.recover_samples=4;
 a.sfn_backend_ipv4=0x0a010203u;a.sfn_backend_port=10086;
 a.schedule.timezone=120;a.schedule.rules[0].minute=123;a.schedule.rules[0].mode=129;a.schedule.rules[0].channel=2;a.schedule.rules[0].on=1;
 assert(mttl_settings_valid(&a));n=mttl_settings_pack(&a,body,sizeof(body));assert(n&&body[0]==7);assert(mttl_settings_unpack(body,n,&b));same_sfn7(&a,&b);assert(!memcmp(&a.schedule,&b.schedule,sizeof(a.schedule)));

 /* Legacy v6 retains voltage/child features but defaults to unprovisioned SFN backend. */
 memcpy(v6,body,n-6);n6=n-6;v6[0]=6;assert(mttl_settings_unpack(v6,n6,&c));same_features(&a,&c);assert(c.sfn_backend_ipv4==0&&c.sfn_backend_port==10086);
 /* Legacy v5 remains accepted and gets safe SFN defaults. */
 memcpy(v5,body,n-18);n5=n-18;v5[0]=5;assert(mttl_settings_unpack(v5,n5,&c));assert(c.child_lock_mask==0&&c.voltage_guard.enabled==0&&c.sfn_backend_ipv4==0&&c.sfn_backend_port==10086);assert(!memcmp(&a.schedule,&c.schedule,sizeof(a.schedule)));

 sn=mttl_settings_storage_pack(&a,storage,sizeof(storage));assert(sn&&storage[0]==3&&storage[3]==7);
 memset(slot,0xaa,sizeof(slot));assert(mttl_settings_encode(slot,storage,sn,77));slot[MTTL_SETTINGS_SLOT-1]=0;
 assert(slot[16]==4);assert(!memcmp(slot+1640,"SC01",4));assert(!memcmp(slot+1720,"SFN2",4));
 assert(mttl_settings_validate(slot,&seq,&physical)&&seq==77);
 assert(mttl_settings_extract(slot,out,sizeof(out),&on));assert(mttl_settings_storage_unpack(out,on,&b));same_sfn7(&a,&b);assert(!memcmp(&a.schedule,&b.schedule,sizeof(a.schedule)));

 /* Sidecar corruption must invalidate the complete atomic record. */
 slot[1724]^=1;assert(!mttl_settings_validate(slot,&seq,&physical));slot[1724]^=1;assert(mttl_settings_validate(slot,&seq,&physical));

 /* A legacy SFN1/v6 sidecar remains readable. */
 sn=wrap_v3(storage,sizeof(storage),v6,n6);memset(slot,0xff,sizeof(slot));assert(mttl_settings_encode(slot,storage,sn,78));slot[MTTL_SETTINGS_SLOT-1]=0;assert(!memcmp(slot+1720,"SFN1",4));assert(mttl_settings_extract(slot,out,sizeof(out),&on));assert(mttl_settings_storage_unpack(out,on,&b));same_features(&a,&b);assert(b.sfn_backend_ipv4==0&&b.sfn_backend_port==10086);

 /* Legacy v5 storage remains extractable with no SFN sidecar. */
 sn=wrap_v3(storage,sizeof(storage),v5,n5);memset(slot,0xff,sizeof(slot));assert(mttl_settings_encode(slot,storage,sn,79));slot[MTTL_SETTINGS_SLOT-1]=0;assert(slot[16]==4);assert(!memcmp(slot+1640,"SC01",4));assert(slot[1720]==0xff);assert(mttl_settings_extract(slot,out,sizeof(out),&on));assert(mttl_settings_storage_unpack(out,on,&b));assert(b.child_lock_mask==0&&b.voltage_guard.enabled==0&&b.sfn_backend_ipv4==0&&b.sfn_backend_port==10086);

 /* Invalid backend addresses are rejected; unprovisioned zero is allowed. */
 a.sfn_backend_ipv4=0x7f000001u;assert(!mttl_settings_valid(&a));a.sfn_backend_ipv4=0xe0000001u;assert(!mttl_settings_valid(&a));a.sfn_backend_ipv4=0;assert(mttl_settings_valid(&a));
 puts("SFN_SETTINGS_SIDECAR_V7_TESTS=PASS");return 0;
}
