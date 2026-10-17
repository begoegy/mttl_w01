#include "wifi_form.h"
static unsigned char lower(unsigned char c){return c>='A'&&c<='Z'?c+32:c;}
int mttl_wifi_form_type(const char *type){
    static const char base[]="application/x-www-form-urlencoded";
    static const char suffix[]="charset=utf-8";
    size_t i=0,j;
    if(!type)return 0;
    while(type[i]&&i<128)++i;
    if(i==128)return 0;
    i=0;while(type[i]==' '||type[i]=='\t')++i;
    for(j=0;base[j];++j)if(lower((unsigned char)type[i++])!=base[j])return 0;
    while(type[i]==' '||type[i]=='\t')++i;
    if(!type[i])return 1;
    if(type[i++]!=';')return 0;
    while(type[i]==' '||type[i]=='\t')++i;
    for(j=0;suffix[j];++j)if(lower((unsigned char)type[i++])!=suffix[j])return 0;
    while(type[i]==' '||type[i]=='\t')++i;
    return !type[i];
}
static int hex(unsigned char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static int decode(const unsigned char *p,size_t n,char *out,size_t cap){
    size_t i=0,j=0;
    while(i<n){unsigned char c=p[i++];if(c=='+')c=' ';else if(c=='%'){int a,b;if(i+1>=n)return 0;a=hex(p[i]);b=hex(p[i+1]);if(a<0||b<0)return 0;c=(unsigned char)(a*16+b);i+=2;}
        if(c<32||c==127||j>=cap-1)return 0;out[j++]=(char)c;
    }out[j]=0;return 1;
}
int mttl_wifi_form(const unsigned char *body,size_t length,char ssid[33],char password[65]){
    size_t pos=0,i,start;unsigned seen=0;
    ssid[0]=password[0]=0;if(!body||!length||length>320)return 0;
    while(pos<length){start=pos;while(pos<length&&body[pos]!='&')++pos;i=start;while(i<pos&&body[i]!='=')++i;
        if(i==pos)return 0;
        if(i-start==4&&body[start]=='s'&&body[start+1]=='s'&&body[start+2]=='i'&&body[start+3]=='d'){
            if(seen&1)return 0;seen|=1;if(!decode(body+i+1,pos-i-1,ssid,33))return 0;
        }else if(i-start==8&&body[start]=='p'&&body[start+1]=='a'&&body[start+2]=='s'&&body[start+3]=='s'&&body[start+4]=='w'&&body[start+5]=='o'&&body[start+6]=='r'&&body[start+7]=='d'){
            if(seen&2)return 0;seen|=2;if(!decode(body+i+1,pos-i-1,password,65))return 0;
        }else return 0;
        if(pos<length&&++pos==length)return 0;
    }return seen==3&&ssid[0];
}
