#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_up_protocol.h"
int main(void){
 char b[1024];sfn_up_command c;sfn_up_outlet o[4]={{1,0,111,0,0,10,31,"0","0"},{2,1,222,1,5300,10,32,"0","0"},{3,0,333,0,0,10,33,"0","0"},{4,1,444,1,1200000,1000,34,"0","0"}};size_t n;
 assert(sfn_up_parse_command("up:getinfo:all\r\n",16,&c)&&c.type==SFN_UP_CMD_GETINFO_ALL);
 assert(sfn_up_parse_command("up:onoff:3:on",13,&c)&&c.type==SFN_UP_CMD_ONOFF&&c.channel==3&&c.on);
 assert(sfn_up_parse_command("up:onoff:1:off",14,&c)&&c.channel==1&&!c.on);
 assert(sfn_up_parse_command("up:power_report:1:vol",21,&c)&&c.type==SFN_UP_CMD_VOLTAGE);
 assert(sfn_up_parse_command("up:query:wifirssi",18,&c)&&c.type==SFN_UP_CMD_RSSI);
 assert(sfn_up_parse_command("up:childlock",12,&c)&&c.type==SFN_UP_CMD_CHILDLOCK_GET);
 assert(sfn_up_parse_command("up:childlock:31",15,&c)&&c.type==SFN_UP_CMD_CHILDLOCK_SET&&c.mask==31);
 assert(!sfn_up_parse_command("up:childlock:32",15,&c));assert(!sfn_up_parse_command("up:onoff:0:on",13,&c));assert(!sfn_up_parse_command("up:onoff:9:on",13,&c));assert(!sfn_up_parse_command("up:query:wifirssi\r\nup:onoff:1:on",32,&c));
 n=sfn_up_encode_bootinfo(b,sizeof(b),"lgutap","88D03907723D","0.1.55-2.1.0");assert(n&&strcmp(b,"up:bootinfo:lgutap;88D03907723D;88D03907723D;0.1.55-2.1.0;connect\r\n")==0);
 assert(sfn_up_encode_onoff_ack(b,sizeof(b),4,1)&&!strcmp(b,"up:onoff:4:on\r\n"));
 assert(sfn_up_encode_event(b,sizeof(b),0,1)&&!strcmp(b,"up:event:onoff:0:on\r\n"));
 assert(sfn_up_encode_voltage(b,sizeof(b),1,224500)&&!strcmp(b,"up:power_report:1:224500\r\n"));
 assert(sfn_up_encode_rssi(b,sizeof(b),-61)&&!strcmp(b,"up:query:-61\r\n"));
 assert(sfn_up_encode_childlock(b,sizeof(b),31)&&!strcmp(b,"up:cl:31\r\n"));
 n=sfn_up_encode_getinfo(b,sizeof(b),o);assert(n);assert(strstr(b,"1:111;off;0;0;0;0;0000000A;")!=0);assert(strstr(b,"2:222;on;1;0;0;5300;0000000A;")!=0);assert(strstr(b,"4:444;on;1;0;0;1200000;000003E8;")!=0);assert(strstr(b,"5:0;on;0;0;0;1205300;00000406;")!=0);assert(n>=2&&b[n-2]=='\r'&&b[n-1]=='\n');
 puts("SFN_UP_PROTOCOL_TESTS=PASS");return 0;
}
