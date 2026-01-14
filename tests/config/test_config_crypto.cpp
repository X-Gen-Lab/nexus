/**
 * \file            test_config_crypto.cpp
 * \brief           Config Manager Encryption Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Config Manager encryption functionality.
 *                  Requirements: 12.1-12.10
 */

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Config Crypto Test Fixture
 */
class ConfigCryptoTest : public ::testing::Test {
  protected:
    void SetUp() override {
        if (config_is_initialized()) {
            config_deinit();
        }
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
    }

    void TearDown() override {
        if (config_is_initialized()) {
            config_deinit();
        }
    }

    /* AES-128 test key (16 bytes) */
    const uint8_t aes128_key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                    0x0c, 0x0d, 0x0e, 0x0f};

    /* AES-256 test key (32 bytes) */
    const uint8_t aes256_key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
};

/*---------------------------------------------------------------------------*/
/* Encryption Key Management Tests - Requirements 12.3, 12.4, 12.5           */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCryptoTest, SetEncryptionKeyAES128) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
}

TEST_F(ConfigCryptoTest, SetEncryptionKeyAES256) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256));
}

TEST_F(ConfigCryptoTest, SetEncryptionKeyNullKey) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_encryption_key(NULL, 16, CONFIG_CRYPTO_AES128));
}

TEST_F(ConfigCryptoTest, SetEncryptionKeyInvalidLength) {
    /* Wrong key length for AES-128 */
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_encryption_key(aes128_key, 15, CONFIG_CRYPTO_AES128));

    /* Wrong key length for AES-256 */
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_encryption_key(aes256_key, 31, CONFIG_CRYPTO_AES256));
}

TEST_F(ConfigCryptoTest, ClearEncryptionKey) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
    EXPECT_EQ(CONFIG_OK, config_clear_encryption_key());
}

TEST_F(ConfigCryptoTest, ClearEncryptionKeyNotInitialized) {
    config_deinit();
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_clear_encryption_key());
}

/*---------------------------------------------------------------------------*/
/* Encrypted String Storage Tests - Requirements 12.1, 12.2                  */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCryptoTest, SetStrEncryptedWithoutKey) {
    /* No encryption key set */
    EXPECT_EQ(CONFIG_ERROR_NO_ENCRYPTION_KEY,
              config_set_str_encrypted("test.key", "secret value"));
}

TEST_F(ConfigCryptoTest, SetStrEncryptedAndGet) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));

    const char* secret = "This is a secret password!";
    EXPECT_EQ(CONFIG_OK, config_set_str_encrypted("secret.password", secret));

    /* Read back the value - should be decrypted automatically */
    char buffer[128];
    EXPECT_EQ(CONFIG_OK,
              config_get_str("secret.password", buffer, sizeof(buffer)));
    EXPECT_STREQ(secret, buffer);
}

TEST_F(ConfigCryptoTest, SetStrEncryptedAES256) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256));

    const char* secret = "AES-256 encrypted secret";
    EXPECT_EQ(CONFIG_OK, config_set_str_encrypted("aes256.secret", secret));

    char buffer[128];
    EXPECT_EQ(CONFIG_OK,
              config_get_str("aes256.secret", buffer, sizeof(buffer)));
    EXPECT_STREQ(secret, buffer);
}

TEST_F(ConfigCryptoTest, SetStrEncryptedNullKey) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_str_encrypted(NULL, "value"));
}

TEST_F(ConfigCryptoTest, SetStrEncryptedNullValue) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_str_encrypted("key", NULL));
}

/*---------------------------------------------------------------------------*/
/* Encrypted Blob Storage Tests - Requirements 12.1, 12.2                    */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCryptoTest, SetBlobEncryptedWithoutKey) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(CONFIG_ERROR_NO_ENCRYPTION_KEY,
              config_set_blob_encrypted("test.blob", data, sizeof(data)));
}

TEST_F(ConfigCryptoTest, SetBlobEncryptedAndGet) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));

    uint8_t secret_data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    EXPECT_EQ(CONFIG_OK, config_set_blob_encrypted("secret.blob", secret_data,
                                                   sizeof(secret_data)));

    /* Read back the value - should be decrypted automatically */
    uint8_t buffer[64];
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK, config_get_blob("secret.blob", buffer, sizeof(buffer),
                                         &actual_size));
    EXPECT_EQ(sizeof(secret_data), actual_size);
    EXPECT_EQ(0, memcmp(secret_data, buffer, sizeof(secret_data)));
}

TEST_F(ConfigCryptoTest, SetBlobEncryptedAES256) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256));

    uint8_t secret_data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    EXPECT_EQ(CONFIG_OK, config_set_blob_encrypted("aes256.blob", secret_data,
                                                   sizeof(secret_data)));

    uint8_t buffer[64];
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK, config_get_blob("aes256.blob", buffer, sizeof(buffer),
                                         &actual_size));
    EXPECT_EQ(sizeof(secret_data), actual_size);
    EXPECT_EQ(0, memcmp(secret_data, buffer, sizeof(secret_data)));
}

TEST_F(ConfigCryptoTest, SetBlobEncryptedNullKey) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
    uint8_t data[] = {0x01};
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_blob_encrypted(NULL, data, sizeof(data)));
}

TEST_F(ConfigCryptoTest, SetBlobEncryptedNullData) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_blob_encrypted("key", NULL, 10));
}

TEST_F(ConfigCryptoTest, SetBlobEncryptedZeroSize) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
    uint8_t data[] = {0x01};
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_blob_encrypted("key", data, 0));
}

