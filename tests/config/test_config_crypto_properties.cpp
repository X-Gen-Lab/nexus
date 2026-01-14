/**
 * \file            test_config_crypto_properties.cpp
 * \brief           Config Manager Encryption Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Config Manager encryption functionality.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 7: Encryption Transparency**
 * **Validates: Requirements 12.1, 12.2**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Config Crypto Property Test Fixture
 */
class ConfigCryptoPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    /* AES-128 test key (16 bytes) */
    const uint8_t aes128_key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                    0x0c, 0x0d, 0x0e, 0x0f};

    /* AES-256 test key (32 bytes) */
    const uint8_t aes256_key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};

    void SetUp() override {
        rng.seed(std::random_device{}());
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

    /**
     * \brief       Generate random string value (printable ASCII only)
     */
    std::string randomString() {
        std::uniform_int_distribution<int> len_dist(1, 100);
        std::uniform_int_distribution<int> char_dist(0, 61);
        int len = len_dist(rng);
        std::string str;
        const char* safe_chars =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        for (int i = 0; i < len; ++i) {
            str += safe_chars[char_dist(rng)];
        }
        return str;
    }

    /**
     * \brief       Generate random blob data
     */
    std::vector<uint8_t> randomBlob() {
        std::uniform_int_distribution<int> len_dist(1, 200);
        std::uniform_int_distribution<int> byte_dist(0, 255);
        int len = len_dist(rng);
        std::vector<uint8_t> blob(len);
        for (int i = 0; i < len; ++i) {
            blob[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        return blob;
    }

    /**
     * \brief       Generate random AES key
     */
    std::vector<uint8_t> randomAESKey(bool aes256) {
        std::uniform_int_distribution<int> byte_dist(0, 255);
        size_t key_len = aes256 ? 32 : 16;
        std::vector<uint8_t> key(key_len);
        for (size_t i = 0; i < key_len; ++i) {
            key[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        return key;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 7: Encryption Transparency                                       */
/* *For any* encrypted key, getting the value with correct encryption key    */
/* SHALL return the original plaintext value.                                */
/* **Validates: Requirements 12.1, 12.2**                                    */
/*---------------------------------------------------------------------------*/

/**
 * Feature: config-manager, Property 7: Encryption Transparency (String,
 * AES-128)
 *
 * *For any* string value encrypted with AES-128, getting the value with the
 * correct encryption key SHALL return the original plaintext value.
 *
 * **Validates: Requirements 12.1, 12.2**
 */
TEST_F(ConfigCryptoPropertyTest, Property7_EncryptionTransparencyStringAES128) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Set up encryption with AES-128 */
        ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes128_key, 16,
                                                       CONFIG_CRYPTO_AES128))
            << "Iteration " << test_iter << ": set_encryption_key failed";

        /* Generate random string */
        std::string original_value = randomString();
        std::string key = "enc.str." + std::to_string(test_iter);

        /* Store encrypted */
        ASSERT_EQ(CONFIG_OK,
                  config_set_str_encrypted(key.c_str(), original_value.c_str()))
            << "Iteration " << test_iter << ": set_str_encrypted failed";

        /* Verify it's marked as encrypted */
        bool is_encrypted = false;
        ASSERT_EQ(CONFIG_OK, config_is_encrypted(key.c_str(), &is_encrypted))
            << "Iteration " << test_iter << ": is_encrypted failed";
        EXPECT_TRUE(is_encrypted) << "Iteration " << test_iter
                                  << ": value should be marked encrypted";

        /* Read back - should be decrypted automatically */
        char buffer[256];
        ASSERT_EQ(CONFIG_OK,
                  config_get_str(key.c_str(), buffer, sizeof(buffer)))
            << "Iteration " << test_iter << ": get_str failed";

        EXPECT_STREQ(original_value.c_str(), buffer)
            << "Iteration " << test_iter
            << ": decrypted value doesn't match original";
    }
}

/**
 * Feature: config-manager, Property 7: Encryption Transparency (String,
 * AES-256)
 *
 * *For any* string value encrypted with AES-256, getting the value with the
 * correct encryption key SHALL return the original plaintext value.
 *
 * **Validates: Requirements 12.1, 12.2**
 */
TEST_F(ConfigCryptoPropertyTest, Property7_EncryptionTransparencyStringAES256) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Set up encryption with AES-256 */
        ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32,
                                                       CONFIG_CRYPTO_AES256))
            << "Iteration " << test_iter << ": set_encryption_key failed";

        /* Generate random string */
        std::string original_value = randomString();
        std::string key = "enc256.str." + std::to_string(test_iter);

        /* Store encrypted */
        ASSERT_EQ(CONFIG_OK,
                  config_set_str_encrypted(key.c_str(), original_value.c_str()))
            << "Iteration " << test_iter << ": set_str_encrypted failed";

        /* Read back - should be decrypted automatically */
        char buffer[256];
        ASSERT_EQ(CONFIG_OK,
                  config_get_str(key.c_str(), buffer, sizeof(buffer)))
            << "Iteration " << test_iter << ": get_str failed";

        EXPECT_STREQ(original_value.c_str(), buffer)
            << "Iteration " << test_iter
            << ": decrypted value doesn't match original";
    }
}

