#include "security/crypto.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
int main(void) {
    uint8_t key[32] = {0}, nonce[12] = {0}, data[16] = {0}, tag[16] = {0};
    assert(!nx_crypto_is_available());
    assert(nx_crypto_get_provider() == NULL);
    assert(nx_crypto_set_provider(NULL) == NX_CRYPTO_UNSUPPORTED);
    assert(nx_crypto_random(data, sizeof(data)) == NX_CRYPTO_UNSUPPORTED);
    assert(nx_crypto_sha256(data, sizeof(data), key) == NX_CRYPTO_UNSUPPORTED);
    assert(nx_crypto_seal(NX_CRYPTO_AES256_GCM, key, sizeof(key), nonce,
        NULL, 0, data, sizeof(data), data, tag) == NX_CRYPTO_UNSUPPORTED);
    assert(nx_crypto_open(NX_CRYPTO_AES256_GCM, key, sizeof(key), nonce,
        NULL, 0, data, sizeof(data), tag, data) == NX_CRYPTO_UNSUPPORTED);
    puts("crypto: unconfigured provider fails closed");
    return 0;
}
