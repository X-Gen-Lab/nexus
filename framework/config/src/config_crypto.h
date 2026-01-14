/**
 * \file            config_crypto.h
 * \brief           Config Manager Crypto Internal Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Internal header for config encryption functionality.
 *                  This header is not part of the public API.
 */

#ifndef CONFIG_CRYPTO_H
#define CONFIG_CRYPTO_H

#include "config/config_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           AES block size in bytes
 */
#define CONFIG_CRYPTO_AES_BLOCK_SIZE 16

/**
 * \brief           AES-128 key size in bytes
 */
#define CONFIG_CRYPTO_AES128_KEY_SIZE 16

/**
 * \brief           AES-256 key size in bytes
 */
#define CONFIG_CRYPTO_AES256_KEY_SIZE 32

/**
 * \brief           Maximum encryption key size
 */
#define CONFIG_CRYPTO_MAX_KEY_SIZE CONFIG_CRYPTO_AES256_KEY_SIZE

/**
 * \brief           IV size for CBC mode
 */
#define CONFIG_CRYPTO_IV_SIZE 16

/**
 * \brief           Check if encryption is enabled
 * \return          true if encryption key is set, false otherwise
 */
bool config_crypto_is_enabled(void);

/**
 * \brief           Get the current encryption algorithm
 * \return          Current encryption algorithm
 */
config_crypto_algo_t config_crypto_get_algo(void);

/**
 * \brief           Encrypt data using the configured key
 * \param[in]       plaintext: Input data to encrypt
 * \param[in]       plaintext_len: Length of input data
 * \param[out]      ciphertext: Output buffer for encrypted data
 * \param[in,out]   ciphertext_len: Input: buffer size, Output: actual size
 * \return          CONFIG_OK on success, error code otherwise
 * \note            Output includes IV prepended to ciphertext
 */
config_status_t config_crypto_encrypt(const uint8_t* plaintext,
                                      size_t plaintext_len, uint8_t* ciphertext,
                                      size_t* ciphertext_len);

/**
 * \brief           Decrypt data using the configured key
 * \param[in]       ciphertext: Input encrypted data (IV + ciphertext)
 * \param[in]       ciphertext_len: Length of encrypted data
 * \param[out]      plaintext: Output buffer for decrypted data
 * \param[in,out]   plaintext_len: Input: buffer size, Output: actual size
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_crypto_decrypt(const uint8_t* ciphertext,
                                      size_t ciphertext_len, uint8_t* plaintext,
                                      size_t* plaintext_len);

/**
 * \brief           Calculate encrypted size for given plaintext size
 * \param[in]       plaintext_len: Length of plaintext
 * \return          Required buffer size for ciphertext (includes IV)
 */
size_t config_crypto_get_encrypted_size(size_t plaintext_len);

/**
 * \brief           Calculate decrypted size for given ciphertext size
 * \param[in]       ciphertext_len: Length of ciphertext (includes IV)
 * \return          Maximum possible plaintext size
 */
size_t config_crypto_get_decrypted_size(size_t ciphertext_len);

/**
 * \brief           Clear crypto state (called during deinit)
 */
void config_crypto_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_CRYPTO_H */
