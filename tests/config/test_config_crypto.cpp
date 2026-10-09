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
#include <string>
#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "config/config.h"
#include "../../framework/config/src/config_crypto.h"
#include "../../framework/config/src/config_namespace.h"
#include "../../framework/config/src/config_store.h"
#include "security/crypto.h"
#include "security/crypto_openssl.h"
}

/**
 * \brief           Config Crypto Test Fixture
 */
class ConfigCryptoTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ASSERT_EQ(NX_CRYPTO_OK, nx_crypto_set_provider(nx_crypto_openssl_provider()));
        if (config_is_initialized()) {
            config_deinit();
        }
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
    }

    void TearDown() override {
        EXPECT_EQ(NX_CRYPTO_OK, nx_crypto_set_provider(nx_crypto_openssl_provider()));
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
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
}

TEST_F(ConfigCryptoTest, SetEncryptionKeyAES256) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
}

TEST_F(ConfigCryptoTest, SetEncryptionKeyNullKey) {
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_encryption_key(NULL, 16, CONFIG_CRYPTO_AES128_GCM));
}

TEST_F(ConfigCryptoTest, SetEncryptionKeyInvalidLength) {
    /* Wrong key length for AES-128 */
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_encryption_key(aes128_key, 15, CONFIG_CRYPTO_AES128_GCM));

    /* Wrong key length for AES-256 */
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_encryption_key(aes256_key, 31, CONFIG_CRYPTO_AES256_GCM));
}

TEST_F(ConfigCryptoTest, ClearEncryptionKey) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
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
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));

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
              config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));

    const char* secret = "AES-256 encrypted secret";
    EXPECT_EQ(CONFIG_OK, config_set_str_encrypted("aes256.secret", secret));

    char buffer[128];
    EXPECT_EQ(CONFIG_OK,
              config_get_str("aes256.secret", buffer, sizeof(buffer)));
    EXPECT_STREQ(secret, buffer);
}

TEST_F(ConfigCryptoTest, SetStrEncryptedNullKey) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_str_encrypted(NULL, "value"));
}

TEST_F(ConfigCryptoTest, SetStrEncryptedNullValue) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
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
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));

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
              config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));

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
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
    uint8_t data[] = {0x01};
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_blob_encrypted(NULL, data, sizeof(data)));
}

TEST_F(ConfigCryptoTest, SetBlobEncryptedNullData) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_blob_encrypted("key", NULL, 10));
}

TEST_F(ConfigCryptoTest, SetBlobEncryptedZeroSize) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
    uint8_t data[] = {0x01};
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_set_blob_encrypted("key", data, 0));
}

/*---------------------------------------------------------------------------*/
/* Encryption Status Tests - Requirements 12.6                               */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCryptoTest, IsEncryptedTrue) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
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
        config_rotate_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
}

TEST_F(ConfigCryptoTest, RotateKeyAES128ToAES256) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));

    /* Store encrypted value with AES-128 */
    EXPECT_EQ(CONFIG_OK, config_set_str_encrypted("rotate.test", "secret"));

    /* Rotate to AES-256 */
    EXPECT_EQ(CONFIG_OK, config_rotate_encryption_key(aes256_key, 32,
                                                      CONFIG_CRYPTO_AES256_GCM));

    char output[32];
    ASSERT_EQ(CONFIG_OK, config_get_str("rotate.test", output, sizeof(output)));
    EXPECT_STREQ("secret", output);
}

TEST_F(ConfigCryptoTest, RotateKeyNullKey) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
    EXPECT_EQ(CONFIG_ERROR_INVALID_PARAM,
              config_rotate_encryption_key(NULL, 16, CONFIG_CRYPTO_AES128_GCM));
}

TEST_F(ConfigCryptoTest, RotateKeyInvalidLength) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
    EXPECT_EQ(
        CONFIG_ERROR_INVALID_PARAM,
        config_rotate_encryption_key(aes128_key, 15, CONFIG_CRYPTO_AES128_GCM));
}

/*---------------------------------------------------------------------------*/
/* Export with Decrypt Flag Tests - Requirements 12.9                        */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigCryptoTest, ExportWithDecryptFlag) {
    EXPECT_EQ(CONFIG_OK,
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));

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
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));

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
              config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
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
                                         aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
}

static nx_crypto_status_t failEntropy(void*, uint8_t*, size_t) {
    return NX_CRYPTO_ENTROPY_FAILED;
}
static nx_crypto_status_t repeatNonce(void*, uint8_t* output, size_t size) {
    memset(output, 0x55, size);
    return NX_CRYPTO_OK;
}

TEST_F(ConfigCryptoTest, MissingProviderFailsClosed) {
    ASSERT_EQ(NX_CRYPTO_UNSUPPORTED, nx_crypto_set_provider(nullptr));
    EXPECT_EQ(CONFIG_ERROR_UNSUPPORTED,
        config_set_encryption_key(aes256_key, sizeof(aes256_key), CONFIG_CRYPTO_AES256_GCM));
    EXPECT_EQ(CONFIG_ERROR_NO_ENCRYPTION_KEY, config_set_str_encrypted("secret", "never stored"));
    bool exists = true;
    EXPECT_EQ(CONFIG_OK, config_exists("secret", &exists));
    EXPECT_FALSE(exists);
}

