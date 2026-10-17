#include "lifecycle.h"
typedef struct { uint32_t now,address; int association,web_calls,stop_ok,join_ok,dhcp_ok,web_ok,commit_ok,commits,ap_pending; } mock;
static int stop(void *p){return ((mock *)p)->stop_ok;}
static int ap(void *p){return ((mock *)p)->ap_pending?-1:1;}
static int join(void *p,const char *ssid,const char *password){(void)ssid;(void)password;return ((mock *)p)->join_ok;}
static int associated(void *p){return ((mock *)p)->association;}
static int dhcp(void *p){return ((mock *)p)->dhcp_ok;}
static uint32_t ip(void *p){return ((mock *)p)->address;}
static int web(void *p){mock *m=p;++m->web_calls;return m->web_ok;}
static uint32_t clock(void *p){return ((mock *)p)->now;}
static int commit(void *p,const char *ssid,const char *password){mock *m=p;(void)ssid;(void)password;++m->commits;return m->commit_ok;}
unsigned mttl_lifecycle_mock_tests(void){
    unsigned failed=0;mttl_lifecycle life;
    mock m={0,0,0,0,1,1,1,1,1,0,0};
    mttl_services s={&m,stop,ap,join,associated,dhcp,ip,web,clock,commit};
    mttl_lifecycle_init(&life);mttl_provision(&life.network,0);
    mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_AP||m.web_calls!=1)++failed;
    if(!mttl_wifi_submit(&life.network,(const uint8_t *)"SSID",4,(const uint8_t *)"password",8,0))++failed;
    mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_JOINING||m.commits)++failed;
    m.association=1;mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_DHCP)++failed;
    mttl_lifecycle_step(&life,&s);
    if(m.web_calls!=1||m.commits)++failed;
    m.address=0xc000020au;mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_ONLINE||m.commits!=1||m.web_calls!=2||life.network.password[0])++failed;
    if(!mttl_wifi_submit(&life.network,(const uint8_t *)"NewSSID",7,(const uint8_t *)"newpassword",11,m.now))++failed;
    life.join_started=life.dhcp_started=0;
    mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);
    mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_ONLINE||m.commits!=2||m.web_calls!=3)++failed;
    m.commits=1;m.web_calls=2;
    m.association=0;mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_STOPPING)++failed;
    mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_AP||!life.web_started)++failed;
    m.join_ok=0;mttl_wifi_submit(&life.network,(const uint8_t *)"S",1,0,0,m.now);mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_STOPPING)++failed;
    mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);
    m.join_ok=1;m.association=0;mttl_wifi_submit(&life.network,(const uint8_t *)"S",1,0,0,m.now);mttl_lifecycle_step(&life,&s);
    m.now=15000;mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_AP||m.commits!=1)++failed;
    mttl_lifecycle_step(&life,&s);m.web_ok=0;
    m.association=1;mttl_wifi_submit(&life.network,(const uint8_t *)"S",1,0,0,m.now);
    mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_STOPPING)++failed; // online web failure
    mttl_lifecycle_init(&life);m.now=0;m.stop_ok=1;m.web_calls=0;m.ap_pending=1;
    mttl_provision(&life.network,0);mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);
    if(life.ap_started||m.web_calls)++failed;
    m.now=4999;mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_AP)++failed;
    m.now=5000;mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_STOPPING)++failed;
    m.stop_ok=0;mttl_lifecycle_step(&life,&s);
    m.now=15000;mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_FAULT||m.web_calls)++failed;
    /* Cleanup timeout recovers without reboot, even after a long outage. */
    m.now=29999;mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_FAULT)++failed;
    m.now=30000;mttl_lifecycle_step(&life,&s);
    if(life.network.state!=MTTL_NET_STOPPING)++failed;
    m.stop_ok=1;m.ap_pending=0;m.web_ok=1;m.join_ok=0;
    mttl_lifecycle_step(&life,&s);
    /* Saved credentials can be submitted immediately after cleanup, before
     * an AP or HTTP server exists. Repeated absent-AP joins never commit. */
    {unsigned attempt;int baseline=m.commits;
     for(attempt=0;attempt<20;attempt++){
      if(!mttl_wifi_submit(&life.network,(const uint8_t *)"Saved",5,(const uint8_t *)"password",8,m.now))++failed;
      mttl_lifecycle_step(&life,&s);
      if(life.network.state!=MTTL_NET_STOPPING||m.commits!=baseline)++failed;
      mttl_lifecycle_step(&life,&s);m.now+=15000;
     }
     m.join_ok=1;m.association=1;m.address=0xc000020au;
     mttl_wifi_submit(&life.network,(const uint8_t *)"Saved",5,(const uint8_t *)"password",8,m.now);
     mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);mttl_lifecycle_step(&life,&s);
     if(life.network.state!=MTTL_NET_ONLINE||m.commits!=baseline+1)++failed;
    }
    return failed;
}
