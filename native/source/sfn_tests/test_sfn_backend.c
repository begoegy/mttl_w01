#include <assert.h>
#include <stdio.h>
#include "../standalone/firmware/settings.h"
#include "../standalone/firmware/sfn_backend.h"
int main(void){
 mttl_settings s={0};s.sfn_backend_port=10086;
 assert(sfn_backend_valid(0,10086));assert(!sfn_backend_valid(0,0));
 assert(!sfn_backend_valid(0x7f000001u,10086));assert(!sfn_backend_valid(0xe0000001u,10086));assert(!sfn_backend_valid(0xffffffffu,10086));
 assert(sfn_backend_valid(0xc000020au,10086)); /* TEST-NET-1 192.0.2.10 */
 assert(sfn_backend_pin(&s,0xc000020au,10086)==1);assert(s.sfn_backend_ipv4==0xc000020au&&s.sfn_backend_port==10086);
 assert(sfn_backend_pin(&s,0xc000020au,10086)==0);
 assert(sfn_backend_pin(&s,0xc0000214u,21197)==1);assert(s.sfn_backend_ipv4==0xc0000214u&&s.sfn_backend_port==21197);
 puts("SFN_BACKEND_PROVISION_TESTS=PASS");return 0;
}
