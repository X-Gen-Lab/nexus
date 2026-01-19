/**
 * \file            test_nx_option_bytes.cpp
 * \brief           Option Bytes Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for Option Bytes peripheral implementation.
 *                  Requirements: 9.1-9.7, 10.1-10.6
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_option_bytes.h"
#include "hal/interface/nx_power.h"
#include "native_option_bytes_test.h"
}

/**
 * \brief           Option Bytes Test Fixture
 */
class OptionBytesTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all Option Bytes instances before each test */
        nx_option_bytes_native_reset_all();

        /* Get Option Bytes instance */
        opt_bytes = nx_option_bytes_native_get(0);
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
        nx_option_bytes_native_reset_all();
    }

    nx_option_bytes_t* opt_bytes = nullptr;
};

/*---------------------------------------------------------------------------*/
/* User Data Read/Write Tests - Requirements 9.2, 9.3                        */
/*---------------------------------------------------------------------------*/

TEST_F(OptionBytesTest, ReadWriteUserData) {
    /* Write user data */
    uint8_t write_data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    EXPECT_EQ(NX_OK, opt_bytes->set_user_data(opt_bytes, write_data, 16));

    /* Apply changes */
    EXPECT_EQ(NX_OK, opt_bytes->apply(opt_bytes));

    /* Read user data */
    uint8_t read_data[16] = {0};
    EXPECT_EQ(NX_OK, opt_bytes->get_user_data(opt_bytes, read_data, 16));

    /* Verify data matches */
    EXPECT_EQ(0, memcmp(write_data, read_data, 16));
}

