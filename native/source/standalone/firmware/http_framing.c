#include "http_framing.h"
static int eq(const uint8_t *s,size_t n,const char *key){size_t k;for(k=0;k<n&&key[k];k++){unsigned c=s[k],d=(unsigned char)key[k];if(c>='A'&&c<='Z')c+=32;if(d>='A'&&d<='Z')d+=32;if(c!=d)return 0;}return k==n&&!key[k];}
int mttl_http_framing(const uint8_t *p,size_t n,uint32_t *length){
 size_t at=0,end,start,colon,k;int seen=0;uint32_t value=0;
 if(!p||!length||n<4)return 0;
 while(at+1<n&&!(p[at]=='\r'&&p[at+1]=='\n')){if(p[at]<32||p[at]==127)return 0;at++;}
 if(at+1>=n)return 0;at+=2;
 while(at+1<n){start=at;end=at;
  while(end+1<n&&!(p[end]=='\r'&&p[end+1]=='\n'))end++;
  if(end+1>=n)return 0;
  if(end==start){if(end+2!=n)return 0;*length=value;return 1;}
  colon=start;while(colon<end&&p[colon]!=':')colon++;
  if(colon==start||colon==end||p[start]==' '||p[start]=='\t')return 0;
  for(k=start;k<colon;k++){unsigned c=p[k];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='!'||c=='#'||c=='$'||c=='%'||c=='&'||c=='\''||c=='*'||c=='+'||c=='.'||c=='^'||c=='`'||c=='|'||c=='~'))return 0;}
  for(k=colon+1;k<end;k++)if((p[k]<32&&p[k]!='\t')||p[k]==127)return 0;
  if(eq(p+start,colon-start,"transfer-encoding"))return 0;
  if(eq(p+start,colon-start,"content-length")){
   if(seen++)return 0;k=colon+1;while(k<end&&(p[k]==' '||p[k]=='\t'))k++;
   if(k==end||p[k]<'0'||p[k]>'9')return 0;
   while(k<end&&p[k]>='0'&&p[k]<='9'){uint32_t d=p[k++]-'0';if(value>(0x7fffffffu-d)/10u)return 0;value=value*10+d;}
   while(k<end&&(p[k]==' '||p[k]=='\t'))k++;if(k!=end)return 0;
  }
  at=end+2;
 }
 return 0;
}
/* Exact case-insensitive names; reject duplicate requested fields. */
int mttl_http_field(const uint8_t *p,size_t n,const char *key,const uint8_t **value,size_t *size){
 uint32_t length;size_t at=0,end,colon,start,stop;int found=0;
 if(!key||!value||!size)return 0;*value=0;*size=0;
 if(!mttl_http_framing(p,n,&length))return 0;
 while(at+1<n&&!(p[at]=='\r'&&p[at+1]=='\n'))at++;at+=2;
 while(at+1<n){end=at;while(end+1<n&&!(p[end]=='\r'&&p[end+1]=='\n'))end++;
  if(end==at)break;colon=at;while(colon<end&&p[colon]!=':')colon++;
  if(eq(p+at,colon-at,key)){
   if(found){*value=0;*size=0;return 0;}found=1;start=colon+1;stop=end;
   while(start<stop&&(p[start]==' '||p[start]=='\t'))start++;
   while(stop>start&&(p[stop-1]==' '||p[stop-1]=='\t'))stop--;
   *value=p+start;*size=stop-start;
  }
  at=end+2;
 }
 return found;
}
