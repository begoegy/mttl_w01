#include "sfn_ota_auth.h"
#include "sfn_ota_public_key.h"
#include <string.h>

/* Realtek Ameba ROM ABI is documented by the vendor HomeKit Ed25519 header/source:
 * signature[64], message, uint64 message length, public key[32]. */
extern int rom_ed25519_verify_signature(const unsigned char sig[64],const unsigned char *message,unsigned long long length,const unsigned char public_key[32]);
static const uint8_t domain[16]={'S','F','N','-','O','T','A','-','E','D','2','5','5','1','9',0};

int sfn_ota_auth_message(uint32_t sequence,uint32_t bytes,const uint8_t sha[32],uint8_t out[SFN_OTA_AUTH_MESSAGE_BYTES]){
    unsigned i;uint8_t any=0;
    if(!out||!sha||!sequence||bytes<44u||bytes>0x6a014u)return 0;
    for(i=0;i<32;i++)any|=sha[i];
    if(!any)return 0;
    memcpy(out,domain,sizeof(domain));
    out[16]=(uint8_t)(sequence>>24);out[17]=(uint8_t)(sequence>>16);out[18]=(uint8_t)(sequence>>8);out[19]=(uint8_t)sequence;
    out[20]=(uint8_t)(bytes>>24);out[21]=(uint8_t)(bytes>>16);out[22]=(uint8_t)(bytes>>8);out[23]=(uint8_t)bytes;
    memcpy(out+24,sha,32);
    return 1;
}

int sfn_ota_auth_verify(uint32_t sequence,uint32_t bytes,const uint8_t sha[32],const uint8_t *signature,size_t signature_bytes){
    uint8_t message[SFN_OTA_AUTH_MESSAGE_BYTES];int ok;
    if(!signature||signature_bytes!=SFN_OTA_SIGNATURE_BYTES||!sfn_ota_auth_message(sequence,bytes,sha,message))return 0;
    ok=rom_ed25519_verify_signature(signature,message,(unsigned long long)sizeof(message),sfn_ota_public_key)==0;
    memset(message,0,sizeof(message));
    return ok;
}
