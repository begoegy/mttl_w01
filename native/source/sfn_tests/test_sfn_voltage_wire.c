#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_up_protocol.h"
#include "../standalone/firmware/sfn_voltage_policy.h"
int main(void){
 sfn_up_command c;sfn_voltage_guard_config g;char out[64];size_t n;
 /* low=180.0, high=250.0, hysteresis=10.0, hold=10.0s, recovery=20s, mask=0b0101 */
 assert(sfn_up_parse_command("up:vguard:17:1281:163841800:6553700:20",sizeof("up:vguard:17:1281:163841800:6553700:20")-1,&c));
 assert(c.type==SFN_UP_CMD_VGUARD_SET&&c.revision==17);
 assert(sfn_voltage_policy_decode(c.flags,c.thresholds,c.timing,c.recovery_seconds,&g));
 assert(g.enabled==1&&g.protected_mask==5&&g.low_trip_dv==1800&&g.high_trip_dv==2500&&g.hysteresis_dv==100&&g.trip_hold_ds==100&&g.recovery_seconds==20);
 n=sfn_up_encode_vguard_ack(out,sizeof(out),17);assert(n&&strcmp(out,"up:vguard:17\r\n")==0);
 /* auto-restore is deliberately unsupported; fast hold/recovery promises fail closed. */
 assert(!sfn_voltage_policy_decode(1283,c.thresholds,c.timing,20,&g));
 assert(!sfn_voltage_policy_decode(1281,c.thresholds,(99u<<16)|100u,20,&g));
 assert(!sfn_voltage_policy_decode(1281,c.thresholds,(100u<<16)|100u,9,&g));
 assert(!sfn_voltage_policy_decode(1,c.thresholds,(100u<<16)|100u,20,&g)); /* mask zero */
 assert(!sfn_up_parse_command("up:vguard:0:1281:163841800:655360100:20",sizeof("up:vguard:0:1281:163841800:6553700:20")-1,&c));
 assert(!sfn_up_parse_command("up:vguard:17:1281:163841800:6553700:20:1",sizeof("up:vguard:17:1281:163841800:6553700:20:1")-1,&c));
 puts("SFN_VOLTAGE_WIRE_POLICY_TESTS=PASS");return 0;
}
