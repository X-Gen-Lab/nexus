/**
 * \file            test_nx_option_bytes_properties.cpp
 * \brief           Option Bytes Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Option Bytes peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 14: Option Bytes Write Protection**
 * **Validates: Requirements 9.4**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "devices/native_option_bytes_helpers.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_option_bytes.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Option Bytes Property Test Fixture
 */
class OptionBytesPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_option_bytes_t* opt_bytes = nullptr;

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Reset all Option Bytes instances */
        native_option_bytes_reset_all();

        /* Get Option Bytes instance */
        opt_bytes = nx_factory_option_bytes(0);
        ASSERT_NE(nullptr, opt_bytes);

        /* Initialize Option Bytes */
        nx_lifecycle_t* lifecycle =
            (nx_lifecycle_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t));
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize Option Bytes */
        if (opt_bytes != nullptr) {
            nx_lifecycle_t* lifecycle =
                (nx_lifecycle_t*)((uint8_t*)opt_bytes +
                                  sizeof(nx_option_bytes_t));
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_option_bytes_reset_all();
    }

    /**
     * \brief       Generate random user data
     */
    std::vector<uint8_t> randomUserData(size_t len) {
        std::uniform_int_distribution<int> byte_dist(0, 255);
        std::vector<uint8_t> data(len);
        for (size_t i = 0; i < len; i++) {
            data[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        return data;
    }

    /**
     * \brief       Generate random protection level (0-2)
     */
    uint8_t randomProtectionLevel() {
        std::uniform_int_distribution<int> level_dist(0, 2);
        return static_cast<uint8_t>(level_dist(rng));
    }

    /**
     * \brief       Generate random invalid protection level (> 2)
     */
    uint8_t randomInvalidProtectionLevel() {
        std::uniform_int_distribution<int> level_dist(3, 255);
        return static_cast<uint8_t>(level_dist(rng));
    }
};

/*---------------------------------------------------------------------------*/
/* Property 14: Option Bytes Write Protection                                */
/* *For any* option bytes configuration, when write protection is enabled,   */
/* attempts to write SHALL fail with NX_ERR_PERMISSION.                      */
/* **Validates: Requirements 9.4**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-platform-improvements, Property 14: Option Bytes Write
 * Protection
 *
 * *For any* user data, when write protection is enabled, set_user_data()
 * should return NX_ERR_PERMISSION.
 *
 * **Validates: Requirements 9.4**
 */
TEST_F(OptionBytesPropertyTest, Property14_WriteProtectionBlocksUserDataWrite) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random user data */
        std::uniform_int_distribution<size_t> len_dist(1, 16);
        size_t len = len_dist(rng);
        std::vector<uint8_t> data = randomUserData(len);

        /* Enable write protection */
        ASSERT_EQ(NX_OK, native_option_bytes_set_write_protection(0, true));

        /* Attempt to write user data */
        nx_status_t status =
            opt_bytes->set_user_data(opt_bytes, data.data(), len);

        /* Should fail with permission error */
        EXPECT_EQ(NX_ERR_PERMISSION, status)
            << "Iteration " << test_iter
            << ": Write protection did not block user data write";

        /* Disable write protection for next iteration */
        ASSERT_EQ(NX_OK, native_option_bytes_set_write_protection(0, false));
    }
}

/**
 * Feature: native-platform-improvements, Property 14: Option Bytes Write
 * Protection
 *
 * *For any* protection level, when write protection is enabled,
 * set_read_protection() should return NX_ERR_PERMISSION.
 *
 * **Validates: Requirements 9.4**
 */
TEST_F(OptionBytesPropertyTest,
       Property14_WriteProtectionBlocksProtectionLevelChange) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random protection level */
        uint8_t level = randomProtectionLevel();

        /* Enable write protection */
        ASSERT_EQ(NX_OK, native_option_bytes_set_write_protection(0, true));

        /* Attempt to set protection level */
        nx_status_t status = opt_bytes->set_read_protection(opt_bytes, level);

        /* Should fail with permission error */
        EXPECT_EQ(NX_ERR_PERMISSION, status)
            << "Iteration " << test_iter
            << ": Write protection did not block protection level change";

        /* Disable write protection for next iteration */
        ASSERT_EQ(NX_OK, native_option_bytes_set_write_protection(0, false));
    }
}

