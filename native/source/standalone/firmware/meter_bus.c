#include "meter_bus.h"
typedef struct {mttl_meter_bus *io;uint8_t clock,data,address;} transaction;
static int set(transaction *t,uint8_t pin,int release){int ok=t->io->line(t->io->context,pin,release)==1;t->io->delay(t->io->context);return ok;}
static int high(transaction *t){unsigned i;if(!set(t,t->clock,1))return 0;for(i=0;i<t->io->high_attempts;i++){int value=t->io->sense(t->io->context,t->clock);if(value==1)return 1;if(value<0)return 0;t->io->delay(t->io->context);}return 0;}
static int start(transaction *t){return set(t,t->data,1)&&high(t)&&t->io->sense(t->io->context,t->data)==1&&set(t,t->data,0)&&set(t,t->clock,0);}
static int stop(transaction *t){int a=set(t,t->clock,0),b=set(t,t->data,0),c=high(t),d=set(t,t->data,1);return a&&b&&c&&d;}
static void release(transaction *t){/* Best-effort cleanup even after timeout. */set(t,t->data,1);set(t,t->clock,1);}
static int put(transaction *t,uint8_t value){unsigned i;int ack;for(i=0;i<8;i++){if(!set(t,t->data,(value&0x80)!=0)||!high(t)||!set(t,t->clock,0))return 0;value<<=1;}if(!set(t,t->data,1)||!high(t))return 0;ack=t->io->sense(t->io->context,t->data);if(!set(t,t->clock,0))return 0;return ack==0;}
static int get(transaction *t,uint8_t *out,int last){unsigned i;uint8_t value=0;if(!set(t,t->data,1))return 0;for(i=0;i<8;i++){int bit;if(!high(t))return 0;bit=t->io->sense(t->io->context,t->data);if(bit<0||bit>1)return 0;value=(uint8_t)((value<<1)|bit);if(!set(t,t->clock,0))return 0;}if(!set(t,t->data,last)||!high(t)||!set(t,t->clock,0)||!set(t,t->data,1))return 0;*out=value;return 1;}
static int prepare(transaction *t,mttl_meter_bus *io,unsigned ch){if(!io||ch>=4||!io->line||!io->sense||!io->delay||!io->high_attempts||io->high_attempts>100)return 0;t->io=io;t->clock=ch<2?0x25:0x20;t->data=ch<2?0x24:0x21;t->address=(ch&1)?0:2;return 1;}
static int transfer(void *context,unsigned ch,uint8_t reg,uint8_t *read_data,const uint8_t *write_data,size_t size){transaction t;unsigned i;int ok;if(!size||size>9||(!read_data&&!write_data)||!prepare(&t,context,ch))return 0;
 ok=start(&t)&&put(&t,t.address)&&put(&t,reg);
 if(read_data){if(ok)ok=start(&t)&&put(&t,(uint8_t)(t.address|1));for(i=0;ok&&i<size;i++)ok=get(&t,read_data+i,i+1==size);}
 else for(i=0;ok&&i<size;i++)ok=put(&t,write_data[i]);
 {int ended=stop(&t);release(&t);return ok&&ended;}
}
int mttl_meter_bus_read(void *context,unsigned ch,uint8_t reg,uint8_t *data,size_t n){if(!data)return 0;return transfer(context,ch,reg,data,0,n);}
int mttl_meter_bus_write(void *context,unsigned ch,uint8_t reg,const uint8_t *data,size_t n){if(!data)return 0;return transfer(context,ch,reg,0,data,n);}
static int output_transfer(mttl_meter_bus *io,uint8_t reg,const uint8_t *data,size_t n){
 transaction t;unsigned i;int ok,ended;
 if(!io||!data||!n||n>19||!io->line||!io->sense||!io->delay||!io->high_attempts||io->high_attempts>100)return 0;
 t.io=io;t.clock=0x12;t.data=0x13;t.address=0xa8;
 ok=start(&t)&&put(&t,t.address)&&put(&t,reg);
 for(i=0;ok&&i<n;i++)ok=put(&t,data[i]);
 ended=stop(&t);release(&t);return ok&&ended;
}
int mttl_output_bus_write(mttl_meter_bus *io,const uint8_t *data,size_t n){return n==4?output_transfer(io,0x13,data,n):0;}
int mttl_output_bus_initialize(mttl_meter_bus *io){uint8_t config[19];unsigned i;for(i=0;i<19;i++)config[i]=0xff;return output_transfer(io,0,config,sizeof(config));}
