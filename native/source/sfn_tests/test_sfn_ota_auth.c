#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../standalone/firmware/sfn_ota_auth.h"
#include "sfn_ota_public_key.h"

static uint8_t expected_message[SFN_OTA_AUTH_MESSAGE_BYTES];
static uint8_t expected_signature[SFN_OTA_SIGNATURE_BYTES];
static int rom_calls;
static int force_fail;

int rom_ed25519_verify_signature(const unsigned char sig[64],const unsigned char *message,unsigned long long length,const unsigned char public_key[32]){
    static const uint8_t expected_public[32]={
        0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,
        0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a
    };
    ++rom_calls;
    assert(length==SFN_OTA_AUTH_MESSAGE_BYTES);
    assert(!memcmp(message,expected_message,SFN_OTA_AUTH_MESSAGE_BYTES));
    assert(!memcmp(public_key,expected_public,sizeof(expected_public)));
    assert(!memcmp(public_key,sfn_ota_public_key,sizeof(expected_public)));
    assert(!memcmp(sig,expected_signature,SFN_OTA_SIGNATURE_BYTES));
    return force_fail?-1:0;
}

int main(void){
    uint8_t sha[32],message[SFN_OTA_AUTH_MESSAGE_BYTES],zero[32]={0};
    unsigned i;
    for(i=0;i<32;i++)sha[i]=(uint8_t)(i*7u+3u);
    for(i=0;i<SFN_OTA_SIGNATURE_BYTES;i++)expected_signature[i]=(uint8_t)(0xa5u^i);
    memset(expected_message,0,sizeof(expected_message));
    memcpy(expected_message,"SFN-OTA-ED25519",15);
    expected_message[15]=0;
    expected_message[16]=0;expected_message[17]=0;expected_message[18]=0x4e;expected_message[19]=0x30; /* 20016 */
    expected_message[20]=0;expected_message[21]=0x04;expected_message[22]=0xee;expected_message[23]=0x20; /* 323104 */
    memcpy(expected_message+24,sha,32);
    assert(sfn_ota_auth_message(20016u,323104u,sha,message));
    assert(!memcmp(message,expected_message,sizeof(message)));
    assert(!sfn_ota_auth_message(0,323104u,sha,message));
    assert(!sfn_ota_auth_message(20016u,43u,sha,message));
    assert(!sfn_ota_auth_message(20016u,323104u,zero,message));
    rom_calls=0;force_fail=0;
    assert(sfn_ota_auth_verify(20016u,323104u,sha,expected_signature,sizeof(expected_signature)));
    assert(rom_calls==1);
    force_fail=1;
    assert(!sfn_ota_auth_verify(20016u,323104u,sha,expected_signature,sizeof(expected_signature)));
    assert(rom_calls==2);
    assert(!sfn_ota_auth_verify(20016u,323104u,sha,expected_signature,sizeof(expected_signature)-1));
    assert(rom_calls==2);
    puts("SFN_OTA_AUTH_WIRING=PASS message=56 public_key=32 signature=64 rom_success_and_failure=PASS");
    return 0;
}