/**
 * Feature: native-platform-improvements, Property 14: Option Bytes Write
 * Protection
 *
 * *For any* pending changes, when write protection is enabled, apply()
 * should return NX_ERR_PERMISSION.
 *
 * **Validates: Requirements 9.4**
 */
TEST_F(OptionBytesPropertyTest, Property14_WriteProtectionBlocksApply) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random user data */
        std::uniform_int_distribution<size_t> len_dist(1, 16);
        size_t len = len_dist(rng);
        std::vector<uint8_t> data = randomUserData(len);

        /* Write user data (creates pending changes) */
        ASSERT_EQ(NX_OK, opt_bytes->set_user_data(opt_bytes, data.data(), len));

        /* Enable write protection */
        ASSERT_EQ(NX_OK, native_option_bytes_set_write_protection(0, true));

        /* Attempt to apply changes */
        nx_status_t status = opt_bytes->apply(opt_bytes);

        /* Should fail with permission error */
        EXPECT_EQ(NX_ERR_PERMISSION, status)
            << "Iteration " << test_iter
            << ": Write protection did not block apply";

        /* Disable write protection and reset for next iteration */
        ASSERT_EQ(NX_OK, native_option_bytes_set_write_protection(0, false));
        native_option_bytes_reset(0);

        /* Reinitialize */
        nx_lifecycle_t* lifecycle =
            (nx_lifecycle_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t));
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }
}

/**
 * Feature: native-platform-improvements, Property 14: Option Bytes Write
 * Protection
 *
 * *For any* user data, when write protection is disabled, set_user_data()
 * should succeed.
 *
 * **Validates: Requirements 9.2, 9.3**
 */
TEST_F(OptionBytesPropertyTest, Property14_NoWriteProtectionAllowsWrite) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random user data */
        std::uniform_int_distribution<size_t> len_dist(1, 16);
        size_t len = len_dist(rng);
        std::vector<uint8_t> data = randomUserData(len);

        /* Ensure write protection is disabled */
        ASSERT_EQ(NX_OK, native_option_bytes_set_write_protection(0, false));

        /* Write user data */
        nx_status_t status =
            opt_bytes->set_user_data(opt_bytes, data.data(), len);

        /* Should succeed */
        EXPECT_EQ(NX_OK, status)
            << "Iteration " << test_iter
            << ": Write failed when write protection was disabled";
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Properties: User Data Round Trip                               */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-platform-improvements, Property: User Data Round Trip
 *
 * *For any* user data, writing, applying, and reading should return the
 * same data.
 *
 * **Validates: Requirements 9.2, 9.3**
 */
TEST_F(OptionBytesPropertyTest, UserDataRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random user data */
        std::uniform_int_distribution<size_t> len_dist(1, 16);
        size_t len = len_dist(rng);
        std::vector<uint8_t> write_data = randomUserData(len);

        /* Write user data */
        ASSERT_EQ(NX_OK,
                  opt_bytes->set_user_data(opt_bytes, write_data.data(), len));

        /* Apply changes */
        ASSERT_EQ(NX_OK, opt_bytes->apply(opt_bytes));

        /* Read user data */
        std::vector<uint8_t> read_data(len);
        ASSERT_EQ(NX_OK,
                  opt_bytes->get_user_data(opt_bytes, read_data.data(), len));

        /* Should match */
        EXPECT_EQ(write_data, read_data)
            << "Iteration " << test_iter << ": User data round trip failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Properties: Protection Level Validation                        */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-platform-improvements, Property: Protection Level Validation
 *
 * *For any* valid protection level (0-2), set_read_protection() should
 * succeed.
 *
 * **Validates: Requirements 9.2, 9.3**
 */
TEST_F(OptionBytesPropertyTest, ValidProtectionLevelAccepted) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random valid protection level */
        uint8_t level = randomProtectionLevel();

        /* Set protection level */
        nx_status_t status = opt_bytes->set_read_protection(opt_bytes, level);

        /* Should succeed */
        EXPECT_EQ(NX_OK, status)
            << "Iteration " << test_iter
            << ": Valid protection level rejected: " << (int)level;

        /* Apply and verify */
        ASSERT_EQ(NX_OK, opt_bytes->apply(opt_bytes));
        EXPECT_EQ(level, opt_bytes->get_read_protection(opt_bytes))
            << "Iteration " << test_iter;
    }
}