TEST_F(ConfigCryptoTest, EntropyFailureLeavesExistingValueUnchanged) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("secret", "original"));
    nx_crypto_provider_t provider = *nx_crypto_get_provider();
    provider.random = failEntropy;
    ASSERT_EQ(NX_CRYPTO_OK, nx_crypto_set_provider(&provider));
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_set_str_encrypted("secret", "replacement"));
    char buffer[32];
    ASSERT_EQ(CONFIG_OK, config_get_str("secret", buffer, sizeof(buffer)));
    EXPECT_STREQ("original", buffer);
    ASSERT_EQ(NX_CRYPTO_OK, nx_crypto_set_provider(nx_crypto_openssl_provider()));
}

TEST_F(ConfigCryptoTest, RepeatedProviderNonceRejected) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    nx_crypto_provider_t provider = *nx_crypto_get_provider();
    provider.random = repeatNonce;
    ASSERT_EQ(NX_CRYPTO_OK, nx_crypto_set_provider(&provider));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("first", "original"));
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_set_str_encrypted("second", "must fail"));
    ASSERT_EQ(NX_CRYPTO_OK, nx_crypto_set_provider(nx_crypto_openssl_provider()));
}

TEST_F(ConfigCryptoTest, EveryRecordByteIsAuthenticatedOrRejectedWithoutPlaintext) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("critical.setting", "original"));
    uint8_t record[256];
    size_t size = sizeof(record);
    ASSERT_EQ(CONFIG_OK, config_store_get("critical.setting", nullptr, record, &size, nullptr, 0));
    for (size_t byte = 0; byte < size; ++byte) {
        record[byte] ^= 1;
        ASSERT_EQ(CONFIG_OK, config_store_set("critical.setting", CONFIG_TYPE_STRING, record,
            size, CONFIG_FLAG_ENCRYPTED, 0));
        char output[64];
        memset(output, 0x55, sizeof(output));
        EXPECT_NE(CONFIG_OK, config_get_str("critical.setting", output, sizeof(output))) << byte;
        for (char c : output) { EXPECT_EQ(0x55, c) << byte; }
        record[byte] ^= 1;
    }
}

TEST_F(ConfigCryptoTest, MovingRecordToAnotherKeyOrNamespaceFailsAuthentication) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("original", "secret"));
    uint8_t record[256];
    size_t size = sizeof(record);
    ASSERT_EQ(CONFIG_OK, config_store_get("original", nullptr, record, &size, nullptr, 0));
    ASSERT_EQ(CONFIG_OK, config_store_set("renamed", CONFIG_TYPE_STRING, record,
        size, CONFIG_FLAG_ENCRYPTED, 0));
    char output[32];
    memset(output, 0x55, sizeof(output));
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_get_str("renamed", output, sizeof(output)));
    size_t plainSize = sizeof(output);
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_crypto_decrypt_record(record, size,
        reinterpret_cast<uint8_t*>(output), &plainSize, "original", 1, CONFIG_TYPE_STRING));
    plainSize = sizeof(output);
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_crypto_decrypt_record(record, size,
        reinterpret_cast<uint8_t*>(output), &plainSize, "original", 0, CONFIG_TYPE_BLOB));
}

TEST_F(ConfigCryptoTest, RotationPreservesAllValuesAndMetadata) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("first", "one"));
    const uint8_t blob[] = {0, 1, 255, 0, 42};
    ASSERT_EQ(CONFIG_OK, config_set_blob_encrypted("second", blob, sizeof(blob)));
    ASSERT_EQ(CONFIG_OK, config_set_u32("plain", 1234));
    ASSERT_EQ(CONFIG_OK, config_rotate_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    char output[32];
    ASSERT_EQ(CONFIG_OK, config_get_str("first", output, sizeof(output)));
    EXPECT_STREQ("one", output);
    uint8_t blobOutput[sizeof(blob)];
    size_t actual = 0;
    ASSERT_EQ(CONFIG_OK, config_get_blob("second", blobOutput, sizeof(blobOutput), &actual));
    EXPECT_EQ(sizeof(blob), actual);
    EXPECT_EQ(0, memcmp(blob, blobOutput, sizeof(blob)));
    uint32_t plain;
    ASSERT_EQ(CONFIG_OK, config_get_u32("plain", &plain, 0));
    EXPECT_EQ(1234u, plain);
    uint8_t oldId[16];
    ASSERT_EQ(CONFIG_OK, config_get_encryption_key_id(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM, oldId));
    ASSERT_EQ(CONFIG_OK, config_retire_encryption_key(oldId));
    ASSERT_EQ(CONFIG_OK, config_get_str("first", output, sizeof(output)));
    EXPECT_STREQ("one", output);
}

TEST_F(ConfigCryptoTest, CorruptEntryAbortsEntireRotationAndPreservesOldKey) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("first", "one"));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("second", "two"));
    uint8_t original[256], damaged[256];
    size_t originalSize = sizeof(original), damagedSize = sizeof(damaged);
    ASSERT_EQ(CONFIG_OK, config_store_get("first", nullptr, original, &originalSize, nullptr, 0));
    ASSERT_EQ(CONFIG_OK, config_store_get("second", nullptr, damaged, &damagedSize, nullptr, 0));
    damaged[damagedSize - 1] ^= 1;
    ASSERT_EQ(CONFIG_OK, config_store_set("second", CONFIG_TYPE_STRING, damaged,
        damagedSize, CONFIG_FLAG_ENCRYPTED, 0));
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED,
        config_rotate_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    EXPECT_EQ(CONFIG_CRYPTO_AES128_GCM, config_crypto_get_algo());
    uint8_t after[256];
    size_t afterSize = sizeof(after);
    ASSERT_EQ(CONFIG_OK, config_store_get("first", nullptr, after, &afterSize, nullptr, 0));
    EXPECT_EQ(originalSize, afterSize);
    EXPECT_EQ(0, memcmp(original, after, originalSize));
    char buffer[16];
    ASSERT_EQ(CONFIG_OK, config_get_str("first", buffer, sizeof(buffer)));
    EXPECT_STREQ("one", buffer);
}

