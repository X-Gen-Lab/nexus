#include "security/crypto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(nx_crypto_use_default_provider()==NX_CRYPTO_OK);
    nx_crypto_provider_t full=*nx_crypto_get_provider();
    nx_crypto_provider_t partial=full;
    partial.verify_ed25519=NULL;
    assert(nx_crypto_set_provider(&partial)==NX_CRYPTO_OK);
    const uint8_t key[32]={0}, nonce[12]={0}, message[4]={1,2,3,4};
    uint8_t encrypted[4], clear[4], tag[16], digest[32], signature[64]={0};
    assert(nx_crypto_sha256(message,sizeof(message),digest)==NX_CRYPTO_OK);
    assert(nx_crypto_seal(NX_CRYPTO_AES256_GCM,key,32,nonce,NULL,0,message,4,
                         encrypted,tag)==NX_CRYPTO_OK);
    assert(nx_crypto_open(NX_CRYPTO_AES256_GCM,key,32,nonce,NULL,0,encrypted,4,
                         tag,clear)==NX_CRYPTO_OK);
    assert(!memcmp(clear,message,4));
    assert(nx_crypto_verify_ed25519(key,message,4,signature)==NX_CRYPTO_UNSUPPORTED);
    partial.random=NULL;
    assert(nx_crypto_set_provider(&partial)==NX_CRYPTO_OK);
    assert(nx_crypto_random(clear,sizeof(clear))==NX_CRYPTO_UNSUPPORTED);
    partial.sha256=NULL;
    assert(nx_crypto_set_provider(&partial)==NX_CRYPTO_OK);
    assert(nx_crypto_sha256(message,4,digest)==NX_CRYPTO_UNSUPPORTED);
    partial.open=NULL;
    assert(nx_crypto_set_provider(&partial)==NX_CRYPTO_OK);
    memset(clear,0xa5,sizeof(clear));
    assert(nx_crypto_open(NX_CRYPTO_AES256_GCM,key,32,nonce,NULL,0,encrypted,4,
                         tag,clear)==NX_CRYPTO_UNSUPPORTED);
    for(size_t i=0;i<sizeof(clear);++i) assert(clear[i]==0xa5);
    nx_crypto_provider_t empty={0};
    assert(nx_crypto_set_provider(&empty)==NX_CRYPTO_INVALID_PARAM);
    assert(nx_crypto_set_provider(&full)==NX_CRYPTO_OK);
    puts("partial crypto providers: AEAD independent of optional verifier; missing capabilities fail closed");
    return 0;
}
