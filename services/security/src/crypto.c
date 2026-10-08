#include "security/crypto.h"
#include <stdlib.h>
#include <string.h>

#if defined(NX_SECURITY_OPENSSL) && NX_SECURITY_OPENSSL
extern const nx_crypto_provider_t* nx_crypto_openssl_provider(void);
extern const nx_crypto_provider_t nx_crypto_openssl_default_provider;
static const nx_crypto_provider_t* g_provider = &nx_crypto_openssl_default_provider;
#else
static const nx_crypto_provider_t* g_provider;
#endif

void nx_crypto_secure_zero(void* data, size_t size) {
    volatile uint8_t* bytes = (volatile uint8_t*)data;
    while (size-- > 0) { *bytes++ = 0; }
}

nx_crypto_status_t nx_crypto_set_provider(const nx_crypto_provider_t* provider) {
    if (provider && !provider->random && !provider->seal && !provider->open &&
        !provider->sha256 && !provider->verify_ed25519) {
        return NX_CRYPTO_INVALID_PARAM;
    }
    g_provider = provider;
    return provider ? NX_CRYPTO_OK : NX_CRYPTO_UNSUPPORTED;
}

nx_crypto_status_t nx_crypto_use_default_provider(void) {
#if defined(NX_SECURITY_OPENSSL) && NX_SECURITY_OPENSSL
    return nx_crypto_set_provider(nx_crypto_openssl_provider());
#else
    return nx_crypto_set_provider(NULL);
#endif
}

bool nx_crypto_is_available(void) {
    return g_provider != NULL;
}

const nx_crypto_provider_t* nx_crypto_get_provider(void) {
    (void)nx_crypto_is_available();
    return g_provider;
}

nx_crypto_status_t nx_crypto_random(uint8_t* output, size_t size) {
    if (!output || size == 0) { return NX_CRYPTO_INVALID_PARAM; }
    if (!nx_crypto_is_available() || !g_provider->random) { return NX_CRYPTO_UNSUPPORTED; }
    nx_crypto_status_t status = g_provider->random(g_provider->context, output, size);
    if (status != NX_CRYPTO_OK) { nx_crypto_secure_zero(output, size); }
    return status;
}

static bool valid_aead(nx_crypto_algorithm_t algorithm, const uint8_t* key,
                       size_t key_size, const uint8_t* nonce,
                       const uint8_t* aad, size_t aad_size,
                       const uint8_t* input, size_t input_size,
                       const uint8_t* tag, const uint8_t* output) {
    size_t expected = algorithm == NX_CRYPTO_AES128_GCM ? 16u :
                      algorithm == NX_CRYPTO_AES256_GCM ? 32u : 0u;
    return expected && key && key_size == expected && nonce && tag && output &&
           (aad || aad_size == 0) && (input || input_size == 0) &&
           input_size <= NX_CRYPTO_MAX_AEAD_SIZE && aad_size <= NX_CRYPTO_MAX_AEAD_SIZE;
}

nx_crypto_status_t nx_crypto_seal(nx_crypto_algorithm_t algorithm,
    const uint8_t* key, size_t key_size, const uint8_t nonce[NX_CRYPTO_NONCE_SIZE],
    const uint8_t* aad, size_t aad_size, const uint8_t* input, size_t input_size,
    uint8_t* output, uint8_t tag[NX_CRYPTO_TAG_SIZE]) {
    if (!valid_aead(algorithm, key, key_size, nonce, aad, aad_size, input,
                    input_size, tag, output)) { return NX_CRYPTO_INVALID_PARAM; }
    if (!nx_crypto_is_available() || !g_provider->seal) { return NX_CRYPTO_UNSUPPORTED; }
    nx_crypto_status_t status = g_provider->seal(g_provider->context, algorithm,
        key, nonce, aad, aad_size, input, input_size, output, tag);
    if (status != NX_CRYPTO_OK) {
        nx_crypto_secure_zero(output, input_size);
        nx_crypto_secure_zero(tag, NX_CRYPTO_TAG_SIZE);
    }
    return status;
}

nx_crypto_status_t nx_crypto_open(nx_crypto_algorithm_t algorithm,
    const uint8_t* key, size_t key_size, const uint8_t nonce[NX_CRYPTO_NONCE_SIZE],
    const uint8_t* aad, size_t aad_size, const uint8_t* input, size_t input_size,
    const uint8_t tag[NX_CRYPTO_TAG_SIZE], uint8_t* output) {
    if (!valid_aead(algorithm, key, key_size, nonce, aad, aad_size, input,
                    input_size, tag, output)) { return NX_CRYPTO_INVALID_PARAM; }
    if (!nx_crypto_is_available() || !g_provider->open) { return NX_CRYPTO_UNSUPPORTED; }
    uint8_t* scratch = (uint8_t*)malloc(input_size ? input_size : 1u);
    if (!scratch) { return NX_CRYPTO_NO_MEMORY; }
    nx_crypto_status_t status = g_provider->open(g_provider->context, algorithm,
        key, nonce, aad, aad_size, input, input_size, tag, scratch);
    if (status == NX_CRYPTO_OK && input_size) { memcpy(output, scratch, input_size); }
    nx_crypto_secure_zero(scratch, input_size);
    free(scratch);
    return status;
}

nx_crypto_status_t nx_crypto_sha256(const uint8_t* input, size_t input_size,
                                   uint8_t output[NX_CRYPTO_SHA256_SIZE]) {
    if ((!input && input_size) || !output) { return NX_CRYPTO_INVALID_PARAM; }
    if (!nx_crypto_is_available() || !g_provider->sha256) { return NX_CRYPTO_UNSUPPORTED; }
    nx_crypto_status_t status = g_provider->sha256(g_provider->context, input, input_size, output);
    if (status != NX_CRYPTO_OK) { nx_crypto_secure_zero(output, NX_CRYPTO_SHA256_SIZE); }
    return status;
}

nx_crypto_status_t nx_crypto_verify_ed25519(
    const uint8_t key[NX_CRYPTO_ED25519_PUBLIC_KEY_SIZE], const uint8_t* message,
    size_t message_size, const uint8_t signature[NX_CRYPTO_ED25519_SIGNATURE_SIZE]) {
    if (!key || (!message && message_size) || !signature) { return NX_CRYPTO_INVALID_PARAM; }
    if (!nx_crypto_is_available() || !g_provider->verify_ed25519) { return NX_CRYPTO_UNSUPPORTED; }
    return g_provider->verify_ed25519(g_provider->context, key, message, message_size, signature);
}