TEST_F(ConfigCryptoTest, EntropyFailureDuringRotationLeavesEveryRecordUnchanged) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("first", "one"));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("second", "two"));
    nx_crypto_provider_t provider = *nx_crypto_get_provider();
    provider.random = failEntropy;
    ASSERT_EQ(NX_CRYPTO_OK, nx_crypto_set_provider(&provider));
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED,
        config_rotate_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    EXPECT_EQ(CONFIG_CRYPTO_AES128_GCM, config_crypto_get_algo());
    char buffer[16];
    ASSERT_EQ(CONFIG_OK, config_get_str("first", buffer, sizeof(buffer)));
    EXPECT_STREQ("one", buffer);
    ASSERT_EQ(CONFIG_OK, config_get_str("second", buffer, sizeof(buffer)));
    EXPECT_STREQ("two", buffer);
    ASSERT_EQ(NX_CRYPTO_OK, nx_crypto_set_provider(nx_crypto_openssl_provider()));
}

TEST_F(ConfigCryptoTest, SerializedRecordCanBeRestoredWithRegisteredGenerationsAfterRestart) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("before", "old"));
    uint8_t oldRecord[256];
    size_t oldSize = sizeof(oldRecord);
    ASSERT_EQ(CONFIG_OK, config_store_get("before", nullptr, oldRecord, &oldSize, nullptr, 0));
    ASSERT_EQ(CONFIG_OK, config_rotate_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("after", "new"));
    uint8_t newRecord[256];
    size_t newSize = sizeof(newRecord);
    ASSERT_EQ(CONFIG_OK, config_store_get("after", nullptr, newRecord, &newSize, nullptr, 0));
    ASSERT_EQ(CONFIG_OK, config_deinit());
    ASSERT_EQ(CONFIG_OK, config_init(nullptr));
    ASSERT_EQ(CONFIG_OK, config_register_encryption_key(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM, false));
    ASSERT_EQ(CONFIG_OK, config_register_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM, true));
    ASSERT_EQ(CONFIG_OK, config_store_set("before", CONFIG_TYPE_STRING, oldRecord,
        oldSize, CONFIG_FLAG_ENCRYPTED, 0));
    ASSERT_EQ(CONFIG_OK, config_store_set("after", CONFIG_TYPE_STRING, newRecord,
        newSize, CONFIG_FLAG_ENCRYPTED, 0));
    char output[16];
    ASSERT_EQ(CONFIG_OK, config_get_str("before", output, sizeof(output)));
    EXPECT_STREQ("old", output);
    ASSERT_EQ(CONFIG_OK, config_get_str("after", output, sizeof(output)));
    EXPECT_STREQ("new", output);
    uint8_t oldId[16];
    ASSERT_EQ(CONFIG_OK, config_get_encryption_key_id(aes128_key, 16, CONFIG_CRYPTO_AES128_GCM, oldId));
    EXPECT_EQ(CONFIG_ERROR_ALREADY_EXISTS, config_retire_encryption_key(oldId));
}

TEST_F(ConfigCryptoTest, ReinitializingSameKeyDoesNotReplayNonces) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    uint8_t first[128], second[128];
    const uint8_t data[] = "same";
    size_t firstSize = sizeof(first), secondSize = sizeof(second);
    ASSERT_EQ(CONFIG_OK, config_crypto_encrypt(data, sizeof(data), first, &firstSize));
    ASSERT_EQ(CONFIG_OK, config_deinit());
    ASSERT_EQ(CONFIG_OK, config_init(nullptr));
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_crypto_encrypt(data, sizeof(data), second, &secondSize));
    EXPECT_EQ(firstSize, secondSize);
    EXPECT_NE(0, memcmp(first + CONFIG_CRYPTO_HEADER_SIZE, second + CONFIG_CRYPTO_HEADER_SIZE,
        CONFIG_CRYPTO_NONCE_SIZE));
}

TEST_F(ConfigCryptoTest, TruncatedAndLegacyRecordsRejectedWithoutPlaintext) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    const uint8_t data[] = "secret";
    uint8_t record[128], output[128];
    size_t size = sizeof(record);
    ASSERT_EQ(CONFIG_OK, config_crypto_encrypt(data, sizeof(data), record, &size));
    for (size_t truncated = 0; truncated < size; ++truncated) {
        memset(output, 0x55, sizeof(output));
        size_t plainSize = sizeof(output);
        EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT,
            config_crypto_decrypt(record, truncated, output, &plainSize)) << truncated;
        for (uint8_t byte : output) { EXPECT_EQ(0x55u, byte); }
    }
    record[size] = 0;
    size_t plainSize = sizeof(output);
    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT, config_crypto_decrypt(record, size + 1, output, &plainSize));
    uint8_t legacy[64] = {0};
    plainSize = sizeof(output);
    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT, config_crypto_decrypt(legacy, sizeof(legacy), output, &plainSize));
    EXPECT_EQ(0u, config_crypto_get_encrypted_size(SIZE_MAX));
}

