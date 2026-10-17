#ifndef SFN_UP_PROTOCOL_H
#define SFN_UP_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SFN_UP_CMD_NONE=0,
    SFN_UP_CMD_GETINFO_ALL,
    SFN_UP_CMD_ONOFF,
    SFN_UP_CMD_VOLTAGE,
    SFN_UP_CMD_RSSI,
    SFN_UP_CMD_CHILDLOCK_GET,
    SFN_UP_CMD_CHILDLOCK_SET,
    SFN_UP_CMD_VGUARD_SET,
    SFN_UP_CMD_CLOCK_SYNC,
    SFN_UP_CMD_SCHEDULE_BEGIN,
    SFN_UP_CMD_SCHEDULE_ITEM,
    SFN_UP_CMD_SCHEDULE_COMMIT,
    SFN_UP_CMD_SCHEDULE_STATUS
} sfn_up_command_type;

typedef struct {
    sfn_up_command_type type;
    uint8_t channel;
    uint8_t on;
    uint8_t mask;
    uint32_t revision,flags,thresholds,timing,recovery_seconds;
    uint32_t epoch,transition1_epoch,transition2_epoch,generation;
    int16_t offset_minutes,transition1_offset_minutes,transition2_offset_minutes;
    uint16_t date_days,minute;
    uint8_t count,slot,outlet,repeat,days;
} sfn_up_command;

typedef struct {
    uint8_t channel;
    uint8_t on;
    int32_t runtime;
    int32_t state;
    int32_t power_mw;
    uint32_t energy_wh;
    int32_t temperature_c;
    const char *overload;
    const char *overheat;
} sfn_up_outlet;

int sfn_up_parse_command(const char *line,size_t size,sfn_up_command *out);
size_t sfn_up_encode_bootinfo(char *out,size_t cap,const char *model,const char mac[13],const char *firmware);
size_t sfn_up_encode_onoff_ack(char *out,size_t cap,unsigned channel,int on);
size_t sfn_up_encode_event(char *out,size_t cap,unsigned channel,int on);
size_t sfn_up_encode_voltage(char *out,size_t cap,unsigned channel,int32_t millivolts);
size_t sfn_up_encode_rssi(char *out,size_t cap,int rssi);
size_t sfn_up_encode_childlock(char *out,size_t cap,unsigned mask);
size_t sfn_up_encode_vguard_ack(char *out,size_t cap,uint32_t revision);
size_t sfn_up_encode_clock_ack(char *out,size_t cap,uint32_t epoch);
size_t sfn_up_encode_schedule_commit_ack(char *out,size_t cap,uint32_t generation,unsigned count);
size_t sfn_up_encode_schedule_status(char *out,size_t cap,uint32_t generation,unsigned count,int clock_valid,int storage_valid);
size_t sfn_up_encode_getinfo(char *out,size_t cap,const sfn_up_outlet outlets[4]);
#endif