/**
 * Feature: config-manager, Property 7: Encryption Transparency (Blob, AES-128)
 *
 * *For any* blob value encrypted with AES-128, getting the value with the
 * correct encryption key SHALL return the original plaintext value.
 *
 * **Validates: Requirements 12.1, 12.2**
 */
TEST_F(ConfigCryptoPropertyTest, Property7_EncryptionTransparencyBlobAES128) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Set up encryption with AES-128 */
        ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes128_key, 16,
                                                       CONFIG_CRYPTO_AES128))
            << "Iteration " << test_iter << ": set_encryption_key failed";

        /* Generate random blob */
        std::vector<uint8_t> original_value = randomBlob();
        std::string key = "enc.blob." + std::to_string(test_iter);

        /* Store encrypted */
        ASSERT_EQ(CONFIG_OK,
                  config_set_blob_encrypted(key.c_str(), original_value.data(),
                                            original_value.size()))
            << "Iteration " << test_iter << ": set_blob_encrypted failed";

        /* Verify it's marked as encrypted */
        bool is_encrypted = false;
        ASSERT_EQ(CONFIG_OK, config_is_encrypted(key.c_str(), &is_encrypted))
            << "Iteration " << test_iter << ": is_encrypted failed";
        EXPECT_TRUE(is_encrypted) << "Iteration " << test_iter
                                  << ": value should be marked encrypted";

        /* Read back - should be decrypted automatically */
        std::vector<uint8_t> buffer(original_value.size() + 100);
        size_t actual_size = 0;
        ASSERT_EQ(CONFIG_OK, config_get_blob(key.c_str(), buffer.data(),
                                             buffer.size(), &actual_size))
            << "Iteration " << test_iter << ": get_blob failed";

        EXPECT_EQ(original_value.size(), actual_size)
            << "Iteration " << test_iter << ": size mismatch";
        EXPECT_EQ(0, memcmp(original_value.data(), buffer.data(),
                            original_value.size()))
            << "Iteration " << test_iter
            << ": decrypted blob doesn't match original";
    }
}

/**
 * Feature: config-manager, Property 7: Encryption Transparency (Blob, AES-256)
 *
 * *For any* blob value encrypted with AES-256, getting the value with the
 * correct encryption key SHALL return the original plaintext value.
 *
 * **Validates: Requirements 12.1, 12.2**
 */
TEST_F(ConfigCryptoPropertyTest, Property7_EncryptionTransparencyBlobAES256) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Set up encryption with AES-256 */
        ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes256_key, 32,
                                                       CONFIG_CRYPTO_AES256))
            << "Iteration " << test_iter << ": set_encryption_key failed";

        /* Generate random blob */
        std::vector<uint8_t> original_value = randomBlob();
        std::string key = "enc256.blob." + std::to_string(test_iter);

        /* Store encrypted */
        ASSERT_EQ(CONFIG_OK,
                  config_set_blob_encrypted(key.c_str(), original_value.data(),
                                            original_value.size()))
            << "Iteration " << test_iter << ": set_blob_encrypted failed";

        /* Read back - should be decrypted automatically */
        std::vector<uint8_t> buffer(original_value.size() + 100);
        size_t actual_size = 0;
        ASSERT_EQ(CONFIG_OK, config_get_blob(key.c_str(), buffer.data(),
                                             buffer.size(), &actual_size))
            << "Iteration " << test_iter << ": get_blob failed";

        EXPECT_EQ(original_value.size(), actual_size)
            << "Iteration " << test_iter << ": size mismatch";
        EXPECT_EQ(0, memcmp(original_value.data(), buffer.data(),
                            original_value.size()))
            << "Iteration " << test_iter
            << ": decrypted blob doesn't match original";
    }
}

/**
 * Feature: config-manager, Property 7: Encryption Transparency (Random Key)
 *
 * *For any* randomly generated encryption key and string value, the
 * encryption/decryption round-trip SHALL preserve the original value.
 *
 * **Validates: Requirements 12.1, 12.2**
 */