TEST_F(ConfigCryptoTest, DecryptExportFailsClosedWithoutKeyAndClearsOutput) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("secret", "confidential"));
    ASSERT_EQ(CONFIG_OK, config_clear_encryption_key());
    for (config_format_t format : {CONFIG_FORMAT_JSON, CONFIG_FORMAT_BINARY}) {
        uint8_t output[1024];
        memset(output, 0x55, sizeof(output));
        size_t actual = 99;
        EXPECT_EQ(CONFIG_ERROR_NO_ENCRYPTION_KEY, config_export(format, CONFIG_EXPORT_FLAG_DECRYPT,
            output, sizeof(output), &actual));
        EXPECT_EQ(0u, actual);
        for (uint8_t byte : output) { EXPECT_EQ(0u, byte); }
    }
}

TEST_F(ConfigCryptoTest, EncryptedSetterPreservesPersistentFlagAndRejectsReadonly) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    const char initial[] = "initial";
    ASSERT_EQ(CONFIG_OK, config_store_set("persistent", CONFIG_TYPE_STRING, initial,
        sizeof(initial), CONFIG_FLAG_PERSISTENT, 0));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("persistent", "secret"));
    uint8_t flags = 0;
    ASSERT_EQ(CONFIG_OK, config_store_get_flags("persistent", 0, &flags));
    EXPECT_EQ(CONFIG_FLAG_PERSISTENT | CONFIG_FLAG_ENCRYPTED, flags);
    ASSERT_EQ(CONFIG_OK, config_store_set("protected", CONFIG_TYPE_STRING, initial,
        sizeof(initial), CONFIG_FLAG_READONLY, 0));
    EXPECT_EQ(CONFIG_ERROR_READ_ONLY, config_set_str_encrypted("protected", "must not replace"));
    char output[32];
    ASSERT_EQ(CONFIG_OK, config_get_str("protected", output, sizeof(output)));
    EXPECT_STREQ("initial", output);
    const std::string oversized(CONFIG_MAX_MAX_VALUE_SIZE + 1, 's');
    EXPECT_EQ(CONFIG_ERROR_VALUE_TOO_LARGE, config_set_str_encrypted("large", oversized.c_str()));
}

TEST_F(ConfigCryptoTest, NamespaceReadAuthenticatesContextInsteadOfReturningCiphertext) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("secret", "verified"));
    config_ns_handle_t ns;
    ASSERT_EQ(CONFIG_OK, config_open_namespace("default", &ns));
    char output[32];
    ASSERT_EQ(CONFIG_OK, config_ns_get_str(ns, "secret", output, sizeof(output)));
    EXPECT_STREQ("verified", output);
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns));
    ASSERT_EQ(CONFIG_OK, config_open_namespace("other", &ns));
    uint8_t record[256];
    size_t size = sizeof(record);
    ASSERT_EQ(CONFIG_OK, config_store_get("secret", nullptr, record, &size, nullptr, 0));
    ASSERT_EQ(CONFIG_OK, config_store_set("secret", CONFIG_TYPE_STRING, record,
        size, CONFIG_FLAG_ENCRYPTED, 1));
    memset(output, 0x55, sizeof(output));
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_ns_get_str(ns, "secret", output, sizeof(output)));
    for (char byte : output) { EXPECT_EQ(0x55, byte); }
    uint32_t scalar = 42, result = 77;
    ASSERT_EQ(CONFIG_OK, config_store_set("scalar", CONFIG_TYPE_U32, &scalar,
        sizeof(scalar), CONFIG_FLAG_ENCRYPTED, 1));
    EXPECT_EQ(CONFIG_ERROR_UNSUPPORTED, config_ns_get_u32(ns, "scalar", &result, 0));
    EXPECT_EQ(77u, result);
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigCryptoTest, EncryptedLengthQueriesReturnAuthenticatedPlaintextSize) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("string", "seven77"));
    uint8_t data[] = {0, 1, 255, 0};
    ASSERT_EQ(CONFIG_OK, config_set_blob_encrypted("blob", data, sizeof(data)));
    size_t size = 99;
    ASSERT_EQ(CONFIG_OK, config_get_str_len("string", &size));
    EXPECT_EQ(7u, size);
    ASSERT_EQ(CONFIG_OK, config_get_blob_len("blob", &size));
    EXPECT_EQ(sizeof(data), size);
    ASSERT_EQ(CONFIG_OK, config_clear_encryption_key());
    size = 99;
    EXPECT_EQ(CONFIG_ERROR_NO_ENCRYPTION_KEY, config_get_str_len("string", &size));
    EXPECT_EQ(99u, size);
    EXPECT_EQ(CONFIG_ERROR_NO_ENCRYPTION_KEY, config_get_blob_len("blob", &size));
    EXPECT_EQ(99u, size);
}

TEST_F(ConfigCryptoTest, EncryptedLengthQueryRejectsTamperedRecordWithoutReturningMetadata) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("string", "secret"));
    uint8_t record[256];
    size_t recordSize = sizeof(record);
    ASSERT_EQ(CONFIG_OK, config_store_get("string", nullptr, record, &recordSize, nullptr, 0));
    record[recordSize - 1] ^= 1;
    ASSERT_EQ(CONFIG_OK, config_store_set("string", CONFIG_TYPE_STRING, record,
        recordSize, CONFIG_FLAG_ENCRYPTED, 0));
    size_t size = 99;
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_get_str_len("string", &size));
    EXPECT_EQ(99u, size);
}

