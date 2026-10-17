#include "sfn_boot_guard_core.h"
#include "ota_hash.h"
#include <string.h>
#define GUARD_MAGIC 0x31474653u /* SFG1 little-endian */
#define GUARD_HEADER_BYTES 20u
#define GUARD_ATTEMPTS_OFF 20u
#define GUARD_TRUSTED_OFF 24u
#define GUARD_ATTEMPT_LIMIT 2u
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static int read_exact(const sfn_boot_guard_io *io,uint32_t a,uint8_t *p,size_t n){return io&&io->read&&p&&n&&io->read(io->context,a,p,n)==1;}
static int write_exact(const sfn_boot_guard_io *io,uint32_t a,const uint8_t *p,size_t n){uint8_t check[256];size_t pos=0,k;if(!io||!io->write||!p||!n)return 0;while(pos<n){k=n-pos;if(k>sizeof(check))k=sizeof(check);if(io->write(io->context,a+pos,p+pos,k)!=1||!read_exact(io,a+pos,check,k)||memcmp(check,p+pos,k))return 0;pos+=k;}return 1;}
static int sector_blank(const sfn_boot_guard_io *io,uint32_t a){uint8_t b[256];unsigned i,j;for(i=0;i<4096;i+=sizeof(b)){if(!read_exact(io,a+i,b,sizeof(b)))return 0;for(j=0;j<sizeof(b);j++)if(b[j]!=255)return 0;}return 1;}
static int erase_blank(const sfn_boot_guard_io *io,uint32_t a){return io&&io->erase_sector&&io->erase_sector(io->context,a)==1&&sector_blank(io,a);}
static int geometry(const sfn_boot_guard_descriptor *d){return d&&d->first==0xb000u&&d->second==0x80000u&&d->guard_sector==0xed000u&&d->flash_size==0x100000u&&d->rescue_bytes>4096&&d->rescue_bytes<=0x70000u&&d->marker&&d->marker_size>=16&&d->marker_size<=64&&d->rescue_sector0&&d->rescue_sha256&&d->rescue_tail_sha256&&d->marker_offset>=40&&d->marker_offset+ d->marker_size<d->rescue_bytes&&d->guard_sector+4096<=d->flash_size;}
static int signature(const sfn_boot_guard_io *io,uint32_t slot){uint8_t b[8];return read_exact(io,slot+8,b,8)?mttl_ota_signature(b):MTTL_OTA_INVALID;}
static int marker_at(const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io,uint32_t slot){uint8_t b[64];if(d->marker_size>sizeof(b)||!read_exact(io,slot+d->marker_offset,b,d->marker_size))return 0;return !memcmp(b,d->marker,d->marker_size);}
static int locate(const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io,uint32_t *native,uint32_t *rescue){int a=marker_at(d,io,d->first),b=marker_at(d,io,d->second);if(a==b)return 0;*native=a?d->first:d->second;*rescue=a?d->second:d->first;return 1;}
static int digest_range(const sfn_boot_guard_io *io,uint32_t address,uint32_t bytes,uint32_t normalize_base,uint8_t out[32]){mttl_ota_hash h;uint8_t b[256];uint32_t pos=0;unsigned i;mttl_ota_hash_init(&h);while(pos<bytes){size_t n=bytes-pos;if(n>sizeof(b))n=sizeof(b);if(!read_exact(io,address+pos,b,n)){(void)mttl_ota_hash_finish(&h,out);return 0;}if(normalize_base!=0xffffffffu){for(i=0;i<n;i++){uint32_t at=normalize_base+pos+i;if(at>=8&&at<16)b[i]=255;}}if(!mttl_ota_hash_update(&h,b,n)){(void)mttl_ota_hash_finish(&h,out);return 0;}pos+=(uint32_t)n;}return mttl_ota_hash_finish(&h,out);}
static int rescue_full_ok(const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io,uint32_t slot){uint8_t h[32];int sig=signature(io,slot);if(sig!=MTTL_OTA_OLD&&sig!=MTTL_OTA_NEW)return 0;return digest_range(io,slot,d->rescue_bytes,0,h)&&!memcmp(h,d->rescue_sha256,32);}
static int rescue_tail_ok(const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io,uint32_t slot){uint8_t h[32];return digest_range(io,slot+4096,d->rescue_bytes-4096,0xffffffffu,h)&&!memcmp(h,d->rescue_tail_sha256,32);}
static void expected_header(const sfn_boot_guard_descriptor *d,uint32_t native,uint32_t rescue,uint8_t out[GUARD_HEADER_BYTES]){memset(out,255,GUARD_HEADER_BYTES);put32(out,GUARD_MAGIC);put32(out+4,native);put32(out+8,rescue);put32(out+12,d->rescue_bytes);put32(out+16,d->marker_offset);}
static int guard_header(const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io,uint32_t native,uint32_t rescue,uint32_t *attempts,uint32_t *trusted){uint8_t actual[GUARD_HEADER_BYTES],wanted[GUARD_HEADER_BYTES],w[4];expected_header(d,native,rescue,wanted);if(!read_exact(io,d->guard_sector,actual,sizeof(actual))||memcmp(actual,wanted,sizeof(actual)))return 0;if(!read_exact(io,d->guard_sector+GUARD_ATTEMPTS_OFF,w,4))return 0;*attempts=le32(w);if(!read_exact(io,d->guard_sector+GUARD_TRUSTED_OFF,w,4))return 0;*trusted=le32(w);return 1;}
static unsigned cleared(uint32_t v){unsigned n=0;while(v!=0xffffffffu){n+=(~v)&1u;v=(v>>1)|0x80000000u;}return n;}
static int clear_attempt_bit(const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io,uint32_t attempts){uint8_t b[4];unsigned n=cleared(attempts);uint32_t next;if(n>=32)return 0;next=attempts&~(1u<<n);put32(b,next);return write_exact(io,d->guard_sector+GUARD_ATTEMPTS_OFF,b,4);}
static int write_guard_header(const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io,uint32_t native,uint32_t rescue){uint8_t h[GUARD_HEADER_BYTES];expected_header(d,native,rescue,h);return write_exact(io,d->guard_sector,h,sizeof(h));}
int sfn_boot_guard_start(sfn_boot_guard_state *s,const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io,uint32_t now){uint32_t native,rescue,attempts=0xffffffffu,trusted=0xffffffffu;int blank,header,full;
 if(!s||!geometry(d)||!io||!io->read||!io->write||!io->erase_sector)return SFN_BOOT_GUARD_FAILED;
 memset(s,0,sizeof(*s));
 if(!locate(d,io,&native,&rescue))return SFN_BOOT_GUARD_UNAVAILABLE;
 s->native_slot=native;s->rescue_slot=rescue;s->started_ms=now;
 blank=sector_blank(io,d->guard_sector);header=!blank&&guard_header(d,io,native,rescue,&attempts,&trusted);full=rescue_full_ok(d,io,rescue);
 if(!full){/* A nonblank guard record plus an exact rescue tail means a prior rollback may have lost power while rebuilding sector 0. */
  if(blank||!rescue_tail_ok(d,io,rescue))return SFN_BOOT_GUARD_UNAVAILABLE;
  s->available=1;s->armed=1;return SFN_BOOT_GUARD_ROLLBACK_READY;
 }
 s->available=1;
 if(blank){if(!write_guard_header(d,io,native,rescue))return SFN_BOOT_GUARD_FAILED;attempts=0xffffffffu;trusted=0xffffffffu;}
 else if(!header){s->armed=1;return SFN_BOOT_GUARD_ROLLBACK_READY;}
 if(trusted==0){s->trusted=1;return SFN_BOOT_GUARD_TRUSTED;}
 if(cleared(attempts)>=GUARD_ATTEMPT_LIMIT){s->armed=1;return SFN_BOOT_GUARD_ROLLBACK_READY;}
 if(!clear_attempt_bit(d,io,attempts))return SFN_BOOT_GUARD_FAILED;
 s->armed=1;return SFN_BOOT_GUARD_ARMED;
}
int sfn_boot_guard_mark_trusted(sfn_boot_guard_state *s,const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io){uint8_t zero[4]={0,0,0,0};if(!s||!s->armed||s->trusted||!d||!io)return 0;if(!write_exact(io,d->guard_sector+GUARD_TRUSTED_OFF,zero,4))return 0;s->trusted=1;s->armed=0;return 1;}
int sfn_boot_guard_should_timeout(const sfn_boot_guard_state *s,uint32_t now,uint32_t timeout){return s&&s->armed&&!s->trusted&&timeout&&((uint32_t)(now-s->started_ms)>=timeout);}
static int restore_sector0(const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io,uint32_t rescue){uint8_t chunk[256],readback[256];uint32_t off;unsigned i;if(!erase_blank(io,rescue))return 0;for(off=0;off<4096;off+=sizeof(chunk)){memcpy(chunk,d->rescue_sector0+off,sizeof(chunk));for(i=0;i<sizeof(chunk);i++){uint32_t at=off+i;if(at>=8&&at<16)chunk[i]=(uint8_t)"81958711"[at-8];}if(io->write(io->context,rescue+off,chunk,sizeof(chunk))!=1||!read_exact(io,rescue+off,readback,sizeof(readback))||memcmp(chunk,readback,sizeof(chunk)))return 0;}return rescue_full_ok(d,io,rescue)&&signature(io,rescue)==MTTL_OTA_NEW;}
static int word_write(const sfn_boot_guard_io *io,uint32_t address,uint32_t value,uint32_t *readback){uint8_t b[4],before[4],after[4];put32(b,value);if(!read_exact(io,address,before,4)||((le32(before)&value)!=value)||io->write(io->context,address,b,4)!=1||!read_exact(io,address,after,4))return 0;*readback=le32(after);return *readback==value;}
int sfn_boot_guard_restore(sfn_boot_guard_state *s,const sfn_boot_guard_descriptor *d,const sfn_boot_guard_io *io){mttl_ota_layout l;mttl_ota_commit c;uint32_t a,v,r;int sig;if(!s||!s->available||!d||!io||!rescue_tail_ok(d,io,s->rescue_slot))return 0;sig=signature(io,s->rescue_slot);if(sig!=MTTL_OTA_NEW||!rescue_full_ok(d,io,s->rescue_slot)){if(!restore_sector0(d,io,s->rescue_slot))return 0;}l.first=d->first;l.second=d->second;l.protected_start=d->guard_sector;l.flash_size=d->flash_size;if(!mttl_ota_commit_init(&c,&l,s->native_slot,s->rescue_slot,1))return 0;while(mttl_ota_commit_action(&c,&a,&v)){int ok;r=0;ok=word_write(io,a,v,&r);if(!mttl_ota_commit_ack(&c,ok,r))return 0;}if(!mttl_ota_commit_done(&c))return 0;(void)erase_blank(io,d->guard_sector);return 1;}
