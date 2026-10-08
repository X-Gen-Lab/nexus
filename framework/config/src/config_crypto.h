/** Internal authenticated configuration record interface. */
#ifndef CONFIG_CRYPTO_H
#define CONFIG_CRYPTO_H
#include "config/config_def.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CONFIG_CRYPTO_AES128_KEY_SIZE 16u
#define CONFIG_CRYPTO_AES256_KEY_SIZE 32u
#define CONFIG_CRYPTO_MAX_KEY_SIZE CONFIG_CRYPTO_AES256_KEY_SIZE
#define CONFIG_CRYPTO_KEY_ID_SIZE 16u
#define CONFIG_CRYPTO_HEADER_SIZE 28u
#define CONFIG_CRYPTO_NONCE_SIZE 12u
#define CONFIG_CRYPTO_TAG_SIZE 16u
#define CONFIG_CRYPTO_RECORD_OVERHEAD 56u

bool config_crypto_is_enabled(void);
config_crypto_algo_t config_crypto_get_algo(void);
/* Anonymous record functions for internal direct tests; config entries use
 * record functions below to authenticate their key/namespace/type identity. */
config_status_t config_crypto_encrypt(const uint8_t* plaintext, size_t length,
                                      uint8_t* record, size_t* record_length);
config_status_t config_crypto_decrypt(const uint8_t* record, size_t length,
                                      uint8_t* plaintext, size_t* plaintext_length);
config_status_t config_crypto_decrypt_record(const uint8_t* record, size_t length,
    uint8_t* plaintext, size_t* plaintext_length, const char* key,
    uint8_t namespace_id, config_type_t type);
/** Read and authenticate a stored encrypted value before returning its size. */
config_status_t config_crypto_get_plaintext_size(const char* key,
    uint8_t namespace_id, config_type_t type, size_t* size);
size_t config_crypto_get_encrypted_size(size_t length);
size_t config_crypto_get_decrypted_size(size_t length);
void config_crypto_clear(void);
#ifdef __cplusplus
}
#endif
#endif