TEST_F(ConfigCryptoTest, EncryptedJsonRoundTripPreservesRecordAndAuthenticatesAfterImport) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("password", "must stay secret"));
    const uint8_t blob[] = {0, 255, 1, 0, 42};
    ASSERT_EQ(CONFIG_OK, config_set_blob_encrypted("token", blob, sizeof(blob)));
    size_t required;
    ASSERT_EQ(CONFIG_OK, config_get_export_size(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE, &required));
    std::vector<char> json(required);
    size_t actual;
    ASSERT_EQ(CONFIG_OK, config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
        json.data(), json.size(), &actual));
    EXPECT_EQ(nullptr, strstr(json.data(), "must stay secret"));
    EXPECT_NE(nullptr, strstr(json.data(), "4e584346"));
    ASSERT_EQ(CONFIG_OK, config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR, json.data(), actual));
    char password[32];
    ASSERT_EQ(CONFIG_OK, config_get_str("password", password, sizeof(password)));
    EXPECT_STREQ("must stay secret", password);
    uint8_t token[32]; size_t size;
    ASSERT_EQ(CONFIG_OK, config_get_blob("token", token, sizeof(token), &size));
    EXPECT_EQ(sizeof(blob), size);
    EXPECT_EQ(0, memcmp(blob, token, size));
}

TEST_F(ConfigCryptoTest, TamperedEncryptedJsonAbortsWholeImportEvenWithClearFlag) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("secret", "original"));
    size_t required;
    ASSERT_EQ(CONFIG_OK, config_get_export_size(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE, &required));
    std::vector<char> json(required); size_t actual;
    ASSERT_EQ(CONFIG_OK, config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
        json.data(), json.size(), &actual));
    std::string damaged(json.data(), actual);
    const size_t value = damaged.find("\"value\":\"");
    ASSERT_NE(std::string::npos, value);
    const size_t end = damaged.find('"', value + 9);
    ASSERT_NE(std::string::npos, end);
    damaged[end - 1] = damaged[end - 1] == '0' ? '1' : '0';
    damaged.insert(1, "\"new\":{\"type\":\"i32\",\"value\":42},");
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_import(CONFIG_FORMAT_JSON,
        CONFIG_IMPORT_FLAG_CLEAR, damaged.data(), damaged.size()));
    bool exists;
    ASSERT_EQ(CONFIG_OK, config_exists("new", &exists)); EXPECT_FALSE(exists);
    char output[32];
    ASSERT_EQ(CONFIG_OK, config_get_str("secret", output, sizeof(output)));
    EXPECT_STREQ("original", output);
    ASSERT_EQ(CONFIG_OK, config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_SKIP_ERRORS,
        damaged.data(), damaged.size()));
    int32_t number;
    ASSERT_EQ(CONFIG_OK, config_get_i32("new", &number, 0)); EXPECT_EQ(42, number);
    ASSERT_EQ(CONFIG_OK, config_get_str("secret", output, sizeof(output)));
    EXPECT_STREQ("original", output);
}

TEST_F(ConfigCryptoTest, LongControlCharacterJsonRoundTripIsComplete) {
    ASSERT_EQ(CONFIG_OK, config_deinit());
    config_manager_config_t config = CONFIG_MANAGER_CONFIG_DEFAULT;
    config.max_value_size = CONFIG_MAX_MAX_VALUE_SIZE;
    ASSERT_EQ(CONFIG_OK, config_init(&config));
    ASSERT_GE(CONFIG_MAX_MAX_VALUE_SIZE, 901);
    std::string value(900, '\x01');
    value[10] = '"'; value[20] = '\\'; value[30] = '\n';
    ASSERT_EQ(CONFIG_OK, config_set_str("large", value.c_str()));
    size_t required;
    ASSERT_EQ(CONFIG_OK, config_get_export_size(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE, &required));
    std::vector<char> json(required); size_t actual;
    ASSERT_EQ(CONFIG_OK, config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
        json.data(), json.size(), &actual));
    EXPECT_GT(actual, 5000u);
    ASSERT_EQ(CONFIG_OK, config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR, json.data(), actual));
    std::vector<char> output(1000);
    ASSERT_EQ(CONFIG_OK, config_get_str("large", output.data(), output.size()));
    EXPECT_EQ(value, std::string(output.data()));
}

TEST_F(ConfigCryptoTest, InvalidJsonValuesDoNotMutateEarlierEntriesOrClearExistingData) {
    ASSERT_EQ(CONFIG_OK, config_set_str("original", "preserved"));
    for (const char* bad : {"{\"type\":\"u32\",\"value\":-1}",
            "{\"type\":\"i32\",\"value\":2147483648}",
            "{\"type\":\"i64\",\"value\":9223372036854775808}",
            "{\"type\":\"float\",\"value\":1e400}",
            "{\"type\":\"string\",\"value\":\"\\u0000bad\"}",
            "{\"type\":\"string\",\"value\":\"\\ud800\"}",
            "{\"type\":\"i32\",\"value\":1,\"unknown\":{\"nested\":1}}"}) {
        std::string json = "{\"new\":{\"type\":\"i32\",\"value\":7},\"invalid\":";
        json += bad; json += '}';
        EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT, config_import(CONFIG_FORMAT_JSON,
            CONFIG_IMPORT_FLAG_CLEAR, json.data(), json.size())) << bad;
        bool exists;
        ASSERT_EQ(CONFIG_OK, config_exists("new", &exists)); EXPECT_FALSE(exists);
        char output[32];
        ASSERT_EQ(CONFIG_OK, config_get_str("original", output, sizeof(output)));
        EXPECT_STREQ("preserved", output);
    }
    const char* unicode = "{\"text\":{\"value\":\"\\u4e2d\\ud83d\\ude80\",\"type\":\"string\"}}";
    ASSERT_EQ(CONFIG_OK, config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_NONE, unicode, strlen(unicode)));
    char output[32]; ASSERT_EQ(CONFIG_OK, config_get_str("text", output, sizeof(output)));
    EXPECT_STREQ("\xe4\xb8\xad\xf0\x9f\x9a\x80", output);
}

