#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_schedule_sync.h"
int main(void){
 sfn_schedule_sync s;sfn_schedule_meta m;mttl_schedule_config c;
 sfn_schedule_sync_init(&s);
 assert(!sfn_schedule_sync_begin(&s,1,1));
 assert(sfn_schedule_sync_clock(&s,1760700000u,180,1770000000u,120,1780000000u,180));
 assert(sfn_schedule_runtime_offset(&s,0,1765000000u,0)==180);assert(sfn_schedule_runtime_offset(&s,0,1770000000u,0)==120);assert(sfn_schedule_runtime_offset(&s,0,1780000000u,0)==180);
 assert(sfn_schedule_sync_begin(&s,42,2));
 assert(sfn_schedule_sync_item(&s,0,1,1,0,0xffff,480,1));
 assert(sfn_schedule_sync_item(&s,1,0,2,0x41,0xffff,1320,0));
 assert(!sfn_schedule_sync_item(&s,1,2,1,0,0xffff,100,1));
 assert(sfn_schedule_sync_commit(&s,42,&c,&m));assert(m.valid&&m.generation==42&&m.count==2&&c.timezone==180);
 assert(c.rules[0].mode==0x81&&c.rules[0].channel==0&&c.rules[0].minute==480&&c.rules[0].on==1);
 assert(c.rules[1].mode==0x82&&c.rules[1].channel==4&&c.rules[1].day==0x41&&c.rules[1].on==0);
 assert(sfn_schedule_runtime_offset(0,&m,1775000000u,0)==120);
 /* Once, too many items, sparse/duplicate slots and wrong generation all fail closed. */
 assert(sfn_schedule_sync_clock(&s,1760700000u,180,0,0,0,0));assert(!sfn_schedule_sync_begin(&s,1,9));assert(sfn_schedule_sync_begin(&s,2,1));assert(!sfn_schedule_sync_item(&s,0,1,0,0,20000,500,1));assert(!sfn_schedule_sync_commit(&s,2,&c,&m));
 assert(sfn_schedule_sync_begin(&s,3,2));assert(sfn_schedule_sync_item(&s,0,1,1,0,0xffff,1,1));assert(!sfn_schedule_sync_commit(&s,3,&c,&m));
 assert(sfn_schedule_sync_clock(&s,1760700000u,180,1770000000u,120,1780000000u,180));assert(sfn_schedule_sync_begin(&s,4,1));assert(sfn_schedule_sync_item(&s,0,4,2,0x7f,0xffff,1439,1));assert(!sfn_schedule_sync_commit(&s,5,&c,&m));assert(sfn_schedule_sync_commit(&s,4,&c,&m));
 puts("SFN_SCHEDULE_SYNC_TRANSACTION_TESTS=PASS");return 0;
}
