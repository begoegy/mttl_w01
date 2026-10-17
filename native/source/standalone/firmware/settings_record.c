#include "settings_record.h"
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t *p,uint32_t x){unsigned i;for(i=0;i<4;i++)p[i]=(uint8_t)(x>>(8*i));}
static uint32_t crc_update(uint32_t x,const uint8_t *p,size_t n){unsigned j;while(n--){x^=*p++;for(j=0;j<8;j++)x=(x>>1)^((0u-(x&1u))&0xedb88320u);}return x;}
static uint32_t record_crc(const uint8_t *slot,size_t size){return ~crc_update(crc_update(0xffffffffu,slot,12),slot+16,size);}
#define ENERGY_OFFSET 1536
#define RELAY_OFFSET 1600
#define SCHEDULE_OFFSET 1640
#define SFN_OFFSET 1720
static uint32_t schedule_crc(const uint8_t *s){return ~crc_update(crc_update(0xffffffffu,s,12),s+SCHEDULE_OFFSET,54);}
static int schedule_present(const uint8_t *s){return s[SCHEDULE_OFFSET]=='S'&&s[SCHEDULE_OFFSET+1]=='C'&&s[SCHEDULE_OFFSET+2]=='0'&&s[SCHEDULE_OFFSET+3]=='1';}
static uint32_t sfn_crc(const uint8_t *s){return ~crc_update(crc_update(0xffffffffu,s,12),s+SFN_OFFSET,16);}
static int sfn_present(const uint8_t *s){return s[SFN_OFFSET]=='S'&&s[SFN_OFFSET+1]=='F'&&s[SFN_OFFSET+2]=='N'&&s[SFN_OFFSET+3]=='1';}
static uint32_t relay_crc(const uint8_t *s){return ~crc_update(crc_update(0xffffffffu,s,12),s+RELAY_OFFSET,6);}
static int relay_present(const uint8_t *s){return s[RELAY_OFFSET]=='R'&&s[RELAY_OFFSET+1]=='L'&&s[RELAY_OFFSET+2]=='0'&&s[RELAY_OFFSET+3]=='1';}
static uint32_t energy_crc(const uint8_t *slot){return ~crc_update(crc_update(0xffffffffu,slot,12),slot+ENERGY_OFFSET,37);}
static int energy_present(const uint8_t *slot){return slot[ENERGY_OFFSET]=='E'&&slot[ENERGY_OFFSET+1]=='N'&&slot[ENERGY_OFFSET+2]=='0'&&slot[ENERGY_OFFSET+3]=='1';}
int mttl_settings_encode(uint8_t *slot,const uint8_t *body,size_t size,uint32_t sequence){
    size_t i,n;const uint8_t *energy=0,*relay=0,*schedule=0,*sfn=0;if(!slot||!body||!size||size>MTTL_SETTINGS_SLOT-17)return 0;
    /* The config codec's bounded envelope is transformed only when its shape
     * is exact. Ordinary/unknown record bodies keep their original encoding. */
    if(size>=41&&size<=1024&&(body[0]==2||body[0]==3)){n=(size_t)body[1]|((size_t)body[2]<<8);if(n==size-(body[0]==3?38:36)&&(body[3]==1||body[3]==4||body[3]==5||body[3]==6)){energy=body+3+n;if(body[0]==3)relay=body+n+36;body+=3;size=n;}}
    /* Preserve a v4 physical base readable by older firmware. Newer SFN
     * extensions live in sequence-bound CRC sidecars in the same atomic record. */
    if(size>=85&&size<=1024&&body[0]==6){sfn=body+size-12;size-=12;}
    if(size>=73&&size<=1024&&(body[0]==5||body[0]==6)){schedule=body+size-50;size-=50;}
    for(i=0;i<MTTL_SETTINGS_SLOT;i++)slot[i]=255;
    slot[0]='M';slot[1]='T';slot[2]='0';slot[3]='2';put32(slot+4,sequence);put32(slot+8,(uint32_t)size);
    for(i=0;i<size;i++)slot[16+i]=body[i];
    if(schedule||sfn)slot[16]=4;
    /* CRC covers version/magic, sequence, length and body, omitting CRC field. */
    put32(slot+12,record_crc(slot,size));
    if(energy){slot[ENERGY_OFFSET]='E';slot[ENERGY_OFFSET+1]='N';slot[ENERGY_OFFSET+2]='0';slot[ENERGY_OFFSET+3]='1';for(i=0;i<33;i++)slot[ENERGY_OFFSET+4+i]=energy[i];put32(slot+ENERGY_OFFSET+37,energy_crc(slot));}
    if(relay){slot[RELAY_OFFSET]='R';slot[RELAY_OFFSET+1]='L';slot[RELAY_OFFSET+2]='0';slot[RELAY_OFFSET+3]='1';slot[RELAY_OFFSET+4]=relay[0];slot[RELAY_OFFSET+5]=relay[1];put32(slot+RELAY_OFFSET+6,relay_crc(slot));}
    if(schedule){slot[SCHEDULE_OFFSET]='S';slot[SCHEDULE_OFFSET+1]='C';slot[SCHEDULE_OFFSET+2]='0';slot[SCHEDULE_OFFSET+3]='1';for(i=0;i<50;i++)slot[SCHEDULE_OFFSET+4+i]=schedule[i];put32(slot+SCHEDULE_OFFSET+54,schedule_crc(slot));}
    if(sfn){slot[SFN_OFFSET]='S';slot[SFN_OFFSET+1]='F';slot[SFN_OFFSET+2]='N';slot[SFN_OFFSET+3]='1';for(i=0;i<12;i++)slot[SFN_OFFSET+4+i]=sfn[i];put32(slot+SFN_OFFSET+16,sfn_crc(slot));}
    /* Final commit is deliberately left erased. Storage driver must program
     * it only after reading back and verifying the staged record. */
    return 1;
}
int mttl_settings_validate(const uint8_t *slot,uint32_t *sequence,size_t *size){
    uint32_t n;if(!slot||!sequence||!size||slot[MTTL_SETTINGS_SLOT-1]!=0)return 0;
    if(slot[0]!='M'||slot[1]!='T'||slot[2]!='0'||slot[3]!='2')return 0;
    n=get32(slot+8);if(!n||n>MTTL_SETTINGS_SLOT-17)return 0;
    if(get32(slot+12)!=record_crc(slot,n))return 0;
    if(n<=1024){unsigned i;int blank=1;for(i=0;i<41;i++)if(slot[ENERGY_OFFSET+i]!=255)blank=0;
        if(!blank&&(!energy_present(slot)||get32(slot+ENERGY_OFFSET+37)!=energy_crc(slot)))return 0;}
    if(n<=1024){unsigned i;int blank=1;for(i=0;i<10;i++)if(slot[RELAY_OFFSET+i]!=255)blank=0;
      if(!blank&&(!relay_present(slot)||get32(slot+RELAY_OFFSET+6)!=relay_crc(slot)||slot[RELAY_OFFSET+4]>15||(slot[RELAY_OFFSET+5]&~slot[RELAY_OFFSET+4])))return 0;}
    if(n<=1024){unsigned i;int blank=1;for(i=0;i<58;i++)if(slot[SCHEDULE_OFFSET+i]!=255)blank=0;
      if(!blank&&(!schedule_present(slot)||slot[16]!=4||get32(slot+SCHEDULE_OFFSET+54)!=schedule_crc(slot)))return 0;}
    if(n<=1024){unsigned i;int blank=1;for(i=0;i<20;i++)if(slot[SFN_OFFSET+i]!=255)blank=0;
      if(!blank&&(!sfn_present(slot)||!schedule_present(slot)||slot[16]!=4||get32(slot+SFN_OFFSET+16)!=sfn_crc(slot)))return 0;}
    *sequence=get32(slot+4);*size=n;return 1;
}
int mttl_settings_extract(const uint8_t *slot,uint8_t *out,size_t cap,size_t *size){
 uint32_t sequence;size_t n,i,base,extra;int extended,scheduled,sfn_ext;
 if(!out||!size||!mttl_settings_validate(slot,&sequence,&n))return 0;
 extended=n<=1024&&energy_present(slot);scheduled=n<=1024&&schedule_present(slot);sfn_ext=n<=1024&&sfn_present(slot);extra=(scheduled?50:0)+(sfn_ext?12:0);
 if(n+extra+(extended?(relay_present(slot)?38:36):0)>cap)return 0;
 base=extended?3:0;for(i=0;i<n;i++)out[base+i]=slot[16+i];
 if(scheduled){out[base]=5;for(i=0;i<50;i++)out[base+n+i]=slot[SCHEDULE_OFFSET+4+i];n+=50;}
 if(sfn_ext){if(!scheduled)return 0;out[base]=6;for(i=0;i<12;i++)out[base+n+i]=slot[SFN_OFFSET+4+i];n+=12;}
 if(extended){out[0]=relay_present(slot)?3:2;out[1]=(uint8_t)n;out[2]=(uint8_t)(n>>8);for(i=0;i<33;i++)out[3+n+i]=slot[ENERGY_OFFSET+4+i];*size=n+36;if(relay_present(slot)){out[n+36]=slot[RELAY_OFFSET+4];out[n+37]=slot[RELAY_OFFSET+5];*size=n+38;}}
 else *size=n;return 1;
}
int mttl_settings_choose(const uint8_t *a,const uint8_t *b){
    uint32_t x=0,y=0,d;size_t n;int av=mttl_settings_validate(a,&x,&n),bv=mttl_settings_validate(b,&y,&n);
    if(!av&&!bv)return -1;if(!av)return 1;if(!bv)return 0;
    d=y-x;if(!d||d==0x80000000u)return -2;return d<0x80000000u?1:0;
}
