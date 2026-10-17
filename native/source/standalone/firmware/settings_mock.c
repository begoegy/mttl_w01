#include "settings.h"
#include "settings_store.h"
#include "config_store.h"
static uint8_t memory[2][MTTL_SETTINGS_SLOT];
static mttl_settings_workspace workspace;
static mttl_config_workspace config_workspace;
static int read_fail,erase_calls,cut;
static int read_count,fail_read_at;
static int erase_fail,corrupt_read_at;
static int read_slot(void *p,unsigned slot,size_t off,uint8_t *out,size_t n){size_t i;(void)p;++read_count;if(read_fail||(fail_read_at&&read_count==fail_read_at)||slot>1||off+n>MTTL_SETTINGS_SLOT)return 0;for(i=0;i<n;i++)out[i]=memory[slot][off+i];if(read_count==corrupt_read_at&&n>16)out[16]^=1;return 1;}
static int erase_slot(void *p,unsigned slot){size_t i;(void)p;++erase_calls;if(slot>1||erase_fail)return 0;for(i=0;i<MTTL_SETTINGS_SLOT;i++)memory[slot][i]=255;return 1;}
static int program(void *p,unsigned slot,size_t off,const uint8_t *data,size_t n){size_t i;(void)p;if(slot>1||off+n>MTTL_SETTINGS_SLOT)return 0;for(i=0;i<n;i++){if(cut==0)return 0;if(cut>0)--cut;if((memory[slot][off+i]&data[i])!=data[i])return 0;memory[slot][off+i]=data[i];}return 1;}
unsigned mttl_settings_mock_tests(void){
    static mttl_settings s,out;
    static uint8_t body[MTTL_SETTINGS_BODY_MAX],loaded[MTTL_SETTINGS_BODY_MAX];
    mttl_settings_io io={0,read_slot,erase_slot,program};
    size_t n,size,i;unsigned failed=0;int before;static const int cuts[]={0,1,16,100,1536,1540,1550,1573,1577,2046,2047};
    mttl_settings_default(&s);n=mttl_settings_pack(&s,body,sizeof(body));
    if(!n||!mttl_settings_unpack(body,n,&out)||out.port!=1883)++failed;
    loaded[0]=123;if(mttl_settings_pack(&s,loaded,1)||loaded[0]!=123)++failed;
    if(mttl_settings_unpack(body,n+1,&out))++failed;
    out.port=99;if(mttl_settings_unpack(body,n-1,&out)||out.port!=99)++failed;
    body[0]=2;if(mttl_settings_unpack(body,n,&out))++failed;body[0]=5;
    s.device[0]=(char)0xc0;s.device[1]=(char)0x80;s.device[2]=0;if(mttl_settings_valid(&s))++failed;
    mttl_settings_default(&s);s.device[0]=(char)0xea;s.device[1]=(char)0xb0;s.device[2]=(char)0x80;s.device[3]=0;
    if(!mttl_settings_valid(&s))++failed;
    s.mqtt_enabled=1;if(mttl_settings_valid(&s))++failed;
    s.broker[0]='a';s.broker[1]=0;if(!mttl_settings_valid(&s))++failed;
    s.topic[0]='#';if(mttl_settings_valid(&s))++failed;
    mttl_settings_default(&s);n=mttl_settings_pack(&s,body,sizeof(body));
    for(i=0;i<sizeof(memory);i++)((uint8_t *)memory)[i]=255;
    cut=-1;read_fail=0;erase_calls=0;
    if(mttl_settings_load(&io,&workspace,loaded,sizeof(loaded),&size)!=0)++failed;
    if(mttl_settings_save(&io,&workspace,body,n)!=1)++failed;
    if(mttl_settings_load(&io,&workspace,loaded,sizeof(loaded),&size)!=1||size!=n)++failed;
    s.language=1;n=mttl_settings_pack(&s,body,sizeof(body));
    for(i=0;i<sizeof(cuts)/sizeof(cuts[0]);i++){
        cut=cuts[i];if(mttl_settings_save(&io,&workspace,body,n)!=-1)++failed;
        if(mttl_settings_load(&io,&workspace,loaded,sizeof(loaded),&size)!=1||loaded[1]!=0)++failed;
    }
    cut=-1;if(mttl_settings_save(&io,&workspace,body,n)!=1)++failed;
    if(mttl_settings_load(&io,&workspace,loaded,sizeof(loaded),&size)!=1||loaded[1]!=1)++failed;
    s.language=0;n=mttl_settings_pack(&s,body,sizeof(body));read_count=0;fail_read_at=4;
    if(mttl_settings_save(&io,&workspace,body,n)!=-1)++failed;
    fail_read_at=0;
    if(mttl_settings_load(&io,&workspace,loaded,sizeof(loaded),&size)!=1||loaded[1]!=0)++failed;
    erase_fail=1;if(mttl_settings_save(&io,&workspace,body,n)!=-1)++failed;erase_fail=0;
    if(mttl_settings_load(&io,&workspace,loaded,sizeof(loaded),&size)!=1||loaded[1]!=0)++failed;
    read_count=0;corrupt_read_at=3;
    if(mttl_settings_save(&io,&workspace,body,n)!=-1)++failed;corrupt_read_at=0;
    if(memory[1][MTTL_SETTINGS_SLOT-1]!=255)++failed;
    if(mttl_settings_load(&io,&workspace,loaded,sizeof(loaded),&size)!=1||loaded[1]!=0)++failed;
    read_fail=1;before=erase_calls;if(mttl_settings_save(&io,&workspace,body,n)!=-1||erase_calls!=before)++failed;read_fail=0;
    for(i=0;i<sizeof(workspace);i++)if(((uint8_t *)&workspace)[i]){++failed;break;}
    memory[0][16]^=1;memory[1][16]^=1;before=erase_calls;
    if(mttl_settings_save(&io,&workspace,body,n)!=-1||erase_calls!=before)++failed;
    for(i=0;i<sizeof(memory);i++)((uint8_t *)memory)[i]=255;
    if(mttl_config_load(&io,&config_workspace,&out)!=0||out.port!=1883)++failed;
    if(mttl_config_save(&io,&config_workspace,&s)!=1)++failed;
    if(mttl_config_load(&io,&config_workspace,&out)!=1||out.language!=0)++failed;
    /* Device checkpoint roundtrip, web exclusion, migration and torn saves. */
    s.energy_valid=15;for(i=0;i<4;i++){s.energy_raw[i]=100+i;s.energy_wh[i]=1000+i;}
    if(mttl_config_save(&io,&config_workspace,&s)!=1)++failed;
    if(mttl_config_load(&io,&config_workspace,&out)!=1||out.energy_valid!=15||out.energy_raw[3]!=103||out.energy_wh[3]!=1003)++failed;
    n=mttl_settings_pack(&s,body,sizeof(body));
    if(!mttl_settings_unpack(body,n,&out)||out.energy_valid||out.energy_wh[3])++failed;
    mttl_settings_keep_energy(&out,&s);if(out.energy_wh[3]!=1003)++failed;
    for(i=0;i<sizeof(cuts)/sizeof(cuts[0]);i++){
        cut=cuts[i];s.energy_wh[0]=2000;
        if(mttl_config_save(&io,&config_workspace,&s)!=-1)++failed;
        if(mttl_config_load(&io,&config_workspace,&out)!=1||out.energy_wh[0]!=1000)++failed;
    }cut=-1;
    n=mttl_settings_storage_pack(&s,body,sizeof(body));
    out.port=123;if(mttl_settings_storage_unpack(body,n-1,&out)||out.port!=123)++failed;
    if(mttl_settings_unpack(body,n,&out))++failed;
    body[n-35]=16;if(mttl_settings_storage_unpack(body,n,&out))++failed;
    n=mttl_settings_pack(&s,body,sizeof(body));
    if(!mttl_settings_storage_unpack(body,n,&out)||out.energy_valid)++failed;
    /* Maximum legal strings plus checkpoint must fit the reserved body. */
    mttl_settings_default(&out);
    for(i=0;i<64;i++){out.device[i]='a';out.channel[0][i]=out.channel[1][i]=out.channel[2][i]=out.channel[3][i]='a';out.wifi_password[i]='a';out.mqtt_user[i]=out.mqtt_password[i]=out.topic[i]='a';}
    for(i=0;i<32;i++)out.ssid[i]='a';for(i=0;i<253;i++)out.broker[i]='a';
    size=mttl_settings_storage_pack(&out,loaded,sizeof(loaded));
    if(size!=972||!mttl_settings_storage_unpack(loaded,size,&out))++failed;
    /* Recreate the malformed unknown-schema record test input. */
    n=mttl_settings_pack(&s,body,sizeof(body));
    body[0]=2;
    mttl_settings_encode(memory[1],body,n,10);memory[1][MTTL_SETTINGS_SLOT-1]=0;
    before=erase_calls;out.port=123;
    if(mttl_config_load(&io,&config_workspace,&out)!=-1||out.port!=123)++failed;
    if(mttl_config_save(&io,&config_workspace,&s)!=-1||erase_calls!=before)++failed;
    for(i=0;i<sizeof(config_workspace);i++)if(((uint8_t *)&config_workspace)[i]){++failed;break;}
    return failed;
}