TEST_F(ConfigCryptoPropertyTest, Property7_EncryptionTransparencyRandomKey) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Randomly choose AES-128 or AES-256 */
        std::uniform_int_distribution<int> algo_dist(0, 1);
        bool use_aes256 = algo_dist(rng) == 1;

        /* Generate random key */
        std::vector<uint8_t> key_data = randomAESKey(use_aes256);
        config_crypto_algo_t algo =
            use_aes256 ? CONFIG_CRYPTO_AES256 : CONFIG_CRYPTO_AES128;

        /* Set up encryption */
        ASSERT_EQ(CONFIG_OK, config_set_encryption_key(key_data.data(),
                                                       key_data.size(), algo))
            << "Iteration " << test_iter << ": set_encryption_key failed";

        /* Generate random string */
        std::string original_value = randomString();
        std::string config_key = "rndkey.str." + std::to_string(test_iter);

        /* Store encrypted */
        ASSERT_EQ(CONFIG_OK, config_set_str_encrypted(config_key.c_str(),
                                                      original_value.c_str()))
            << "Iteration " << test_iter << ": set_str_encrypted failed";

        /* Read back - should be decrypted automatically */
        char buffer[256];
        ASSERT_EQ(CONFIG_OK,
                  config_get_str(config_key.c_str(), buffer, sizeof(buffer)))
            << "Iteration " << test_iter << ": get_str failed";

        EXPECT_STREQ(original_value.c_str(), buffer)
            << "Iteration " << test_iter
            << ": decrypted value doesn't match original (algo="
            << (use_aes256 ? "AES-256" : "AES-128") << ")";
    }
}

/**
 * Feature: config-manager, Property: Encrypted vs Plain Isolation
 *
 * *For any* mix of encrypted and plain values, each value SHALL be
 * retrievable correctly regardless of the other values' encryption status.
 *
 * **Validates: Requirements 12.1, 12.2**
 */
TEST_F(ConfigCryptoPropertyTest, Property_EncryptedPlainIsolation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Set up encryption */
        ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes128_key, 16,
                                                       CONFIG_CRYPTO_AES128));

        /* Generate random values */
        std::string plain_value = randomString();
        std::string encrypted_value = randomString();

        /* Store both plain and encrypted */
        ASSERT_EQ(CONFIG_OK, config_set_str("plain.key", plain_value.c_str()));
        ASSERT_EQ(CONFIG_OK, config_set_str_encrypted("encrypted.key",
                                                      encrypted_value.c_str()));

        /* Verify plain value */
        char plain_buffer[256];
        ASSERT_EQ(CONFIG_OK, config_get_str("plain.key", plain_buffer,
                                            sizeof(plain_buffer)));
        EXPECT_STREQ(plain_value.c_str(), plain_buffer)
            << "Iteration " << test_iter << ": plain value mismatch";

        /* Verify encrypted value */
        char encrypted_buffer[256];
        ASSERT_EQ(CONFIG_OK, config_get_str("encrypted.key", encrypted_buffer,
                                            sizeof(encrypted_buffer)));
        EXPECT_STREQ(encrypted_value.c_str(), encrypted_buffer)
            << "Iteration " << test_iter << ": encrypted value mismatch";

        /* Verify encryption status */
        bool is_plain_encrypted = true;
        bool is_encrypted_encrypted = false;
        ASSERT_EQ(CONFIG_OK,
                  config_is_encrypted("plain.key", &is_plain_encrypted));
        ASSERT_EQ(CONFIG_OK, config_is_encrypted("encrypted.key",
                                                 &is_encrypted_encrypted));
        EXPECT_FALSE(is_plain_encrypted)
            << "Iteration " << test_iter
            << ": plain key should not be encrypted";
        EXPECT_TRUE(is_encrypted_encrypted)
            << "Iteration " << test_iter
            << ": encrypted key should be encrypted";
    }
}

/**
 * Feature: config-manager, Property: Multiple Encrypted Values
 *
 * *For any* number of encrypted values stored, each SHALL be retrievable
 * correctly with the same encryption key.
 *
 * **Validates: Requirements 12.1, 12.2**
 */
TEST_F(ConfigCryptoPropertyTest, Property_MultipleEncryptedValues) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Set up encryption */
        ASSERT_EQ(CONFIG_OK, config_set_encryption_key(aes128_key, 16,
                                                       CONFIG_CRYPTO_AES128));

        /* Generate random number of entries (2-5) */
        std::uniform_int_distribution<int> count_dist(2, 5);
        int num_entries = count_dist(rng);

        /* Store original values */
        std::vector<std::pair<std::string, std::string>> original_values;
        for (int i = 0; i < num_entries; ++i) {
            std::string key = "multi.enc." + std::to_string(i);
            std::string value = randomString();
            original_values.push_back({key, value});
            ASSERT_EQ(CONFIG_OK,
                      config_set_str_encrypted(key.c_str(), value.c_str()))
                << "Iteration " << test_iter
                << ": set_str_encrypted failed for " << key;
        }

        /* Verify all values */
        for (const auto& [key, expected_value] : original_values) {
            char buffer[256];
            ASSERT_EQ(CONFIG_OK,
                      config_get_str(key.c_str(), buffer, sizeof(buffer)))
                << "Iteration " << test_iter << ": get_str failed for " << key;
            EXPECT_STREQ(expected_value.c_str(), buffer)
                << "Iteration " << test_iter << ": value mismatch for " << key;
        }
    }
}
