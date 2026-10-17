#include "lifecycle.h"
#include "deadline.h"
void mttl_lifecycle_init(mttl_lifecycle *l) {
    mttl_network_init(&l->network);l->ap_started=l->join_started=l->dhcp_started=l->web_started=0;l->applied_generation=0;l->waiting=0;l->waiting_since=0;
}
static void recover(mttl_lifecycle *l,const mttl_services *s){mttl_provision(&l->network,s->clock(s->context));}
void mttl_lifecycle_step(mttl_lifecycle *l,const mttl_services *s){
    mttl_network *n=&l->network;
    uint32_t now=s->clock(s->context),generation;
    int result;
    mttl_network_tick(n,now);
    /* A timed-out cleanup is not terminal. Retry the ownership barrier,
     * never start a second radio session while the old one is still active. */
    if(n->state==MTTL_NET_FAULT){
        if(l->waiting!=3){l->waiting=3;l->waiting_since=now;}
        if(!mttl_remaining(l->waiting_since,now,15000)){recover(l,s);l->waiting=0;}
        return;
    }
    if(n->state!=MTTL_NET_STOPPING&&!(n->state==MTTL_NET_AP&&!l->ap_started))l->waiting=0;
    if(n->state==MTTL_NET_STOPPING){
        if(l->waiting!=1){l->waiting=1;l->waiting_since=now;}
        if(!mttl_remaining(l->waiting_since,now,10000)){n->state=MTTL_NET_FAULT;return;}
        generation=n->generation;
        result=s->stop(s->context);
        now=s->clock(s->context);
        if(result && mttl_stopped(n,generation,now)){
            l->ap_started=l->join_started=l->dhcp_started=l->web_started=0;
            l->applied_generation=generation;
            l->waiting=0;
        }
        return;
    }
    if(n->state==MTTL_NET_AP && !l->ap_started){
        if(l->waiting!=2){l->waiting=2;l->waiting_since=now;}
        if(!mttl_remaining(l->waiting_since,now,5000)){recover(l,s);l->waiting=0;return;}
        result=s->ap(s->context);
        if(result<0)return; /* pending TCP/IP setup, not ready for HTTP */
        if(!result){recover(l,s);return;}
        l->ap_started=1;
        l->waiting=0;
        /* AP also requires setup web pages. web failure triggers full stop. */
        if(!s->web(s->context)){recover(l,s);return;}
        l->web_started=1;return;
    }
    if(n->state==MTTL_NET_JOINING){
        generation=n->generation;
        if(!l->join_started){
            /* AP HTTP must finish its submission response before this worker
             * stops it. stop is a complete ownership barrier. */
            if(!s->stop(s->context)){mttl_network_tick(n,s->clock(s->context));return;}
            l->web_started=l->ap_started=0;
            if(!s->join(s->context,n->ssid,n->password)){recover(l,s);return;}
            l->join_started=1;l->applied_generation=generation;
        }
        now=s->clock(s->context);mttl_network_tick(n,now);
        if(n->state!=MTTL_NET_JOINING)return;
        result=s->associated(s->context);
        if(result<0){recover(l,s);return;}
        if(result>0)mttl_associated(n,generation,s->clock(s->context));
        return;
    }
    if(n->state==MTTL_NET_DHCP){
        generation=n->generation;
        if(!l->dhcp_started){if(!s->dhcp(s->context)){recover(l,s);return;}l->dhcp_started=1;}
        now=s->clock(s->context);mttl_network_tick(n,now);
        if(n->state!=MTTL_NET_DHCP)return;
        {
            uint32_t address=s->ip(s->context);
            if(address&&!mttl_got_ip(n,generation,address,s->clock(s->context),s->commit,s->context))recover(l,s);
        }
        return;
    }
    if(n->state==MTTL_NET_ONLINE){
        if(s->associated(s->context)!=1){recover(l,s);return;}
        if(!l->web_started){if(!s->web(s->context)){recover(l,s);return;}l->web_started=1;}
    }
}
