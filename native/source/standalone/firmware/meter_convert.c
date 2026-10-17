#include "meter_convert.h"
#include <string.h>
int mttl_meter_temperature(uint32_t raw,int32_t *out){
 static const int16_t thresholds[118]={212,240,267,295,322,350,377,405,432,460,487,515,542,570,597,625,652,680,707,736,761,786,811,836,861,886,911,936,942,995,1048,1101,1154,1208,1261,1314,1367,1420,1474,1576,1639,1694,1793,1863,1946,2007,2091,2170,2246,2363,2478,2543,2633,2767,2853,2957,3081,3192,3318,3436,3554,3704,3870,3992,4173,4279,4441,4584,4736,4903,5056,5234,5397,5600,5786,5999,6154,6377,6576,6779,7055,7231,7437,7663,7909,8249,8418,8773,8957,9136,9481,9738,10137,10435,10678,10984,11347,11612,11977,12306,12637,12927,13398,13737,14113,14472,15002,15355,15703,16163,16430,16863,17395,17787,18158,18614,19059,19326};
 int32_t q;unsigned i;if(!out||raw>0xffffffu)return 0;
 /* Preserve stock 16-bit narrowing after division, then its range check. */
 q=(int16_t)(raw/10);if(q<212||q>=212+0x4aab)return 0;
 for(i=0;i<118;i++)if(q<=thresholds[i]){*out=(int32_t)(i+1)*10;return 1;}return 0;
}
void mttl_meter_power_init(mttl_meter_power_state *p){if(p)memset(p,0,sizeof(*p));}
static int power_step(mttl_meter_power_state *p,const mttl_meter_power_input *in,mttl_meter_power_result *out,int measured){
 uint32_t carry=0;unsigned ch,j;if(!p||!in||!out||!in->factor||in->factor>20)return 0;
 memset(out,0,sizeof(*out));
 for(ch=0;ch<4;ch++){
  int32_t v=0,signed_raw;uint32_t prior;int ok=(in->read_mask>>ch)&1;
  if(in->raw[ch]>0xffffffu||in->current[ch]>0xffffffu)ok=0;
  out->power[ch]=p->power[ch];if(!ok)continue;
  out->current[ch]=in->current[ch];out->current_valid|=1u<<ch;
  signed_raw=(int32_t)(in->raw[ch]&0x7fffffu);if(in->raw[ch]&0x800000u)signed_raw-=0x800000;
  if(measured)v=signed_raw*in->factor-60;
  else{p->history[ch][p->index[ch]]=signed_raw*in->factor-60;p->index[ch]=(p->index[ch]+1)&3;
   for(j=0;j<4;j++)v+=p->history[ch][j];v/=4;}
  if(ch==1){carry=out->current[0]/1000;v+=(v<-600?50:30)*(int32_t)carry;}
  else if(ch==2){prior=out->current[0]+out->current[1];if(v>=201||v<=-201){carry=prior/1000;v+=(v>=201?-24:38)*(int32_t)carry;}}
  else if(ch==3){carry=(out->current[0]+out->current[1]+out->current[2])/1000;v-=48*(int32_t)carry;}
  if(carry)v+=60;if(v<0)v=-v;
  if(v<220||(((in->known_mask>>ch)&1)&&!((in->on_mask>>ch)&1))||(!measured&&!((in->on_mask>>ch)&1)))v=0;else if(v>=4500001)continue;
  /* Software relay knowledge is mandatory; never guess contact state. */
  if(!measured&&!((in->known_mask>>ch)&1))continue;
  p->power[ch]=out->power[ch]=v;out->power_valid|=1u<<ch;
 }
 return 1;
}
int mttl_meter_power_step(mttl_meter_power_state *p,const mttl_meter_power_input *in,mttl_meter_power_result *out){return power_step(p,in,out,0);}
int mttl_meter_measured_power_step(mttl_meter_power_state *p,const mttl_meter_power_input *in,mttl_meter_power_result *out){return power_step(p,in,out,1);}
void mttl_meter_energy_init(mttl_meter_energy_state *p){if(p){memset(p,0,sizeof(*p));p->previous=p->restart=0xffffffffu;}}
int mttl_meter_checkpoint_step(const mttl_meter_checkpoint *prior,int ok,uint32_t raw,mttl_meter_checkpoint *next){
 uint32_t delta;mttl_meter_checkpoint candidate;
 if(!prior||!next||!ok||raw>0xffffffu||prior->valid>1||prior->raw>0xffffffu||prior->value>0x7fffffffu)return 0;
 candidate=*prior;
 if(!prior->valid){candidate.raw=raw;candidate.value=raw;candidate.valid=1;}
 else{
  /* A lower valid counter starts a new epoch. Never add the same raw twice. */
  delta=raw<prior->raw?raw:raw-prior->raw;
  if(delta>0x7fffffffu-prior->value)return 0;
  candidate.raw=raw;candidate.value+=delta;
 }
 *next=candidate;return 1;
}
int mttl_meter_energy_step(mttl_meter_energy_state *p,int ok,uint32_t raw,int *reset){
 uint32_t sum;if(!p||!reset)return 0;*reset=0;if(!ok||raw>0xffffffu)return 0;
 if(p->previous==0xffffffffu){if(raw<10)p->previous=raw;return 0;}
 if(raw<p->previous){
  if(p->restart==0xffffffffu){p->restart=raw;return 0;}
  if(p->restart<=raw&&raw-p->restart<10){p->restart=raw;if(++p->count<5)return 0;p->base+=p->previous;p->previous=0;*reset=1;}
  p->count=0;p->restart=0xffffffffu;return 0;
 }
 p->count=0;p->restart=0xffffffffu;if(raw-p->previous>=10)return 0;
 sum=p->base+raw;p->value=sum;if(sum<10000000u)p->previous=raw;
 else{p->value=p->base=sum-9999999u;p->previous=0;*reset=1;}return 1;
}
