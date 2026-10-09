/** Official published known-answer vectors and failure-contract regressions. */
#include "security/crypto.h"
#include "security/crypto_openssl.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void decode(const char* hex, uint8_t* output, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        unsigned value;
        assert(sscanf(hex + i * 2, "%2x", &value) == 1);
        output[i] = (uint8_t)value;
    }
}
static void unchanged(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; ++i) { assert(data[i] == 0xa5); }
}
static void gcm_vector(nx_crypto_algorithm_t algorithm, size_t key_size,
                        const char* expected_ciphertext, const char* expected_tag) {
    uint8_t key[32] = {0}, nonce[12] = {0}, plaintext[16] = {0};
    uint8_t ciphertext[16], tag[16], output[16], expected[16];
    assert(nx_crypto_seal(algorithm, key, key_size, nonce, NULL, 0,
        plaintext, sizeof(plaintext), ciphertext, tag) == NX_CRYPTO_OK);
    decode(expected_ciphertext, expected, sizeof(expected));
    assert(memcmp(ciphertext, expected, sizeof(expected)) == 0);
    decode(expected_tag, expected, sizeof(expected));
    assert(memcmp(tag, expected, sizeof(expected)) == 0);
    assert(nx_crypto_open(algorithm, key, key_size, nonce, NULL, 0,
        ciphertext, sizeof(ciphertext), tag, output) == NX_CRYPTO_OK);
    assert(memcmp(output, plaintext, sizeof(output)) == 0);
    for (size_t i = 0; i < sizeof(ciphertext); ++i) {
        ciphertext[i] ^= 1;
        memset(output, 0xa5, sizeof(output));
        assert(nx_crypto_open(algorithm, key, key_size, nonce, NULL, 0,
            ciphertext, sizeof(ciphertext), tag, output) == NX_CRYPTO_AUTH_FAILED);
        unchanged(output, sizeof(output));
        ciphertext[i] ^= 1;
    }
    tag[0] ^= 1;
    memset(output, 0xa5, sizeof(output));
    assert(nx_crypto_open(algorithm, key, key_size, nonce, NULL, 0,
        ciphertext, sizeof(ciphertext), tag, output) == NX_CRYPTO_AUTH_FAILED);
    unchanged(output, sizeof(output));
    tag[0] ^= 1;
    nonce[0] ^= 1;
    assert(nx_crypto_open(algorithm, key, key_size, nonce, NULL, 0,
        ciphertext, sizeof(ciphertext), tag, output) == NX_CRYPTO_AUTH_FAILED);
    unchanged(output, sizeof(output));
    nonce[0] ^= 1;
    key[0] ^= 1;
    assert(nx_crypto_open(algorithm, key, key_size, nonce, NULL, 0,
        ciphertext, sizeof(ciphertext), tag, output) == NX_CRYPTO_AUTH_FAILED);
    unchanged(output, sizeof(output));
    key[0] ^= 1;
    const uint8_t aad[] = "board-identity";
    assert(nx_crypto_open(algorithm, key, key_size, nonce, aad, sizeof(aad),
        ciphertext, sizeof(ciphertext), tag, output) == NX_CRYPTO_AUTH_FAILED);
    unchanged(output, sizeof(output));
}
static nx_crypto_status_t entropy_failure(void* ctx, uint8_t* output, size_t size) {
    (void)ctx;
    memset(output, 0xa5, size);
    return NX_CRYPTO_ENTROPY_FAILED;
}

int main(void) {
    assert(!nx_crypto_is_available() && nx_crypto_get_provider() == NULL);
    assert(nx_crypto_openssl_provider() && !nx_crypto_is_available());
    assert(nx_crypto_set_provider(nx_crypto_openssl_provider()) == NX_CRYPTO_OK);
    gcm_vector(NX_CRYPTO_AES128_GCM, 16,
        "0388dace60b6a392f328c2b971b2fe78", "ab6e47d42cec13bdf53a67b21257bddf");
    gcm_vector(NX_CRYPTO_AES256_GCM, 32,
        "cea7403d4d606b6e074ec5d3baf39d18", "d0d1c8a799996bf0265b98b5d48ab919");
    uint8_t digest[32], expected[32];
    assert(nx_crypto_sha256((const uint8_t*)"abc", 3, digest) == NX_CRYPTO_OK);
    decode("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", expected, 32);
    assert(memcmp(digest, expected, 32) == 0);
    uint8_t public_key[32], signature[64];
    decode("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", public_key, 32);
    decode("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555f"
           "b8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b", signature, 64);
    assert(nx_crypto_verify_ed25519(public_key, NULL, 0, signature) == NX_CRYPTO_OK);
    signature[0] ^= 1;
    assert(nx_crypto_verify_ed25519(public_key, NULL, 0, signature) == NX_CRYPTO_AUTH_FAILED);
    uint8_t first[32], second[32];
    assert(nx_crypto_random(first, sizeof(first)) == NX_CRYPTO_OK);
    assert(nx_crypto_set_provider(nx_crypto_openssl_provider()) == NX_CRYPTO_OK);
    assert(nx_crypto_random(second, sizeof(second)) == NX_CRYPTO_OK);
    assert(memcmp(first, second, sizeof(first)) != 0);
    nx_crypto_provider_t failing = *nx_crypto_get_provider();
    failing.random = entropy_failure;
    assert(nx_crypto_set_provider(&failing) == NX_CRYPTO_OK);
    assert(nx_crypto_random(first, sizeof(first)) == NX_CRYPTO_ENTROPY_FAILED);
    for (size_t i = 0; i < sizeof(first); ++i) { assert(first[i] == 0); }
    assert(nx_crypto_set_provider(NULL) == NX_CRYPTO_UNSUPPORTED);
    assert(nx_crypto_random(first, sizeof(first)) == NX_CRYPTO_UNSUPPORTED);
    assert(nx_crypto_sha256(NULL, 0, digest) == NX_CRYPTO_UNSUPPORTED);
    assert(!nx_crypto_is_available());
    assert(nx_crypto_set_provider(nx_crypto_openssl_provider()) == NX_CRYPTO_OK);
    puts("crypto: NIST GCM, SHA256, RFC8032 Ed25519, tamper, entropy and fail-closed checks passed");
    return 0;
}
