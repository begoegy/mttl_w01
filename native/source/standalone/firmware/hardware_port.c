#include "hardware_port.h"
#include <string.h>
static const uint8_t led_pair[][3]={{0,4,5},{0,2,3},{0,0,1},{1,0,1}};
static void main_indicator(uint8_t out[4]){out[1]&=0xf3;if((out[0]&0x15)||(out[1]&1))out[1]|=4;else if((out[0]&0x2a)==0x2a&&(out[1]&2))out[1]|=8;}
int mttl_hardware_indicators(mttl_hardware_port *p,uint8_t known,uint8_t on){uint8_t out[4];unsigned i;const uint8_t *pair;
 if(!p||!p->verified||!p->write||p->active)return 0;memcpy(out,p->shadow,4);out[1]&=0xcf;out[2]&=0xc0;
 for(i=0;i<4;i++){pair=led_pair[i];out[pair[0]]&=~((1u<<pair[1])|(1u<<pair[2]));if(known&(1u<<i))out[pair[0]]|=1u<<pair[(on&(1u<<i))?1:2];}
 main_indicator(out);if(!p->write(p->context,out,4))return 0;memcpy(p->shadow,out,4);return 1;
}
void mttl_hardware_init(mttl_hardware_port *p){memset(p,0,sizeof(*p));}
int mttl_hardware_begin(void *context,uint8_t ch,uint8_t on){
    static const uint8_t primary[][3]={{1,4,5},{2,0,1},{2,2,3},{2,4,5}};
    mttl_hardware_port *p=context;const uint8_t *pair;unsigned i;uint8_t out[4];
    if(!p||!p->verified||!p->write||p->active||ch>=4||on>1)return 0;
    memcpy(out,p->shadow,4);for(i=0;i<2;i++){pair=i?led_pair[ch]:primary[ch];out[pair[0]]=(uint8_t)((out[pair[0]]&~((1u<<pair[1])|(1u<<pair[2])))|(1u<<pair[on?1:2]));}main_indicator(out);
    if(!p->write(p->context,out,4))return 0;memcpy(p->shadow,out,4);p->active=(uint8_t)(ch+1);return 1;
}
int mttl_hardware_end(void *context,uint8_t ch){mttl_hardware_port *p=context;uint8_t out[4];if(!p||!p->verified||!p->write||ch>=4||p->active!=ch+1)return 0;
    memcpy(out,p->shadow,4);out[1]&=0xcf;out[2]&=0xc0;
    if(!p->write(p->context,out,4))return 0;memcpy(p->shadow,out,4);p->active=0;return 1;
}
void mttl_hardware_sample(mttl_sample *s,int ok,int32_t value,uint32_t now){mttl_sample_update(s,value,ok!=0,now);}
void mttl_button_input_init(mttl_button_input *p,uint32_t ms){memset(p,0,sizeof(*p));p->debounce_ms=ms;}
int mttl_button_input_poll(mttl_button_input *p,int ok,uint8_t pressed,uint32_t now){
    if(!p)return 0;
    if(!ok||pressed>1){p->initialized=p->armed=p->hold_notified=0;memset(&p->button,0,sizeof(p->button));return 0;}
    if(!p->debounce_ms||p->debounce_ms>=0x80000000u)return 0;
    if(!p->initialized){p->initialized=1;p->candidate=p->stable=pressed;p->changed_at=now;return 0;}
    if(p->candidate!=pressed){p->candidate=pressed;p->changed_at=now;return 0;}
    if(now-p->changed_at<p->debounce_ms)return 0;
    if(!p->armed){if(!pressed){p->armed=1;p->stable=0;}return 0;}
    if(p->stable==pressed)return 0;
    p->stable=pressed;return mttl_button_edge(&p->button,pressed,now);
}
int mttl_main_button_input_poll(mttl_button_input *p,int ok,uint8_t pressed,uint32_t now){
    if(!p||!p->debounce_ms||p->debounce_ms>=0x80000000u)return 0;
    int event=mttl_button_input_poll(p,ok,pressed,now);
    if(!p||!ok||pressed>1)return 0;
    if(event){int result=p->hold_notified?0:event;p->hold_notified=0;return result;}
    if(p->armed&&p->stable&&pressed&&p->button.down&&!p->hold_notified&&now-p->button.pressed_at>=10000){p->hold_notified=1;return 2;}
    return 0;
}
