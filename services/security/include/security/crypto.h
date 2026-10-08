/** Maintained cryptography provider boundary. No built-in cryptographic primitives. */
#ifndef NX_SECURITY_CRYPTO_H
#define NX_SECURITY_CRYPTO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define NX_CRYPTO_NONCE_SIZE 12u
#define NX_CRYPTO_TAG_SIZE 16u
#define NX_CRYPTO_SHA256_SIZE 32u
#define NX_CRYPTO_ED25519_PUBLIC_KEY_SIZE 32u
#define NX_CRYPTO_ED25519_SIGNATURE_SIZE 64u
#ifndef NX_CRYPTO_MAX_AEAD_SIZE
#define NX_CRYPTO_MAX_AEAD_SIZE 4096u
#endif

typedef enum {
    NX_CRYPTO_OK = 0,
    NX_CRYPTO_INVALID_PARAM,
    NX_CRYPTO_UNSUPPORTED,
    NX_CRYPTO_ENTROPY_FAILED,
    NX_CRYPTO_AUTH_FAILED,
    NX_CRYPTO_NO_MEMORY,
    NX_CRYPTO_FAILED
} nx_crypto_status_t;

typedef enum {
    NX_CRYPTO_AES128_GCM = 1,
    NX_CRYPTO_AES256_GCM = 2
} nx_crypto_algorithm_t;

/**
 * Providers must use a maintained crypto library / hardware implementation and
 * propagate entropy failure. Never seed from UID, uptime or key bytes. Provider
 * callbacks and context remain valid until explicitly replaced while quiescent.
 * Each callback is an independent optional capability; missing operations return
 * UNSUPPORTED. A provider must implement at least one callback. AEAD configuration
 * requires random/seal/open/SHA256, while Ed25519 is only needed for that verifier.
 * Registration is a startup/maintenance operation and is not thread safe.
 * Calls are task-only, may allocate and block; callers enforce their time budget.
 * Nonce uniqueness and key rotation are record-layer responsibilities.
 */
typedef struct {
    void* context;
    nx_crypto_status_t (*random)(void*, uint8_t*, size_t);
    nx_crypto_status_t (*seal)(void*, nx_crypto_algorithm_t, const uint8_t*,
                              const uint8_t*, const uint8_t*, size_t,
                              const uint8_t*, size_t, uint8_t*, uint8_t*);
    nx_crypto_status_t (*open)(void*, nx_crypto_algorithm_t, const uint8_t*,
                              const uint8_t*, const uint8_t*, size_t,
                              const uint8_t*, size_t, const uint8_t*, uint8_t*);
    nx_crypto_status_t (*sha256)(void*, const uint8_t*, size_t, uint8_t*);
    nx_crypto_status_t (*verify_ed25519)(void*, const uint8_t*, const uint8_t*,
                                        size_t, const uint8_t*);
} nx_crypto_provider_t;

/** NULL disables cryptography, including a compiled-in default provider. */
nx_crypto_status_t nx_crypto_set_provider(const nx_crypto_provider_t* provider);
/** Native defaults to OpenSSL; unconfigured MCU ports return UNSUPPORTED. */
nx_crypto_status_t nx_crypto_use_default_provider(void);
bool nx_crypto_is_available(void);
/** Borrowed provider for platform composition; do not mutate its callbacks. */
const nx_crypto_provider_t* nx_crypto_get_provider(void);
void nx_crypto_secure_zero(void* data, size_t size);
nx_crypto_status_t nx_crypto_random(uint8_t* output, size_t size);
nx_crypto_status_t nx_crypto_seal(nx_crypto_algorithm_t algorithm,
    const uint8_t* key, size_t key_size, const uint8_t nonce[NX_CRYPTO_NONCE_SIZE],
    const uint8_t* aad, size_t aad_size, const uint8_t* input, size_t input_size,
    uint8_t* output, uint8_t tag[NX_CRYPTO_TAG_SIZE]);
/** Caller output is unchanged unless authentication succeeds. Scratch RAM is
 * bounded by NX_CRYPTO_MAX_AEAD_SIZE and securely erased on every exit. */
nx_crypto_status_t nx_crypto_open(nx_crypto_algorithm_t algorithm,
    const uint8_t* key, size_t key_size, const uint8_t nonce[NX_CRYPTO_NONCE_SIZE],
    const uint8_t* aad, size_t aad_size, const uint8_t* input, size_t input_size,
    const uint8_t tag[NX_CRYPTO_TAG_SIZE], uint8_t* output);
nx_crypto_status_t nx_crypto_sha256(const uint8_t* input, size_t input_size,
                                   uint8_t output[NX_CRYPTO_SHA256_SIZE]);
nx_crypto_status_t nx_crypto_verify_ed25519(
    const uint8_t key[NX_CRYPTO_ED25519_PUBLIC_KEY_SIZE], const uint8_t* message,
    size_t message_size, const uint8_t signature[NX_CRYPTO_ED25519_SIGNATURE_SIZE]);
#ifdef __cplusplus
}
#endif
#endif
