#include "sfn_backend.h"
int sfn_backend_valid(uint32_t ipv4,uint16_t port){
    unsigned first;
    if(!port)return 0;
    if(!ipv4)return 1;
    first=(unsigned)(ipv4>>24);
    return first!=0&&first!=127&&first<224&&ipv4!=0xffffffffu;
}
int sfn_backend_pin(mttl_settings *settings,uint32_t ipv4,uint16_t port){
    if(!settings||!ipv4||!sfn_backend_valid(ipv4,port))return 0;
    if(settings->sfn_backend_ipv4==ipv4&&settings->sfn_backend_port==port)return 0;
    settings->sfn_backend_ipv4=ipv4;settings->sfn_backend_port=port;return 1;
}
