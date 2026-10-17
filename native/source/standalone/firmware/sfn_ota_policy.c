#include "sfn_ota_policy.h"
#include <stdio.h>
int sfn_ota_sequence_allowed(uint32_t current_sequence,uint32_t target_sequence){return current_sequence!=0&&target_sequence>current_sequence;}
size_t sfn_ota_profile_marker(char *out,size_t cap,uint32_t sequence){int n;if(!out||!cap||!sequence)return 0;n=snprintf(out,cap,"SFN_NATIVE_REMOTE_OTA_SAFE;SEQ=%lu",(unsigned long)sequence);return n>0&&(size_t)n<cap?(size_t)n:0;}
