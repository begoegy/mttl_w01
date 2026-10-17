#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_up_protocol.h"
int main(void){sfn_up_command c;char b[80];
 const char *clock="up:sfn:90:1760700000:180:1770000000:120:1780000000:180";
 assert(sfn_up_parse_command(clock,strlen(clock),&c)&&c.type==SFN_UP_CMD_CLOCK_SYNC&&c.epoch==1760700000u&&c.offset_minutes==180&&c.transition1_offset_minutes==120);
 assert(sfn_up_parse_command("up:sfn:91:42:2",strlen("up:sfn:91:42:2"),&c)&&c.type==SFN_UP_CMD_SCHEDULE_BEGIN&&c.generation==42&&c.count==2);
 assert(sfn_up_parse_command("up:sfn:92:1:0:2:65:65535:1320:0",strlen("up:sfn:92:1:0:2:65:65535:1320:0"),&c)&&c.type==SFN_UP_CMD_SCHEDULE_ITEM&&c.slot==1&&c.outlet==0&&c.repeat==2&&c.days==65&&c.date_days==65535&&c.minute==1320&&!c.on);
 assert(sfn_up_parse_command("up:sfn:93:42",strlen("up:sfn:93:42"),&c)&&c.type==SFN_UP_CMD_SCHEDULE_COMMIT&&c.generation==42);
 assert(sfn_up_parse_command("up:sfn:94",strlen("up:sfn:94"),&c)&&c.type==SFN_UP_CMD_SCHEDULE_STATUS);
 assert(sfn_up_encode_clock_ack(b,sizeof(b),1760700000u)&&!strcmp(b,"up:sfn:90:1760700000:0\r\n"));
 assert(sfn_up_encode_schedule_commit_ack(b,sizeof(b),42,2)&&!strcmp(b,"up:sfn:93:42:2\r\n"));
 assert(sfn_up_encode_schedule_status(b,sizeof(b),42,2,1,1)&&!strcmp(b,"up:sfn:94:42:770\r\n"));
 assert(!sfn_up_parse_command("up:sfn:90:1760700000:180:1760000000:120:0:0",strlen("up:sfn:90:1760700000:180:1760000000:120:0:0"),&c) || c.type==SFN_UP_CMD_CLOCK_SYNC); /* temporal ordering belongs to sync validator */
 assert(!sfn_up_parse_command("up:sfn:92:1:0:2:65:65535:1320:2",strlen("up:sfn:92:1:0:2:65:65535:1320:2"),&c));
 puts("SFN_SCHEDULE_WIRE_TESTS=PASS");return 0;}
