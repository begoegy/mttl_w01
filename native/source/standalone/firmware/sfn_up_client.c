#include "sfn_up_client.h"
#include <string.h>

static uint32_t delay_ms(uint8_t level){
    static const uint32_t delays[]={1000u,2000u,4000u,8000u,16000u,30000u};
    return delays[level<6?level:5];
}
static int elapsed(uint32_t now,uint32_t then,uint32_t delay){return (uint32_t)(now-then)>=delay;}
static void clear_buffers(sfn_up_client *c){c->rx_size=0;c->tx_size=0;c->tx_sent=0;c->boot_sent=0;}
static void close_link(sfn_up_client *c,const sfn_up_io *io,uint32_t now,int failed){
    if(io&&io->close)(void)io->close(io->context);
    clear_buffers(c);
    c->state=SFN_UP_LINK_BACKOFF;c->backoff_at=now;
    if(failed)++c->io_failures;
    if(c->backoff_level<5)++c->backoff_level;
}
static int queue(sfn_up_client *c,const char *line,size_t n){
    if(!c||!line||!n||n>SFN_UP_TX_CAP||c->tx_size)return 0;
    memcpy(c->tx,line,n);c->tx_size=(uint16_t)n;c->tx_sent=0;return 1;
}
static int flush_tx(sfn_up_client *c,const sfn_up_io *io,uint32_t now){
    int n;if(!c->tx_size)return 1;
    n=io->send(io->context,c->tx+c->tx_sent,(size_t)c->tx_size-c->tx_sent);
    if(n<0){close_link(c,io,now,1);return -1;}
    if(n==0)return 0;
    if((size_t)n>(size_t)c->tx_size-c->tx_sent){close_link(c,io,now,1);return -1;}
    c->tx_sent=(uint16_t)(c->tx_sent+(uint16_t)n);
    if(c->tx_sent==c->tx_size){c->tx_size=0;c->tx_sent=0;return 1;}
    return 0;
}
static int extract_line(sfn_up_client *c,char line[SFN_UP_RX_CAP+1],size_t *size){
    size_t i,n,consume;
    for(i=0;i<c->rx_size;i++)if(c->rx[i]=='\n')break;
    if(i==c->rx_size)return 0;
    n=i+1;consume=n;
    if(n>SFN_UP_RX_CAP)return -1;
    memcpy(line,c->rx,n);line[n]=0;*size=n;
    memmove(c->rx,c->rx+consume,c->rx_size-consume);c->rx_size=(uint16_t)(c->rx_size-consume);
    return 1;
}
void sfn_up_client_init(sfn_up_client *c){if(c){memset(c,0,sizeof(*c));c->state=SFN_UP_LINK_OFF;}}
void sfn_up_client_reset(sfn_up_client *c,const sfn_up_io *io){if(!c)return;if(io&&io->close)(void)io->close(io->context);sfn_up_client_init(c);}
int sfn_up_client_ready(const sfn_up_client *c){return c&&c->state==SFN_UP_LINK_ONLINE&&c->boot_sent&&!c->tx_size;}
int sfn_up_client_publish(sfn_up_client *c,const char *line,size_t size){return sfn_up_client_ready(c)&&size<=SFN_UP_TX_CAP?queue(c,line,size):0;}
int sfn_up_client_step(sfn_up_client *c,const sfn_up_io *io,int network_online,const char *host,uint16_t port,uint32_t now,const char *model,const char mac[13],const char *firmware,sfn_up_command_handler handler,void *context){
    int rc,n;char line[SFN_UP_RX_CAP+1];size_t line_size,response_size;
    if(!c||!io||!io->open||!io->close||!io->send||!io->receive||!host||!model||!mac||!firmware)return 0;
    if(!network_online){if(c->state!=SFN_UP_LINK_OFF)sfn_up_client_reset(c,io);return 1;}
    if(c->state==SFN_UP_LINK_OFF)c->state=SFN_UP_LINK_CONNECTING;
    if(c->state==SFN_UP_LINK_BACKOFF){if(!elapsed(now,c->backoff_at,delay_ms(c->backoff_level)))return 1;c->state=SFN_UP_LINK_CONNECTING;}
    if(c->state==SFN_UP_LINK_CONNECTING){
        rc=io->open(io->context,host,port);if(rc<0){close_link(c,io,now,1);return 1;}if(rc==0)return 1;
        c->state=SFN_UP_LINK_ONLINE;c->connected_at=now;c->backoff_level=0;++c->reconnects;clear_buffers(c);
        response_size=sfn_up_encode_bootinfo((char *)c->tx,sizeof(c->tx),model,mac,firmware);
        if(!response_size){close_link(c,io,now,1);return 0;}c->tx_size=(uint16_t)response_size;
    }
    if(c->state!=SFN_UP_LINK_ONLINE)return 1;
    if(c->tx_size){rc=flush_tx(c,io,now);if(rc<=0)return 1;if(!c->boot_sent){c->boot_sent=1;return 1;}}
    if(c->rx_size==SFN_UP_RX_CAP){close_link(c,io,now,1);return 1;}
    n=io->receive(io->context,c->rx+c->rx_size,SFN_UP_RX_CAP-c->rx_size);
    if(n<0){close_link(c,io,now,1);return 1;}
    if(n>0){if((size_t)n>SFN_UP_RX_CAP-c->rx_size){close_link(c,io,now,1);return 1;}c->rx_size=(uint16_t)(c->rx_size+(uint16_t)n);}
    rc=extract_line(c,line,&line_size);
    if(rc<0){close_link(c,io,now,1);return 1;}
    if(!rc){if(c->rx_size==SFN_UP_RX_CAP){close_link(c,io,now,1);}return 1;}
    {sfn_up_command command;if(!sfn_up_parse_command(line,line_size,&command)){++c->rejected;return 1;}++c->commands;
     if(handler){response_size=handler(context,&command,(char *)c->tx,sizeof(c->tx));if(response_size){if(response_size>sizeof(c->tx)){close_link(c,io,now,1);return 0;}c->tx_size=(uint16_t)response_size;c->tx_sent=0;}}
    }
    return 1;
}