TEST_F(ConfigCryptoTest, BinaryV2UsesLittleEndianAndValidatesHeaderExtent) {
    ASSERT_EQ(CONFIG_OK, config_set_u32("word", 0x01020304));
    uint8_t bytes[256]; size_t actual;
    ASSERT_EQ(CONFIG_OK, config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE,
        bytes, sizeof(bytes), &actual));
    EXPECT_EQ(2u, bytes[4]);
    ASSERT_EQ(30u, actual);
    EXPECT_EQ(4u, bytes[26]); EXPECT_EQ(3u, bytes[27]);
    EXPECT_EQ(2u, bytes[28]); EXPECT_EQ(1u, bytes[29]);
    ASSERT_EQ(CONFIG_OK, config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_CLEAR, bytes, actual));
    uint32_t word;
    ASSERT_EQ(CONFIG_OK, config_get_u32("word", &word, 0)); EXPECT_EQ(0x01020304u, word);
    bytes[12] ^= 1;
    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT, config_import(CONFIG_FORMAT_BINARY,
        CONFIG_IMPORT_FLAG_CLEAR, bytes, actual));
    ASSERT_EQ(CONFIG_OK, config_get_u32("word", &word, 0)); EXPECT_EQ(0x01020304u, word);
}

TEST_F(ConfigCryptoTest, BinaryCiphertextAndExplicitDecryptedExportsRoundTrip) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("secret", "original"));
    uint8_t bytes[512]; size_t actual;
    ASSERT_EQ(CONFIG_OK, config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE,
        bytes, sizeof(bytes), &actual));
    ASSERT_EQ(CONFIG_OK, config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_CLEAR, bytes, actual));
    char output[32];
    ASSERT_EQ(CONFIG_OK, config_get_str("secret", output, sizeof(output)));
    EXPECT_STREQ("original", output);
    bytes[actual - 1] ^= 1;
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_import(CONFIG_FORMAT_BINARY,
        CONFIG_IMPORT_FLAG_CLEAR, bytes, actual));
    ASSERT_EQ(CONFIG_OK, config_get_str("secret", output, sizeof(output)));
    EXPECT_STREQ("original", output);
    ASSERT_EQ(CONFIG_OK, config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_DECRYPT,
        bytes, sizeof(bytes), &actual));
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_import(CONFIG_FORMAT_BINARY,
        CONFIG_IMPORT_FLAG_CLEAR, bytes, actual));
    ASSERT_EQ(CONFIG_OK, config_get_str("secret", output, sizeof(output)));
    EXPECT_STREQ("original", output);
    /* Replacing a sensitive key with plaintext requires an explicit removal;
     * CLEAR alone does not authorize a policy downgrade. */
    ASSERT_EQ(CONFIG_OK, config_delete("secret"));
    ASSERT_EQ(CONFIG_OK, config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_CLEAR, bytes, actual));
    ASSERT_EQ(CONFIG_OK, config_get_str("secret", output, sizeof(output)));
    EXPECT_STREQ("original", output);
    bool encrypted = true;
    ASSERT_EQ(CONFIG_OK, config_is_encrypted("secret", &encrypted)); EXPECT_FALSE(encrypted);
}

TEST_F(ConfigCryptoTest, ClearJsonImportCannotDowngradeExistingEncryptedIdentity) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("secret", "original"));
    ASSERT_EQ(CONFIG_OK, config_set_i32("untouched", 17));
    const char* json = "{\"earlier\":{\"type\":\"i32\",\"value\":9},"
                       "\"secret\":{\"type\":\"string\",\"value\":\"plaintext\"}}";
    for (config_import_flags_t flags : {CONFIG_IMPORT_FLAG_NONE, CONFIG_IMPORT_FLAG_CLEAR}) {
        EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_import(CONFIG_FORMAT_JSON,
            flags, json, strlen(json)));
    }
    bool encrypted = false, exists = true;
    ASSERT_EQ(CONFIG_OK, config_is_encrypted("secret", &encrypted)); EXPECT_TRUE(encrypted);
    ASSERT_EQ(CONFIG_OK, config_exists("earlier", &exists)); EXPECT_FALSE(exists);
    int32_t untouched = 0;
    ASSERT_EQ(CONFIG_OK, config_get_i32("untouched", &untouched, 0)); EXPECT_EQ(17, untouched);
    char value[32];
    ASSERT_EQ(CONFIG_OK, config_get_str("secret", value, sizeof(value))); EXPECT_STREQ("original", value);
}

