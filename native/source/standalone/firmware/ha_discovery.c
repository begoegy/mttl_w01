#include "ha_discovery.h"
#include <stdio.h>
#include <string.h>
static int identity(const char *id){unsigned i;if(!id)return 0;for(i=0;i<12;i++)if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return 0;return !id[12];}
static int quote(const char *s,char *out,size_t cap){size_t i=0,j=0;unsigned char c;if(cap<3)return 0;out[j++]='"';while(s[i]){c=(unsigned char)s[i++];if(c<32)return 0;if(c=='"'||c=='\\'){if(j+2>=cap)return 0;out[j++]='\\';}if(j+2>=cap)return 0;out[j++]=(char)c;}out[j++]='"';out[j]=0;return 1;}
int mttl_ha_base(const mttl_settings *s,const char *id,char *out,size_t cap){int n;if(!s||!out||!identity(id)||!mttl_settings_valid(s))return 0;n=snprintf(out,cap,"%s/standalone/%s",s->topic,id);return n>0&&(size_t)n<cap;}
size_t mttl_ha_discovery(const mttl_settings *s,const char *id,unsigned index,char *topic,size_t tcap,char *json,size_t cap){
    static const char *keys[]={"powerall","meter","voltage","current"};
    static const char *labels[]={"Power All","Meter","Voltage","Current"};
    static const char *channel_labels[]={"Power","Meter","Current","Temperature"};
    static const char *classes[]={"power","energy","voltage","current","temperature"};
    static const char *units[]={"W","kWh","V","A","°C"};
    char base[96],key[24],name[132],device[132],extra[220],display[80];const char *component=index<5?"switch":"sensor";const char *short_id;int n,channel=-1;unsigned kind=0;
    if(!topic||!json||index>=MTTL_HA_ENTITIES||!mttl_ha_base(s,id,base,sizeof(base)))return 0;
    if(index<5){
        if(!index){strcpy(key,"all");if(!quote("SW All",name,sizeof(name)))return 0;}
        else{channel=(int)index-1;snprintf(key,sizeof(key),"sw%u",index);if(!quote(s->channel[channel],name,sizeof(name)))return 0;}
        n=snprintf(extra,sizeof(extra),"\"cmd_t\":\"~/%s/set\",\"pl_on\":\"ON\",\"pl_off\":\"OFF\",\"optimistic\":false,",index?key+2:"all");
    }else{
        if(index<9){kind=index-5;snprintf(key,sizeof(key),"%s",keys[kind]);}
        else{unsigned metric=(index-9)%4;channel=(int)((index-9)/4);kind=metric==2?3:metric==3?4:metric;snprintf(key,sizeof(key),"%s%u",classes[kind],(unsigned)channel+1);if(metric==1)snprintf(key,sizeof(key),"meter%u",(unsigned)channel+1);}
        if(channel<0){if(!quote(labels[index-5],name,sizeof(name)))return 0;}
        else{snprintf(display,sizeof(display),"%s %s",s->channel[channel],channel_labels[(index-9)%4]);if(!quote(display,name,sizeof(name)))return 0;}
        n=snprintf(extra,sizeof(extra),"\"dev_cla\":\"%s\",\"stat_cla\":\"%s\",\"unit_of_meas\":\"%s\",\"exp_aft\":30,",classes[kind],kind==1?"total_increasing":"measurement",units[kind]);
    }
    if(n<0||(size_t)n>=sizeof(extra))return 0;
    short_id=id+5;if(!quote(s->device,device,sizeof(device)))return 0;
    n=snprintf(topic,tcap,"homeassistant/%s/mttl_%s_%s/config",component,short_id,key);if(n<0||(size_t)n>=tcap)return 0;
    n=snprintf(json,cap,"{\"~\":\"%s\",\"name\":%s,\"uniq_id\":\"mttl_%s_%s\",\"default_entity_id\":\"%s.mttl_%s_%s\",\"en\":true,\"stat_t\":\"~/%s\",\"avty_t\":\"~/availability\",%s\"dev\":{\"ids\":[\"mttl_%s\"],\"name\":%s,\"mdl\":\"MTTL-W01\",\"mf\":\"LG U+\",\"sw\":\"2.0.12\"}}",base,name,short_id,key,component,short_id,key,index==5?"power":key,extra,short_id,device);
    if(n<0||(size_t)n>=cap)return 0;return (size_t)n;
}