/*---------------------------------------------------------------------------*/
/* Encryption Status Tests - Requirements 12.6                               */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCryptoTest, IsEncryptedTrue) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
    EXPECT_EQ(CONFIG_OK, config_set_str_encrypted("encrypted.key", "secret"));

    bool encrypted = false;
    EXPECT_EQ(CONFIG_OK, config_is_encrypted("encrypted.key", &encrypted));
    EXPECT_TRUE(encrypted);
}

TEST_F(ConfigCryptoTest, IsEncryptedFalse) {
    EXPECT_EQ(CONFIG_OK, config_set_str("plain.key", "not secret"));

    bool encrypted = true;
    EXPECT_EQ(CONFIG_OK, config_is_encrypted("plain.key", &encrypted));
    EXPECT_FALSE(encrypted);
}

TEST_F(ConfigCryptoTest, IsEncryptedNotFound) {
    bool encrypted = false;
    EXPECT_EQ(CONFIG_ERROR_NOT_FOUND,
              config_is_encrypted("nonexistent.key", &encrypted));
}

TEST_F(ConfigCryptoTest, IsEncryptedNullKey) {
    bool encrypted = false;
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_is_encrypted(NULL, &encrypted));
}

TEST_F(ConfigCryptoTest, IsEncryptedNullResult) {
    EXPECT_EQ(CONFIG_OK, config_set_str("test.key", "value"));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_is_encrypted("test.key", NULL));
}

/*---------------------------------------------------------------------------*/
/* Key Rotation Tests - Requirements 12.7, 12.8                              */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCryptoTest, RotateKeyWithoutExistingKey) {
    EXPECT_EQ(
        CONFIG_ERROR_NO_ENCRYPTION_KEY,
        config_rotate_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256));
}

TEST_F(ConfigCryptoTest, RotateKeyAES128ToAES256) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));

    /* Store encrypted value with AES-128 */
    EXPECT_EQ(CONFIG_OK, config_set_str_encrypted("rotate.test", "secret"));

    /* Rotate to AES-256 */
    EXPECT_EQ(CONFIG_OK, config_rotate_encryption_key(aes256_key, 32,
                                                      CONFIG_CRYPTO_AES256));

    /* Note: Current implementation just changes the key, doesn't re-encrypt
       existing values. New values will use the new key. */
}

TEST_F(ConfigCryptoTest, RotateKeyNullKey) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_rotate_encryption_key(NULL, 16, CONFIG_CRYPTO_AES128));
}

TEST_F(ConfigCryptoTest, RotateKeyInvalidLength) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
    EXPECT_EQ(
        CONFIG_ERROR_INVALID_PARAM,
        config_rotate_encryption_key(aes128_key, 15, CONFIG_CRYPTO_AES128));
}

/*---------------------------------------------------------------------------*/
/* Export with Decrypt Flag Tests - Requirements 12.9                        */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCryptoTest, ExportWithDecryptFlag) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));

    const char* secret = "my secret value";
    EXPECT_EQ(CONFIG_OK, config_set_str_encrypted("export.secret", secret));

    /* Export with decrypt flag */
    size_t size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_DECRYPT, &size));

    std::vector<char> buffer(size + 1);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_DECRYPT,
                            buffer.data(), buffer.size(), &actual_size));

    /* The exported JSON should contain the decrypted value */
    EXPECT_NE(nullptr, strstr(buffer.data(), "export.secret"));
    EXPECT_NE(nullptr, strstr(buffer.data(), secret));
}

TEST_F(ConfigCryptoTest, ExportWithoutDecryptFlag) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));

    const char* secret = "my secret value";
    EXPECT_EQ(CONFIG_OK, config_set_str_encrypted("export.secret2", secret));

    /* Export without decrypt flag */
    size_t size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_get_export_size(CONFIG_FORMAT_JSON,
                                     CONFIG_EXPORT_FLAG_NONE, &size));

    std::vector<char> buffer(size + 1);
    size_t actual_size = 0;
    EXPECT_EQ(CONFIG_OK,
              config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
                            buffer.data(), buffer.size(), &actual_size));

    /* The exported JSON should contain the key but encrypted value (base64) */
    EXPECT_NE(nullptr, strstr(buffer.data(), "export.secret2"));
    /* The plaintext should NOT appear in the export */
    EXPECT_EQ(nullptr, strstr(buffer.data(), secret));
}

/*---------------------------------------------------------------------------*/
/* Not Initialized Tests                                                     */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCryptoTest, SetEncryptionKeyNotInitialized) {
    config_deinit();
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128));
}

TEST_F(ConfigCryptoTest, SetStrEncryptedNotInitialized) {
    config_deinit();
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_set_str_encrypted("key", "value"));
}

TEST_F(ConfigCryptoTest, SetBlobEncryptedNotInitialized) {
    config_deinit();
    uint8_t data[] = {0x01};
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT,
              config_set_blob_encrypted("key", data, sizeof(data)));
}

TEST_F(ConfigCryptoTest, IsEncryptedNotInitialized) {
    config_deinit();
    bool encrypted = false;
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_is_encrypted("key", &encrypted));
}

TEST_F(ConfigCryptoTest, RotateKeyNotInitialized) {
    config_deinit();
    EXPECT_EQ(CONFIG_ERROR_NOT_INIT, config_rotate_encryption_key(
                                         aes128_key, 16, CONFIG_CRYPTO_AES128));
}