TEST_F(ConfigCryptoTest, ClearNamespaceImportsPreserveEncryptionPolicyInBothFormats) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("secret", "original"));
    uint8_t cipher[512]; size_t cipherSize;
    ASSERT_EQ(CONFIG_OK, config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE,
        cipher, sizeof(cipher), &cipherSize));
    ASSERT_EQ(CONFIG_OK, config_import_namespace("default", CONFIG_FORMAT_BINARY,
        CONFIG_IMPORT_FLAG_CLEAR, cipher, cipherSize));
    uint8_t plain[512]; size_t plainSize;
    ASSERT_EQ(CONFIG_OK, config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_DECRYPT,
        plain, sizeof(plain), &plainSize));
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_import_namespace("default", CONFIG_FORMAT_BINARY,
        CONFIG_IMPORT_FLAG_CLEAR, plain, plainSize));
    const char* json = "{\"secret\":{\"type\":\"string\",\"value\":\"plaintext\"}}";
    EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_import_namespace("default", CONFIG_FORMAT_JSON,
        CONFIG_IMPORT_FLAG_CLEAR, json, strlen(json)));
    bool encrypted = false;
    ASSERT_EQ(CONFIG_OK, config_is_encrypted("secret", &encrypted)); EXPECT_TRUE(encrypted);
    char value[32];
    ASSERT_EQ(CONFIG_OK, config_get_str("secret", value, sizeof(value))); EXPECT_STREQ("original", value);
}

TEST_F(ConfigCryptoTest, NonDefaultNamespaceSecurityPolicySurvivesClearAndMergeImports) {
    ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32, CONFIG_CRYPTO_AES256_GCM));
    ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("secret", "original"));
    config_ns_handle_t ns;
    ASSERT_EQ(CONFIG_OK, config_open_namespace("motor", &ns));
    uint8_t nsId = 0;
    ASSERT_EQ(CONFIG_OK, config_namespace_get_id("motor", &nsId));
    ASSERT_NE(0u, nsId);
    /* Build a correctly authenticated fixture with the namespace's identity.
     * Config currently exposes encrypted setters for the default namespace;
     * this test exercises the supported authenticated import/read boundary. */
    uint8_t record[256]; size_t recordSize = sizeof(record);
    ASSERT_EQ(CONFIG_OK, config_store_get("secret", nullptr, record, &recordSize, nullptr, 0));
    uint8_t aad[CONFIG_CRYPTO_HEADER_SIZE + 3 + 6];
    memcpy(aad, record, CONFIG_CRYPTO_HEADER_SIZE);
    aad[CONFIG_CRYPTO_HEADER_SIZE] = nsId;
    aad[CONFIG_CRYPTO_HEADER_SIZE + 1] = CONFIG_TYPE_STRING;
    aad[CONFIG_CRYPTO_HEADER_SIZE + 2] = 6;
    memcpy(aad + CONFIG_CRYPTO_HEADER_SIZE + 3, "secret", 6);
    const uint8_t plaintext[] = "original";
    uint8_t* nonce = record + CONFIG_CRYPTO_HEADER_SIZE;
    uint8_t* ciphertext = nonce + CONFIG_CRYPTO_NONCE_SIZE;
    ASSERT_EQ(NX_CRYPTO_OK, nx_crypto_random(nonce, CONFIG_CRYPTO_NONCE_SIZE));
    ASSERT_EQ(NX_CRYPTO_OK, nx_crypto_seal(NX_CRYPTO_AES256_GCM, aes256_key, 32,
        nonce, aad, sizeof(aad), plaintext, sizeof(plaintext), ciphertext, ciphertext + sizeof(plaintext)));
    ASSERT_EQ(CONFIG_OK, config_store_set("secret", CONFIG_TYPE_STRING, record, recordSize,
        CONFIG_FLAG_ENCRYPTED | CONFIG_FLAG_PERSISTENT, nsId));
    char original[32];
    ASSERT_EQ(CONFIG_OK, config_ns_get_str(ns, "secret", original, sizeof(original)));
    EXPECT_STREQ("original", original);
    uint8_t plain[512]; size_t plainSize;
    ASSERT_EQ(CONFIG_OK, config_export_namespace("motor", CONFIG_FORMAT_BINARY,
        CONFIG_EXPORT_FLAG_DECRYPT, plain, sizeof(plain), &plainSize));
    const char* json = "{\"secret\":{\"type\":\"string\",\"value\":\"plaintext\"}}";
    for (config_import_flags_t flags : {CONFIG_IMPORT_FLAG_NONE, CONFIG_IMPORT_FLAG_CLEAR}) {
        EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_import_namespace("motor",
            CONFIG_FORMAT_JSON, flags, json, strlen(json)));
        EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_import_namespace("motor",
            CONFIG_FORMAT_BINARY, flags, plain, plainSize));
        EXPECT_EQ(CONFIG_ERROR_CRYPTO_FAILED, config_import(CONFIG_FORMAT_BINARY,
            flags, plain, plainSize));
    }
    uint8_t retainedFlags = 0;
    ASSERT_EQ(CONFIG_OK, config_store_get_flags("secret", nsId, &retainedFlags));
    EXPECT_EQ(CONFIG_FLAG_ENCRYPTED | CONFIG_FLAG_PERSISTENT, retainedFlags);
    ASSERT_EQ(CONFIG_OK, config_ns_get_str(ns, "secret", original, sizeof(original)));
    EXPECT_STREQ("original", original);
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns));
}

TEST_F(ConfigCryptoTest, JsonRejectsInvalidUtf8AndDuplicateEntriesWithoutMutation) {
    ASSERT_EQ(CONFIG_OK, config_set_str("original", "preserved"));
    const std::string invalidUtf8 = "{\"bad\":{\"type\":\"string\",\"value\":\"\xc0\xaf\"}}";
    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT, config_import(CONFIG_FORMAT_JSON,
        CONFIG_IMPORT_FLAG_CLEAR, invalidUtf8.data(), invalidUtf8.size()));
    const char* duplicate = "{\"same\":{\"type\":\"i32\",\"value\":1},\"same\":{\"type\":\"i32\",\"value\":2}}";
    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT, config_import(CONFIG_FORMAT_JSON,
        CONFIG_IMPORT_FLAG_CLEAR, duplicate, strlen(duplicate)));
    char output[32];
    ASSERT_EQ(CONFIG_OK, config_get_str("original", output, sizeof(output)));
    EXPECT_STREQ("preserved", output);
    ASSERT_EQ(CONFIG_OK, config_set_str("bad", "\xc0\xaf"));
    uint8_t exported[512]; size_t size = 99;
    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT, config_export(CONFIG_FORMAT_JSON,
        CONFIG_EXPORT_FLAG_NONE, exported, sizeof(exported), &size));
    EXPECT_EQ(0u, size);
}