TEST_F(OptionBytesTest, ReadWritePartialUserData) {
    /* Write partial user data */
    uint8_t write_data[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

    EXPECT_EQ(NX_OK, opt_bytes->set_user_data(opt_bytes, write_data, 8));

    /* Apply changes */
    EXPECT_EQ(NX_OK, opt_bytes->apply(opt_bytes));

    /* Read partial user data */
    uint8_t read_data[8] = {0};
    EXPECT_EQ(NX_OK, opt_bytes->get_user_data(opt_bytes, read_data, 8));

    /* Verify data matches */
    EXPECT_EQ(0, memcmp(write_data, read_data, 8));
}

TEST_F(OptionBytesTest, WriteUserDataInvalidParams) {
    uint8_t data[16] = {0};

    /* NULL pointer */
    EXPECT_EQ(NX_ERR_NULL_PTR,
              opt_bytes->set_user_data(opt_bytes, nullptr, 16));

    /* Zero length */
    EXPECT_EQ(NX_ERR_INVALID_PARAM,
              opt_bytes->set_user_data(opt_bytes, data, 0));

    /* Length too large */
    EXPECT_EQ(NX_ERR_INVALID_PARAM,
              opt_bytes->set_user_data(opt_bytes, data, 17));
}

TEST_F(OptionBytesTest, ReadUserDataInvalidParams) {
    /* NULL pointer */
    EXPECT_EQ(NX_ERR_NULL_PTR,
              opt_bytes->get_user_data(opt_bytes, nullptr, 16));

    /* Zero length */
    uint8_t data[16] = {0};
    EXPECT_EQ(NX_ERR_INVALID_PARAM,
              opt_bytes->get_user_data(opt_bytes, data, 0));

    /* Length too large */
    EXPECT_EQ(NX_ERR_INVALID_PARAM,
              opt_bytes->get_user_data(opt_bytes, data, 17));
}

/*---------------------------------------------------------------------------*/
/* Read Protection Tests - Requirements 9.2, 9.3                             */
/*---------------------------------------------------------------------------*/

TEST_F(OptionBytesTest, SetGetReadProtection) {
    /* Default protection level should be 0 */
    EXPECT_EQ(0, opt_bytes->get_read_protection(opt_bytes));

    /* Set protection level 1 */
    EXPECT_EQ(NX_OK, opt_bytes->set_read_protection(opt_bytes, 1));
    EXPECT_EQ(NX_OK, opt_bytes->apply(opt_bytes));
    EXPECT_EQ(1, opt_bytes->get_read_protection(opt_bytes));

    /* Set protection level 2 */
    EXPECT_EQ(NX_OK, opt_bytes->set_read_protection(opt_bytes, 2));
    EXPECT_EQ(NX_OK, opt_bytes->apply(opt_bytes));
    EXPECT_EQ(2, opt_bytes->get_read_protection(opt_bytes));

    /* Set back to level 0 */
    EXPECT_EQ(NX_OK, opt_bytes->set_read_protection(opt_bytes, 0));
    EXPECT_EQ(NX_OK, opt_bytes->apply(opt_bytes));
    EXPECT_EQ(0, opt_bytes->get_read_protection(opt_bytes));
}

TEST_F(OptionBytesTest, SetReadProtectionInvalidLevel) {
    /* Invalid protection level (> 2) */
    EXPECT_EQ(NX_ERR_INVALID_PARAM,
              opt_bytes->set_read_protection(opt_bytes, 3));
}

/*---------------------------------------------------------------------------*/
/* Write Protection Tests - Requirements 9.4                                 */
/*---------------------------------------------------------------------------*/

TEST_F(OptionBytesTest, WriteProtection) {
    uint8_t data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    /* Enable write protection */
    EXPECT_EQ(NX_OK, nx_option_bytes_native_set_write_protection(0, true));

    /* Attempt to write should fail */
    EXPECT_EQ(NX_ERR_PERMISSION, opt_bytes->set_user_data(opt_bytes, data, 16));

    /* Attempt to set protection level should fail */
    EXPECT_EQ(NX_ERR_PERMISSION, opt_bytes->set_read_protection(opt_bytes, 1));

    /* Disable write protection */
    EXPECT_EQ(NX_OK, nx_option_bytes_native_set_write_protection(0, false));

    /* Write should now succeed */
    EXPECT_EQ(NX_OK, opt_bytes->set_user_data(opt_bytes, data, 16));
    EXPECT_EQ(NX_OK, opt_bytes->apply(opt_bytes));
}

/*---------------------------------------------------------------------------*/
/* Pending Changes Tests - Requirements 9.2, 9.3                             */
/*---------------------------------------------------------------------------*/

TEST_F(OptionBytesTest, PendingChanges) {
    uint8_t write_data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    /* Initially no pending changes */
    bool has_pending = false;
    EXPECT_EQ(NX_OK,
              nx_option_bytes_native_has_pending_changes(0, &has_pending));
    EXPECT_FALSE(has_pending);

    /* Write data (creates pending changes) */
    EXPECT_EQ(NX_OK, opt_bytes->set_user_data(opt_bytes, write_data, 16));

    /* Should have pending changes */
    EXPECT_EQ(NX_OK,
              nx_option_bytes_native_has_pending_changes(0, &has_pending));
    EXPECT_TRUE(has_pending);

    /* Apply changes */
    EXPECT_EQ(NX_OK, opt_bytes->apply(opt_bytes));

    /* Should no longer have pending changes */
    EXPECT_EQ(NX_OK,
              nx_option_bytes_native_has_pending_changes(0, &has_pending));
    EXPECT_FALSE(has_pending);
}

TEST_F(OptionBytesTest, ApplyWithoutPendingChanges) {
    /* Apply without any pending changes should succeed */
    EXPECT_EQ(NX_OK, opt_bytes->apply(opt_bytes));
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirements 9.6, 10.2                                  */
/*---------------------------------------------------------------------------*/

TEST_F(OptionBytesTest, LifecycleInit) {
    /* Already initialized in SetUp */
    bool initialized = false;
    bool suspended = false;
    EXPECT_EQ(NX_OK,
              nx_option_bytes_native_get_state(0, &initialized, &suspended));
    EXPECT_TRUE(initialized);
    EXPECT_FALSE(suspended);
}

TEST_F(OptionBytesTest, LifecycleDeinit) {
    nx_lifecycle_t* lifecycle =
        (nx_lifecycle_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t));

    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Check state */
    bool initialized = false;
    EXPECT_EQ(NX_OK,
              nx_option_bytes_native_get_state(0, &initialized, nullptr));
    EXPECT_FALSE(initialized);
}

TEST_F(OptionBytesTest, LifecycleSuspendResume) {
    nx_lifecycle_t* lifecycle =
        (nx_lifecycle_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t));

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Check state */
    bool suspended = false;
    EXPECT_EQ(NX_OK, nx_option_bytes_native_get_state(0, nullptr, &suspended));
    EXPECT_TRUE(suspended);

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Check state */
    EXPECT_EQ(NX_OK, nx_option_bytes_native_get_state(0, nullptr, &suspended));
    EXPECT_FALSE(suspended);
}

