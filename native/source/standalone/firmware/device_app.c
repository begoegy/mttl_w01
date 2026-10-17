#include "device_app.h"
#include <stdio.h>
#include <string.h>
static void publish_sample(mttl_device_app *,const char *,uint32_t);
static int quote(const char *s,char *out,size_t cap){size_t j=0;unsigned char c;if(cap<3)return 0;out[j++]='"';while(*s){c=(unsigned char)*s++;if(c<32)return 0;if(c=='"'||c=='\\'){if(j+2>=cap)return 0;out[j++]='\\';}if(j+2>=cap)return 0;out[j++]=(char)c;}out[j++]='"';out[j]=0;return 1;}
void mttl_device_init(mttl_device_app *d,uint64_t reference){memset(d,0,sizeof(*d));mttl_settings_default(&d->settings);mttl_control_init(&d->control,100);mttl_hardware_init(&d->hardware);mttl_mqtt_engine_init(&d->mqtt);mttl_ntp_client_init(&d->ntp,reference);}
int mttl_device_load(mttl_device_app *d,const mttl_settings_io *io,mttl_config_workspace *workspace){int result;if(!workspace)return -1;result=mttl_config_load(io,workspace,&d->settings);if(result>=0){d->persistent=1;++d->revision;}return result;}
int mttl_device_configure(mttl_device_app *d,const uint8_t *body,size_t size,const mttl_settings_io *io,mttl_config_workspace *workspace){
    mttl_settings candidate;int result=0;volatile uint8_t *p=(volatile uint8_t *)&candidate;size_t i;
    if(!mttl_settings_unpack(body,size,&candidate))goto done;
    mttl_settings_keep_energy(&candidate,&d->settings);
    if(io&&(!workspace||mttl_config_save(io,workspace,&candidate)!=1))goto done;
    d->settings=candidate;d->persistent=io!=0;++d->revision;result=1;
done:for(i=0;i<sizeof(candidate);i++)p[i]=0;return result;
}
int mttl_device_command(void *context,uint8_t channel,uint8_t on){mttl_device_app *d=context;if(!d||!d->hardware.verified||!d->hardware.write)return 0;if(d->relay_request)return d->relay_request(channel,on);return mttl_request(&d->control,channel,on);}
void mttl_device_button(mttl_device_app *d,unsigned button,int down,uint32_t now){
    int event;if(button>4)return;if(!button){event=mttl_button_edge(&d->main_button,down!=0,now);if(event==2)d->provision_requested=1;else if(event==1&&d->control.known_mask==15)mttl_device_command(d,MTTL_ALL,d->control.on_mask!=15);}
    else{unsigned ch=button-1;event=mttl_button_edge(&d->channel_button[ch],down!=0,now);if(event&&(d->control.known_mask&(1u<<ch)))mttl_device_command(d,(uint8_t)ch,!(d->control.on_mask&(1u<<ch)));}
}
void mttl_device_tick(mttl_device_app *d,const char *id,int network,uint32_t now,const mttl_mqtt_io *io){
    mttl_output output={&d->hardware,mttl_hardware_begin,mttl_hardware_end};
    if(!d->relay_request&&d->hardware.verified&&d->hardware.write)mttl_control_tick(&d->control,&output,now);
    mttl_ntp_link(&d->ntp,network,now);
    if(io){mttl_mqtt_io bound=*io;bound.command=mttl_device_command;bound.command_context=d;mttl_mqtt_engine_step(&d->mqtt,&d->settings,id,network,d->revision,now,&bound);}
    if(d->mqtt.session.state!=MTTL_MQ_ONLINE||d->mqtt.rediscover){d->mqtt_known=d->mqtt_all=d->mqtt_was_online=0;}
    else{unsigned dirty=(d->control.known_mask&~d->mqtt_known)|((d->control.on_mask^d->mqtt_on)&d->control.known_mask);
      if(dirty||!d->mqtt_all||(uint32_t)(now-d->publish_at)>=200){publish_sample(d,id,now);d->publish_at=now;}}
}
static void publish_sample(mttl_device_app *d,const char *id,uint32_t now){
    static const char *keys[]={"power","meter","voltage","current"};
    const mttl_sample *value=0;char base[96],topic[128],key[24],payload[24];unsigned index=d->publish_index,metric,ch;int32_t raw;unsigned scale=1000;uint32_t magnitude;
    if(d->mqtt.tx_size||d->mqtt.rediscover||!mttl_ha_base(&d->settings,id,base,sizeof(base)))return;
    /* Completed relay changes precede the 25-entity sensor rotation. */
    {unsigned dirty=(d->control.known_mask&~d->mqtt_known)|((d->control.on_mask^d->mqtt_on)&d->control.known_mask);
     if(dirty){for(ch=0;ch<4;ch++)if(dirty&(1u<<ch))break;index=ch+1;}
     else if(d->control.known_mask==15&&(!d->mqtt_all||d->mqtt_was_online!=(d->control.on_mask==15)))index=0;
     /* Do not repeat unchanged switches while other commands are queued. */
     else if(index<5)index=5;}
    /* Switch confirmations are out-of-band: never rewind sensor rotation.
     * Otherwise repeated sw1..sw4 changes starve late channel sensors. */
    if(index>=5)d->publish_index=(uint8_t)(index+1<MTTL_HA_ENTITIES?index+1:5);
    if(index<5){if(!index){if(d->control.known_mask!=15)return;strcpy(key,"all");strcpy(payload,d->control.on_mask==15?"ON":"OFF");}
        else{ch=index-1;if(!(d->control.known_mask&(1u<<ch)))return;snprintf(key,sizeof(key),"sw%u",index);strcpy(payload,(d->control.on_mask&(1u<<ch))?"ON":"OFF");}}
    else{
        if(index<9){metric=index-5;value=metric==0?&d->samples.total_power:metric==1?&d->samples.total_meter:metric==2?&d->samples.voltage:&d->samples.total_current;strcpy(key,keys[metric]);if(metric==2)scale=10;}
        else{metric=(index-9)%4;ch=(index-9)/4;value=metric==0?&d->samples.power[ch]:metric==1?&d->samples.meter[ch]:metric==2?&d->samples.current[ch]:&d->samples.temperature[ch];snprintf(key,sizeof(key),"%s%u",metric==0?"power":metric==1?"meter":metric==2?"current":"temperature",ch+1);if(metric==3)scale=10;}
        if(!mttl_sample_read(value,now,MTTL_SENSOR_MAX_AGE_MS,&raw))return;
        magnitude=(uint32_t)(raw<0?-(int64_t)raw:raw);snprintf(payload,sizeof(payload),"%s%lu.%0*lu",raw<0?"-":"",(unsigned long)(magnitude/scale),scale==10?1:3,(unsigned long)(magnitude%scale));
    }
    snprintf(topic,sizeof(topic),"%s/%s",base,key);if(mttl_mqtt_engine_publish(&d->mqtt,topic,(const uint8_t *)payload,strlen(payload),index<5,now)&&index<5){if(!index){d->mqtt_all=1;d->mqtt_was_online=d->control.on_mask==15;}else{unsigned bit=1u<<(index-1);d->mqtt_known|=bit;d->mqtt_on=(d->mqtt_on&~bit)|(d->control.on_mask&bit);}}
}
static void sample(const mttl_sample *s,uint32_t now,char out[16]){int32_t value;if(mttl_sample_read(s,now,MTTL_SENSOR_MAX_AGE_MS,&value))snprintf(out,16,"%ld",(long)value);else strcpy(out,"null");}
size_t mttl_device_status(const mttl_device_app *d,uint32_t now,char *out,size_t cap){
    char name[132],voltage[16],power[16],meter[16],current[16],temp[16],on[8];size_t used;unsigned i;int n;
    if(!quote(d->settings.device,name,sizeof(name)))return 0;sample(&d->samples.voltage,now,voltage);
    sample(&d->samples.total_power,now,power);sample(&d->samples.total_meter,now,meter);sample(&d->samples.total_current,now,current);
    n=snprintf(out,cap,"{\"name\":%s,\"language\":%u,\"revision\":%lu,\"persistent\":%s,\"hardware_ready\":%s,\"voltage_dv\":%s,\"power_mw\":%s,\"meter_wh\":%s,\"current_ma\":%s,\"mqtt_state\":%u,\"channels\":[",name,d->settings.language,(unsigned long)d->revision,d->persistent?"true":"false",d->hardware.verified&&d->hardware.write?"true":"false",voltage,power,meter,current,(unsigned)d->mqtt.session.state);
    if(n<0||(size_t)n>=cap)return 0;used=(size_t)n;
    for(i=0;i<4;i++){if(!quote(d->settings.channel[i],name,sizeof(name)))return 0;sample(&d->samples.power[i],now,power);sample(&d->samples.meter[i],now,meter);sample(&d->samples.current[i],now,current);sample(&d->samples.temperature[i],now,temp);
        strcpy(on,(d->control.known_mask&(1u<<i))?((d->control.on_mask&(1u<<i))?"true":"false"):"null");
        n=snprintf(out+used,cap-used,"%s{\"name\":%s,\"on\":%s,\"power_mw\":%s,\"meter_wh\":%s,\"current_ma\":%s,\"temp_dc\":%s}",i?",":"",name,on,power,meter,current,temp);if(n<0||(size_t)n>=cap-used)return 0;used+=(size_t)n;}
    n=snprintf(out+used,cap-used,"],\"protection\":{\"reason\":%u,\"error\":%u}}",(unsigned)d->protection.reason,(unsigned)d->protection.error_mask);return n>0&&(size_t)n<cap-used?used+(size_t)n:0;
}
size_t mttl_device_settings_json(const mttl_device_app *d,char *out,size_t cap){return mttl_settings_json(&d->settings,d->persistent,out,cap);}
size_t mttl_settings_json(const mttl_settings *s,int persistent,char *out,size_t cap){
    char device[132],channels[4][132],broker[510],user[132],topic[132];int n;unsigned i;size_t used;
    if(!quote(s->device,device,sizeof(device))||!quote(s->broker,broker,sizeof(broker))||!quote(s->mqtt_user,user,sizeof(user))||!quote(s->topic,topic,sizeof(topic)))return 0;
    for(i=0;i<4;i++)if(!quote(s->channel[i],channels[i],sizeof(channels[i])))return 0;
    n=snprintf(out,cap,"{\"device\":%s,\"channels\":[%s,%s,%s,%s],\"language\":%u,\"broker\":%s,\"port\":%u,\"user\":%s,\"topic\":%s,\"enabled\":%s,\"wifi_password_set\":%s,\"mqtt_password_set\":%s,\"persistent\":%s,\"protection\":{\"enabled\":%u,\"temperature\":%u,\"current\":%u,\"power\":%u},\"schedule_hex\":\"",device,channels[0],channels[1],channels[2],channels[3],s->language,broker,s->port,user,topic,s->mqtt_enabled?"true":"false",s->wifi_password[0]?"true":"false",s->mqtt_password[0]?"true":"false",persistent?"true":"false",(unsigned)s->protection.enabled,(unsigned)s->protection.temperature_c,(unsigned)s->protection.current_ma,(unsigned)s->protection.power_w);
    if(n<0||(size_t)n+103>=cap)return 0;used=(size_t)n;
    {uint8_t bytes[50];static const char hex[]="0123456789abcdef";mttl_schedule_pack(&s->schedule,bytes);for(i=0;i<50;i++){out[used++]=hex[bytes[i]>>4];out[used++]=hex[bytes[i]&15];}}
    out[used++]='"';out[used++]='}';out[used]=0;return used;
}
