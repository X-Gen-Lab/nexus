/**
 * \file            test_nx_hal.cpp
 * \brief           Unit tests for Nexus HAL initialization
 * \author          Nexus Team
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/nx_hal.h"
}

/**
 * \brief           Test fixture for HAL initialization tests
 */
class NxHalTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Ensure HAL is deinitialized before each test
        nx_hal_deinit();
    }

    void TearDown() override {
        // Clean up after each test
        nx_hal_deinit();
    }
};

/**
 * \brief           Test HAL initialization
 */
TEST_F(NxHalTest, Initialization) {
    // Initially not initialized
    EXPECT_FALSE(nx_hal_is_initialized());

    // Initialize HAL
    nx_status_t status = nx_hal_init();
    EXPECT_EQ(NX_OK, status);
    EXPECT_TRUE(nx_hal_is_initialized());

    // Calling init again should be idempotent
    status = nx_hal_init();
    EXPECT_EQ(NX_OK, status);
    EXPECT_TRUE(nx_hal_is_initialized());
}

/**
 * \brief           Test HAL deinitialization
 */
TEST_F(NxHalTest, Deinitialization) {
    // Initialize first
    nx_status_t status = nx_hal_init();
    EXPECT_EQ(NX_OK, status);
    EXPECT_TRUE(nx_hal_is_initialized());

    // Deinitialize
    status = nx_hal_deinit();
    EXPECT_EQ(NX_OK, status);
    EXPECT_FALSE(nx_hal_is_initialized());

    // Calling deinit again should be idempotent
    status = nx_hal_deinit();
    EXPECT_EQ(NX_OK, status);
    EXPECT_FALSE(nx_hal_is_initialized());
}

/**
 * \brief           Test HAL version string
 */
TEST_F(NxHalTest, Version) {
    const char* version = nx_hal_get_version();
    EXPECT_NE(nullptr, version);
    EXPECT_STREQ("1.0.0", version);
}

/**
 * \brief           Test HAL initialization lifecycle
 */
TEST_F(NxHalTest, InitDeinitCycle) {
    // Multiple init/deinit cycles
    for (int i = 0; i < 3; i++) {
        EXPECT_FALSE(nx_hal_is_initialized());

        nx_status_t status = nx_hal_init();
        EXPECT_EQ(NX_OK, status);
        EXPECT_TRUE(nx_hal_is_initialized());

        status = nx_hal_deinit();
        EXPECT_EQ(NX_OK, status);
        EXPECT_FALSE(nx_hal_is_initialized());
    }
}

/**
 * \brief           Test that main header includes all necessary headers
 */
TEST_F(NxHalTest, HeaderInclusion) {
    // This test verifies that nx_hal.h includes all necessary headers
    // by attempting to use types from each included header

    // Base types
    nx_status_t status = NX_OK;
    EXPECT_EQ(NX_OK, status);

    // Device base class
    const nx_device_t* dev = nx_device_find("nonexistent");
    EXPECT_EQ(nullptr, dev);

    // Factory functions are declared
    nx_uart_t* uart = nx_factory_uart(0);
    if (uart) {
        nx_factory_uart_release(uart);
    }

    // Resource managers
    nx_dma_manager_t* dma_mgr = nx_dma_manager_get();
    EXPECT_NE(nullptr, dma_mgr);

    nx_isr_manager_t* isr_mgr = nx_isr_manager_get();
    EXPECT_NE(nullptr, isr_mgr);
}