TEST_F(OptionBytesTest, LifecycleGetState) {
    nx_lifecycle_t* lifecycle =
        (nx_lifecycle_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t));

    /* Should be running */
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Suspend */
    lifecycle->suspend(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Resume */
    lifecycle->resume(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Deinitialize */
    lifecycle->deinit(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 9.7, 10.3                           */
/*---------------------------------------------------------------------------*/

TEST_F(OptionBytesTest, PowerEnableDisable) {
    nx_power_t* power =
        (nx_power_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t) +
                      sizeof(nx_lifecycle_t));

    /* Initially disabled */
    EXPECT_FALSE(power->is_enabled(power));

    /* Enable */
    EXPECT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(power->is_enabled(power));

    /* Disable */
    EXPECT_EQ(NX_OK, power->disable(power));
    EXPECT_FALSE(power->is_enabled(power));
}

TEST_F(OptionBytesTest, PowerCallback) {
    nx_power_t* power =
        (nx_power_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t) +
                      sizeof(nx_lifecycle_t));

    /* Callback tracking */
    static bool callback_called = false;
    static bool callback_enabled = false;

    auto callback = [](void* user_data, bool enabled) {
        callback_called = true;
        callback_enabled = enabled;
    };

    /* Set callback */
    EXPECT_EQ(NX_OK, power->set_callback(power, callback, nullptr));

    /* Enable power - should trigger callback */
    callback_called = false;
    EXPECT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(callback_enabled);

    /* Disable power - should trigger callback */
    callback_called = false;
    EXPECT_EQ(NX_OK, power->disable(power));
    EXPECT_TRUE(callback_called);
    EXPECT_FALSE(callback_enabled);
}

/*---------------------------------------------------------------------------*/
/* Error Condition Tests - Requirements 10.6                                 */
/*---------------------------------------------------------------------------*/

TEST_F(OptionBytesTest, OperationsOnUninitializedDevice) {
    /* Deinitialize */
    nx_lifecycle_t* lifecycle =
        (nx_lifecycle_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t));
    lifecycle->deinit(lifecycle);

    /* Operations should fail */
    uint8_t data[16] = {0};
    EXPECT_EQ(NX_ERR_NOT_INIT, opt_bytes->get_user_data(opt_bytes, data, 16));
    EXPECT_EQ(NX_ERR_NOT_INIT, opt_bytes->set_user_data(opt_bytes, data, 16));
    EXPECT_EQ(NX_ERR_NOT_INIT, opt_bytes->set_read_protection(opt_bytes, 1));
    EXPECT_EQ(NX_ERR_NOT_INIT, opt_bytes->apply(opt_bytes));
}

TEST_F(OptionBytesTest, DoubleInitialize) {
    nx_lifecycle_t* lifecycle =
        (nx_lifecycle_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t));

    /* Already initialized, should fail */
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));
}

TEST_F(OptionBytesTest, DoubleSuspend) {
    nx_lifecycle_t* lifecycle =
        (nx_lifecycle_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t));

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Suspend again should fail */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->suspend(lifecycle));
}

TEST_F(OptionBytesTest, ResumeWithoutSuspend) {
    nx_lifecycle_t* lifecycle =
        (nx_lifecycle_t*)((uint8_t*)opt_bytes + sizeof(nx_option_bytes_t));

    /* Resume without suspend should fail */
    EXPECT_EQ(NX_ERR_INVALID_STATE, lifecycle->resume(lifecycle));
}