/**
 * Feature: native-platform-improvements, Property: Protection Level Validation
 *
 * *For any* invalid protection level (> 2), set_read_protection() should
 * return NX_ERR_INVALID_PARAM.
 *
 * **Validates: Requirements 9.2, 9.3**
 */
TEST_F(OptionBytesPropertyTest, InvalidProtectionLevelRejected) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random invalid protection level */
        uint8_t level = randomInvalidProtectionLevel();

        /* Attempt to set invalid protection level */
        nx_status_t status = opt_bytes->set_read_protection(opt_bytes, level);

        /* Should fail */
        EXPECT_EQ(NX_ERR_INVALID_PARAM, status)
            << "Iteration " << test_iter
            << ": Invalid protection level accepted: " << (int)level;
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Properties: Pending Changes Behavior                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-platform-improvements, Property: Pending Changes
 *
 * *For any* write operation, changes should not be visible until apply()
 * is called.
 *
 * **Validates: Requirements 9.2, 9.3**
 */
TEST_F(OptionBytesPropertyTest, ChangesNotVisibleUntilApply) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random user data */
        std::uniform_int_distribution<size_t> len_dist(1, 16);
        size_t len = len_dist(rng);
        std::vector<uint8_t> new_data = randomUserData(len);

        /* Read current data */
        std::vector<uint8_t> old_data(len);
        ASSERT_EQ(NX_OK,
                  opt_bytes->get_user_data(opt_bytes, old_data.data(), len));

        /* Write new data (without applying) */
        ASSERT_EQ(NX_OK,
                  opt_bytes->set_user_data(opt_bytes, new_data.data(), len));

        /* Read data again */
        std::vector<uint8_t> current_data(len);
        ASSERT_EQ(NX_OK, opt_bytes->get_user_data(opt_bytes,
                                                  current_data.data(), len));

        /* Should still match old data (changes not applied) */
        EXPECT_EQ(old_data, current_data)
            << "Iteration " << test_iter << ": Changes visible before apply()";

        /* Apply changes */
        ASSERT_EQ(NX_OK, opt_bytes->apply(opt_bytes));

        /* Read data again */
        ASSERT_EQ(NX_OK, opt_bytes->get_user_data(opt_bytes,
                                                  current_data.data(), len));

        /* Should now match new data */
        EXPECT_EQ(new_data, current_data)
            << "Iteration " << test_iter
            << ": Changes not visible after apply()";
    }
}

/**
 * Feature: native-platform-improvements, Property: Pending Changes
 *
 * *For any* protection level change, the change should not be visible until
 * apply() is called.
 *
 * **Validates: Requirements 9.2, 9.3**
 */
TEST_F(OptionBytesPropertyTest, ProtectionLevelChangeNotVisibleUntilApply) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Get current protection level */
        uint8_t old_level = opt_bytes->get_read_protection(opt_bytes);

        /* Generate different protection level */
        uint8_t new_level = (old_level + 1) % 3;

        /* Set new protection level (without applying) */
        ASSERT_EQ(NX_OK, opt_bytes->set_read_protection(opt_bytes, new_level));

        /* Get protection level again */
        uint8_t current_level = opt_bytes->get_read_protection(opt_bytes);

        /* Should still be old level (changes not applied) */
        EXPECT_EQ(old_level, current_level)
            << "Iteration " << test_iter
            << ": Protection level change visible before apply()";

        /* Apply changes */
        ASSERT_EQ(NX_OK, opt_bytes->apply(opt_bytes));

        /* Get protection level again */
        current_level = opt_bytes->get_read_protection(opt_bytes);

        /* Should now be new level */
        EXPECT_EQ(new_level, current_level)
            << "Iteration " << test_iter
            << ": Protection level change not visible after apply()";
    }
}