TEST_F(ConfigCryptoTest, SkipErrorsCannotTurnMalformedJsonIntoClearAuthorization) {
    ASSERT_EQ(CONFIG_OK, config_set_i32("live", 17));
    for (const char* malformed : {"{\"bad\":{\"value\":}}",
            "{\"bad\":{\"value\":01}}", "{\"bad\":{\"value\":1e}}",
            "{\"bad\":{\"value\":+1}}", "{\"bad\":{\"value\":1.}}"}) {
        EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT, config_import(CONFIG_FORMAT_JSON,
            static_cast<config_import_flags_t>(CONFIG_IMPORT_FLAG_CLEAR | CONFIG_IMPORT_FLAG_SKIP_ERRORS),
            malformed, strlen(malformed)));
        int32_t live = 0;
        ASSERT_EQ(CONFIG_OK, config_get_i32("live", &live, 0)); EXPECT_EQ(17, live);
    }
    const char* semanticError = "{\"bad\":{\"type\":\"u32\",\"value\":-1},"
                                "\"valid\":{\"type\":\"i32\",\"value\":9}}";
    ASSERT_EQ(CONFIG_OK, config_import(CONFIG_FORMAT_JSON,
        CONFIG_IMPORT_FLAG_SKIP_ERRORS, semanticError, strlen(semanticError)));
    int32_t valid = 0;
    ASSERT_EQ(CONFIG_OK, config_get_i32("valid", &valid, 0)); EXPECT_EQ(9, valid);
}

TEST_F(ConfigCryptoTest, GlobalJsonRejectsNamespaceAmbiguityAndExplicitNamespaceExportWorks) {
    ASSERT_EQ(CONFIG_OK, config_set_str("same", "first"));
    config_ns_handle_t ns;
    ASSERT_EQ(CONFIG_OK, config_open_namespace("other", &ns));
    ASSERT_EQ(CONFIG_OK, config_ns_set_str(ns, "same", "second"));
    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns));
    size_t required = 99;
    EXPECT_EQ(CONFIG_ERROR_UNSUPPORTED,
        config_get_export_size(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE, &required));
    char output[1024]; size_t actual = 99;
    EXPECT_EQ(CONFIG_ERROR_UNSUPPORTED, config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
        output, sizeof(output), &actual));
    EXPECT_EQ(0u, actual);
    ASSERT_EQ(CONFIG_OK, config_export_namespace("other", CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
        output, sizeof(output), &actual));
    EXPECT_NE(nullptr, strstr(output, "second"));
    EXPECT_EQ(nullptr, strstr(output, "first"));
    ASSERT_EQ(CONFIG_OK, config_export_namespace("default", CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
        output, sizeof(output), &actual));
    EXPECT_NE(nullptr, strstr(output, "first"));
}

TEST_F(ConfigCryptoTest, JsonFloatRoundTripRetainsBinary32Value) {
    const float value = 1.23456776142120361328125f;
    ASSERT_EQ(CONFIG_OK, config_set_float("precise", value));
    char bytes[256]; size_t actual;
    ASSERT_EQ(CONFIG_OK, config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
        bytes, sizeof(bytes), &actual));
    ASSERT_EQ(CONFIG_OK, config_import(CONFIG_FORMAT_JSON, CONFIG_IMPORT_FLAG_CLEAR, bytes, actual));
    float decoded;
    ASSERT_EQ(CONFIG_OK, config_get_float("precise", &decoded, 0));
    EXPECT_EQ(0, memcmp(&value, &decoded, sizeof(value)));
}

TEST_F(ConfigCryptoTest, JsonRejectsNonfiniteFloatWhileBinaryPreservesItsBits) {
    uint32_t bits = 0x7fc00001u;
    float value;
    memcpy(&value, &bits, sizeof(value));
    ASSERT_EQ(CONFIG_OK, config_set_float("notFinite", value));
    uint8_t bytes[256]; memset(bytes, 0x55, sizeof(bytes)); size_t actual = 99;
    EXPECT_EQ(CONFIG_ERROR_INVALID_FORMAT, config_export(CONFIG_FORMAT_JSON, CONFIG_EXPORT_FLAG_NONE,
        bytes, sizeof(bytes), &actual));
    EXPECT_EQ(0u, actual);
    for (uint8_t byte : bytes) EXPECT_EQ(0u, byte);
    ASSERT_EQ(CONFIG_OK, config_export(CONFIG_FORMAT_BINARY, CONFIG_EXPORT_FLAG_NONE,
        bytes, sizeof(bytes), &actual));
    ASSERT_EQ(CONFIG_OK, config_import(CONFIG_FORMAT_BINARY, CONFIG_IMPORT_FLAG_CLEAR, bytes, actual));
    float decoded;
    ASSERT_EQ(CONFIG_OK, config_get_float("notFinite", &decoded, 0));
    EXPECT_EQ(0, memcmp(&value, &decoded, sizeof(value)));
}
