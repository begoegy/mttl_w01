#include "ntp_transport.h"
int mttl_ntp_transport_init(mttl_ntp_transport *t,uint32_t server){
    if(!t||!server||(server>>24)==0||(server>>24)==127||(server>>24)>=224)return 0;
    t->server=server;t->generation=0;t->opened=0;return 1;
}
static int close_socket(mttl_ntp_transport *t,const mttl_ntp_io *io){
    if(!t->opened)return 1;
    if(io->close(io->context)!=1)return 0;
    t->opened=0;return 1;
}
int mttl_ntp_transport_tick(mttl_ntp_transport *t,mttl_ntp_client *c,const mttl_ntp_io *io,int online,uint32_t now){
    uint8_t packet[49],token[8];uint32_t source=0;uint16_t port=0;int n;
    if(!t||!c||!io||!io->open||!io->close||!io->send||!io->recv||!io->entropy)return 0;
    mttl_ntp_link(c,online,now);
    if(!online){close_socket(t,io);return 0;}
    /* Expire before reading, so late packets cannot set time. */
    mttl_ntp_due(c,now);
    if(t->opened&&(!c->waiting||t->generation!=c->generation)){
        if(!close_socket(t,io))return 0;
    }
    if(t->opened){
        n=io->recv(io->context,packet,sizeof(packet),&source,&port);
        if(n<0){mttl_ntp_send_failed(c,now,t->generation);close_socket(t,io);return 0;}
        if(n==48&&source==t->server&&port==123&&mttl_ntp_accept(c,now,t->generation,packet,48)){
            close_socket(t,io);return 1;
        }
        return 0;
    }
    if(!mttl_ntp_due(c,now))return 0;
    /* Account failed entropy/open attempts using the client's bounded retry. */
    {unsigned i;for(i=0;i<8;i++)token[i]=0;}
    if(io->entropy(io->context,token,8)!=1){
        c->started=now;c->delay=5000;return 0;
    }
    if(!mttl_ntp_begin(c,now,token,packet,&t->generation)){
        c->started=now;c->delay=5000;return 0;
    }
    if(io->open(io->context,t->server,123)!=1){mttl_ntp_send_failed(c,now,t->generation);return 0;}
    t->opened=1;
    if(io->send(io->context,packet,48)!=48){mttl_ntp_send_failed(c,now,t->generation);close_socket(t,io);}
    return 0;
}
