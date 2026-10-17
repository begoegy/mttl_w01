#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_ota_policy.h"
int main(void){char b[64];size_t n;
 assert(!sfn_ota_sequence_allowed(0,1));
 assert(!sfn_ota_sequence_allowed(20015,20014));
 assert(!sfn_ota_sequence_allowed(20015,20015));
 assert(sfn_ota_sequence_allowed(20015,20016));
 assert(sfn_ota_sequence_allowed(20015,0xffffffffu));
 n=sfn_ota_profile_marker(b,sizeof(b),20016);assert(n&&strcmp(b,"SFN_NATIVE_REMOTE_OTA_SAFE;SEQ=20016")==0);
 assert(!sfn_ota_profile_marker(b,8,20016));assert(!sfn_ota_profile_marker(b,sizeof(b),0));
 puts("SFN_OTA_POLICY=PASS downgrade=REJECT same=REJECT upgrade=PASS marker=EXACT");return 0;}
