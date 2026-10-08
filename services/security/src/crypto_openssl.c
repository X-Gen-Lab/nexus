#include "security/crypto.h"
#include <limits.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

static nx_crypto_status_t random_bytes(void* ctx, uint8_t* data, size_t size) {
    (void)ctx;
    return RAND_bytes_ex(NULL, data, size, 256) == 1 ? NX_CRYPTO_OK : NX_CRYPTO_ENTROPY_FAILED;
}

static const EVP_CIPHER* cipher_for(nx_crypto_algorithm_t algorithm) {
    return algorithm == NX_CRYPTO_AES128_GCM ? EVP_aes_128_gcm() : EVP_aes_256_gcm();
}

static nx_crypto_status_t seal(void* ctx, nx_crypto_algorithm_t algorithm,
    const uint8_t* key, const uint8_t* nonce, const uint8_t* aad, size_t aad_size,
    const uint8_t* input, size_t input_size, uint8_t* output, uint8_t* tag) {
    (void)ctx;
    if (input_size > INT_MAX || aad_size > INT_MAX) { return NX_CRYPTO_INVALID_PARAM; }
    EVP_CIPHER_CTX* cipher = EVP_CIPHER_CTX_new();
    if (!cipher) { return NX_CRYPTO_NO_MEMORY; }
    int bytes = 0, tail = 0;
    int ok = EVP_EncryptInit_ex(cipher, cipher_for(algorithm), NULL, NULL, NULL) == 1 &&
        EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_IVLEN, NX_CRYPTO_NONCE_SIZE, NULL) == 1 &&
        EVP_EncryptInit_ex(cipher, NULL, NULL, key, nonce) == 1;
    if (ok && aad_size) { ok = EVP_EncryptUpdate(cipher, NULL, &bytes, aad, (int)aad_size) == 1; }
    bytes = 0;
    if (ok && input_size) { ok = EVP_EncryptUpdate(cipher, output, &bytes, input, (int)input_size) == 1; }
    if (ok) { ok = EVP_EncryptFinal_ex(cipher, output + bytes, &tail) == 1 &&
        EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_GET_TAG, NX_CRYPTO_TAG_SIZE, tag) == 1 &&
        (size_t)(bytes + tail) == input_size; }
    EVP_CIPHER_CTX_free(cipher);
    return ok ? NX_CRYPTO_OK : NX_CRYPTO_FAILED;
}

static nx_crypto_status_t open_record(void* ctx, nx_crypto_algorithm_t algorithm,
    const uint8_t* key, const uint8_t* nonce, const uint8_t* aad, size_t aad_size,
    const uint8_t* input, size_t input_size, const uint8_t* tag, uint8_t* output) {
    (void)ctx;
    if (input_size > INT_MAX || aad_size > INT_MAX) { return NX_CRYPTO_INVALID_PARAM; }
    EVP_CIPHER_CTX* cipher = EVP_CIPHER_CTX_new();
    if (!cipher) { return NX_CRYPTO_NO_MEMORY; }
    int bytes = 0, tail = 0;
    int ok = EVP_DecryptInit_ex(cipher, cipher_for(algorithm), NULL, NULL, NULL) == 1 &&
        EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_IVLEN, NX_CRYPTO_NONCE_SIZE, NULL) == 1 &&
        EVP_DecryptInit_ex(cipher, NULL, NULL, key, nonce) == 1;
    if (ok && aad_size) { ok = EVP_DecryptUpdate(cipher, NULL, &bytes, aad, (int)aad_size) == 1; }
    bytes = 0;
    if (ok && input_size) { ok = EVP_DecryptUpdate(cipher, output, &bytes, input, (int)input_size) == 1; }
    if (ok) { ok = EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_TAG, NX_CRYPTO_TAG_SIZE, (void*)tag) == 1; }
    int authenticated = ok ? EVP_DecryptFinal_ex(cipher, output + bytes, &tail) : -1;
    EVP_CIPHER_CTX_free(cipher);
    if (!ok) { return NX_CRYPTO_FAILED; }
    return authenticated == 1 && (size_t)(bytes + tail) == input_size ? NX_CRYPTO_OK : NX_CRYPTO_AUTH_FAILED;
}

static nx_crypto_status_t sha256(void* ctx, const uint8_t* input, size_t size, uint8_t* output) {
    (void)ctx;
    unsigned int written = 0;
    return EVP_Digest(input, size, output, &written, EVP_sha256(), NULL) == 1 &&
           written == NX_CRYPTO_SHA256_SIZE ? NX_CRYPTO_OK : NX_CRYPTO_FAILED;
}

static nx_crypto_status_t verify_ed25519(void* ctx, const uint8_t* key,
    const uint8_t* message, size_t size, const uint8_t* signature) {
    (void)ctx;
    EVP_PKEY* public_key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL,
        key, NX_CRYPTO_ED25519_PUBLIC_KEY_SIZE);
    if (!public_key) { return NX_CRYPTO_FAILED; }
    EVP_MD_CTX* digest = EVP_MD_CTX_new();
    if (!digest) { EVP_PKEY_free(public_key); return NX_CRYPTO_NO_MEMORY; }
    int initialized = EVP_DigestVerifyInit(digest, NULL, NULL, NULL, public_key);
    int verified = initialized == 1 ? EVP_DigestVerify(digest, signature,
        NX_CRYPTO_ED25519_SIGNATURE_SIZE, message, size) : -1;
    EVP_MD_CTX_free(digest);
    EVP_PKEY_free(public_key);
    return verified == 1 ? NX_CRYPTO_OK : verified == 0 ? NX_CRYPTO_AUTH_FAILED : NX_CRYPTO_FAILED;
}

/* Constant initialization permits concurrent first use without a lazy global
 * registration race. Explicit replacement still requires quiescent callers. */
const nx_crypto_provider_t nx_crypto_openssl_default_provider = {
    NULL, random_bytes, seal, open_record, sha256, verify_ed25519
};
const nx_crypto_provider_t* nx_crypto_openssl_provider(void) {
    return &nx_crypto_openssl_default_provider;
}
