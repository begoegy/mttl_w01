#include "sfn_ota_http.h"
#include <string.h>
static int ci(char a,char b){if(a>='A'&&a<='Z')a=(char)(a+32);if(b>='A'&&b<='Z')b=(char)(b+32);return a==b;}
static int prefix_ci(const uint8_t *p,size_t n,const char *s){size_t i,m=strlen(s);if(n<m)return 0;for(i=0;i<m;i++)if(!ci((char)p[i],s[i]))return 0;return 1;}
static const uint8_t *line_end(const uint8_t *p,const uint8_t *end){while(p+1<end){if(p[0]=='\r'&&p[1]=='\n')return p;p++;}return 0;}
void sfn_fwr_decoder_init(sfn_fwr_decoder *d){if(d)memset(d,0,sizeof(*d));}
int sfn_fwr_decode(sfn_fwr_decoder *d,uint8_t *data,size_t n){static const uint8_t seed[16]={0xca,0x59,0x23,0xe7,0xd7,0x9a,0x84,0xe8,0x43,0xb3,0xaf,0xe7,0x94,0xb1,0xaf,0xa7};size_t i;if(!d||!data||!n||d->offset>0x70000u||n>0x70000u-d->offset)return 0;for(i=0;i<n;i++){uint32_t at=d->offset+(uint32_t)i;if(at<16u)d->key[at]=(uint8_t)((seed[at]^data[i])+1u);else data[i]^=d->key[at&15u];}d->offset+=(uint32_t)n;return 1;}

int sfn_ota_integrity_init(sfn_ota_integrity *s,uint32_t bytes,const uint8_t expected[32]){
 unsigned i;uint8_t any=0;if(!s||!expected||bytes<44u||bytes>0x6a014u)return 0;memset(s,0,sizeof(*s));for(i=0;i<32;i++)any|=expected[i];if(!any)return 0;memcpy(s->expected,expected,32);s->expected_bytes=bytes;mttl_ota_hash_init(&s->hash);s->active=1;return 1;
}
int sfn_ota_integrity_feed(sfn_ota_integrity *s,const uint8_t *data,size_t n){
 if(!s||!s->active||s->failed||!data||!n||n>s->expected_bytes-s->received){if(s)s->failed=1;return 0;}if(!mttl_ota_hash_update(&s->hash,data,n)){s->failed=1;return 0;}s->received+=(uint32_t)n;return 1;
}
int sfn_ota_integrity_finish(sfn_ota_integrity *s){
 uint8_t digest[32];int ok;if(!s||!s->active||s->failed||s->received!=s->expected_bytes){if(s)s->failed=1;return 0;}ok=mttl_ota_hash_finish(&s->hash,digest)&&!memcmp(digest,s->expected,32);memset(digest,0,sizeof(digest));s->active=0;if(!ok)s->failed=1;return ok;
}
int sfn_ota_http_header(const uint8_t *data,size_t n,uint32_t *content_length,size_t *body_offset){const uint8_t *p,*e,*end;uint32_t length=0;int have_length=0,have_type=0;if(!data||!content_length||!body_offset||n>1024)return -1;end=data+n;e=line_end(data,end);if(!e)return n==1024?-1:0;if(!((size_t)(e-data)>=12&&(!memcmp(data,"HTTP/1.1 200",12)||!memcmp(data,"HTTP/1.0 200",12))))return -1;p=e+2;while(p<end){e=line_end(p,end);if(!e)return n==1024?-1:0;if(e==p){*content_length=length;*body_offset=(size_t)(e+2-data);return have_length&&have_type&&length>=44u&&length<=0x70000u?1:-1;}if(prefix_ci(p,(size_t)(e-p),"Content-Length:")){const uint8_t *q=p+15;uint64_t v=0;unsigned digits=0;if(have_length)return -1;while(q<e&&(*q==' '||*q=='\t'))q++;while(q<e&&*q>='0'&&*q<='9'){v=v*10u+(unsigned)(*q-'0');if(v>0xffffffffu)return -1;q++;digits++;}while(q<e&&(*q==' '||*q=='\t'))q++;if(!digits||q!=e)return -1;length=(uint32_t)v;have_length=1;}else if(prefix_ci(p,(size_t)(e-p),"Content-Type:")){const uint8_t *q=p+13;while(q<e&&(*q==' '||*q=='\t'))q++;if((size_t)(e-q)!=24||!prefix_ci(q,(size_t)(e-q),"application/octet-stream"))return -1;have_type=1;}p=e+2;}return n==1024?-1:0;}
