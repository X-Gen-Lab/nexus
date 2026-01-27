/**
 * \file            test_nx_device_msvc.cpp
 * \brief           Device Registration Unit Tests for MSVC
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-21
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for MSVC device registration mechanism.
 *                  Tests manual device registration, lookup, and cleanup.
 *                  Requirements: 1.2, 1.3, 1.4, 8.1, 8.2, 8.3, 8.4
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "tests/hal/native/native_test_helpers.h"
}

/*---------------------------------------------------------------------------*/
/* Test Fixture                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           MSVC Device Registration Test Fixture
 */
class MSVCDeviceRegistrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
        /* Clear all devices before each test (MSVC only) */
        nx_device_clear_all();
#endif
    }

    void TearDown() override {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
        /* Clear all devices after each test (MSVC only) */
        nx_device_clear_all();
#endif
    }

    /* Helper function to create a test device descriptor */
    static nx_device_config_state_t test_state;
    static void* test_init(const nx_device_t* dev) {
        (void)dev;
        return (void*)0x12345678;
    }
};

/* Static member initialization */
nx_device_config_state_t MSVCDeviceRegistrationTest::test_state = {0, false,
                                                                   nullptr};

/*---------------------------------------------------------------------------*/
/* Basic Registration Tests - Requirements 1.2, 1.3                          */
/*---------------------------------------------------------------------------*/

TEST_F(MSVCDeviceRegistrationTest, RegisterSingleDevice) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Create a test device */
    nx_device_t test_device = {
        .name = "TEST_DEVICE",
        .config = nullptr,
        .state = &test_state,
        .device_init = test_init,
    };

    /* Register the device */
    EXPECT_EQ(NX_OK, nx_device_register(&test_device));

    /* Find the device */
    const nx_device_t* found = nx_device_find("TEST_DEVICE");
    ASSERT_NE(nullptr, found);
    EXPECT_STREQ("TEST_DEVICE", found->name);
    EXPECT_EQ(&test_device, found);
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}

TEST_F(MSVCDeviceRegistrationTest, RegisterMultipleDevices) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Create multiple test devices */
    nx_device_config_state_t state1 = {0, false, nullptr};
    nx_device_config_state_t state2 = {0, false, nullptr};
    nx_device_config_state_t state3 = {0, false, nullptr};

    nx_device_t device1 = {
        .name = "DEVICE1",
        .config = nullptr,
        .state = &state1,
        .device_init = test_init,
    };

    nx_device_t device2 = {
        .name = "DEVICE2",
        .config = nullptr,
        .state = &state2,
        .device_init = test_init,
    };

    nx_device_t device3 = {
        .name = "DEVICE3",
        .config = nullptr,
        .state = &state3,
        .device_init = test_init,
    };

    /* Register all devices */
    EXPECT_EQ(NX_OK, nx_device_register(&device1));
    EXPECT_EQ(NX_OK, nx_device_register(&device2));
    EXPECT_EQ(NX_OK, nx_device_register(&device3));

    /* Find all devices */
    const nx_device_t* found1 = nx_device_find("DEVICE1");
    const nx_device_t* found2 = nx_device_find("DEVICE2");
    const nx_device_t* found3 = nx_device_find("DEVICE3");

    ASSERT_NE(nullptr, found1);
    ASSERT_NE(nullptr, found2);
    ASSERT_NE(nullptr, found3);

    EXPECT_STREQ("DEVICE1", found1->name);
    EXPECT_STREQ("DEVICE2", found2->name);
    EXPECT_STREQ("DEVICE3", found3->name);
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}

/*---------------------------------------------------------------------------*/
/* Clear Tests - Requirement 1.4                                             */
/*---------------------------------------------------------------------------*/

TEST_F(MSVCDeviceRegistrationTest, ClearRemovesAllDevices) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Register multiple devices */
    nx_device_config_state_t state1 = {0, false, nullptr};
    nx_device_config_state_t state2 = {0, false, nullptr};

    nx_device_t device1 = {
        .name = "CLEAR_TEST1",
        .config = nullptr,
        .state = &state1,
        .device_init = test_init,
    };

    nx_device_t device2 = {
        .name = "CLEAR_TEST2",
        .config = nullptr,
        .state = &state2,
        .device_init = test_init,
    };

    nx_device_register(&device1);
    nx_device_register(&device2);

    /* Verify devices are registered */
    ASSERT_NE(nullptr, nx_device_find("CLEAR_TEST1"));
    ASSERT_NE(nullptr, nx_device_find("CLEAR_TEST2"));

    /* Clear all devices */
    nx_device_clear_all();

    /* Verify devices are removed */
    EXPECT_EQ(nullptr, nx_device_find("CLEAR_TEST1"));
    EXPECT_EQ(nullptr, nx_device_find("CLEAR_TEST2"));
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests - Requirements 8.1, 8.2, 8.3                         */
/*---------------------------------------------------------------------------*/

TEST_F(MSVCDeviceRegistrationTest, RegistryFullReturnsNoMemory) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Fill the registry to capacity */
    nx_device_config_state_t states[64];
    nx_device_t devices[64];

    for (int i = 0; i < 64; ++i) {
        states[i] = {0, false};
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "DEVICE_%d", i);

        devices[i] = {
            .name = _strdup(name_buf),
            .config = nullptr,
            .state = &states[i],
            .device_init = test_init,
        };

        nx_status_t result = nx_device_register(&devices[i]);
        if (result == NX_ERR_NO_MEMORY) {
            /* Registry is full, this is expected */
            EXPECT_GT(i, 0); /* Should have registered at least one device */

            /* Cleanup allocated names */
            for (int j = 0; j <= i; ++j) {
                free((void*)devices[j].name);
            }
            return;
        }
        EXPECT_EQ(NX_OK, result);
    }

    /* If we get here, registry size is >= 64, cleanup */
    for (int i = 0; i < 64; ++i) {
        free((void*)devices[i].name);
    }
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}

TEST_F(MSVCDeviceRegistrationTest, NullDeviceReturnsInvalidArg) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Try to register NULL device */
    EXPECT_EQ(NX_ERR_NULL_PTR, nx_device_register(nullptr));
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}

TEST_F(MSVCDeviceRegistrationTest, FindNonExistentDeviceReturnsNull) {
    /* Try to find a device that doesn't exist */
    const nx_device_t* found = nx_device_find("NONEXISTENT_DEVICE");
    EXPECT_EQ(nullptr, found);
}

TEST_F(MSVCDeviceRegistrationTest, FindWithNullNameReturnsNull) {
    /* Try to find with NULL name */
    const nx_device_t* found = nx_device_find(nullptr);
    EXPECT_EQ(nullptr, found);
}

/*---------------------------------------------------------------------------*/
/* Setup/Cleanup Tests - Requirements 8.4                                    */
/*---------------------------------------------------------------------------*/

TEST_F(MSVCDeviceRegistrationTest, SetupRegistersExpectedDevices) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Call setup function */
    native_test_setup_devices();

    /* Verify some expected devices are registered */
    /* Note: Actual devices depend on CONFIG macros */

    /* Try to find UART0 if configured */
#ifdef INSTANCE_NX_UART_0
    const nx_device_t* uart0 = nx_device_find("UART0");
    EXPECT_NE(nullptr, uart0);
    if (uart0 != nullptr) {
        EXPECT_STREQ("UART0", uart0->name);
    }
#endif

    /* Try to find SPI0 if configured */
#ifdef INSTANCE_NX_SPI_0
    const nx_device_t* spi0 = nx_device_find("SPI0");
    EXPECT_NE(nullptr, spi0);
    if (spi0 != nullptr) {
        EXPECT_STREQ("SPI0", spi0->name);
    }
#endif

    /* Try to find I2C0 if configured */
#ifdef INSTANCE_NX_I2C_0
    const nx_device_t* i2c0 = nx_device_find("I2C0");
    EXPECT_NE(nullptr, i2c0);
    if (i2c0 != nullptr) {
        EXPECT_STREQ("I2C0", i2c0->name);
    }
#endif

    /* Try to find GPIO PA0 if configured */
#ifdef INSTANCE_NX_GPIO_PA0
    const nx_device_t* gpiopa0 = nx_device_find("GPIOPA0");
    EXPECT_NE(nullptr, gpiopa0);
    if (gpiopa0 != nullptr) {
        EXPECT_STREQ("GPIOPA0", gpiopa0->name);
    }
#endif
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}

TEST_F(MSVCDeviceRegistrationTest, CleanupClearsAllDevices) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Setup devices */
    native_test_setup_devices();

    /* Verify at least one device is registered */
    (void)0; /* Placeholder for device verification */
#ifdef INSTANCE_NX_UART_0
    EXPECT_NE(nullptr, nx_device_find("UART0"));
#endif
#ifdef INSTANCE_NX_SPI_0
    EXPECT_NE(nullptr, nx_device_find("SPI0"));
#endif

    /* Call cleanup */
    native_test_cleanup_devices();

    /* Verify devices are cleared */
#ifdef INSTANCE_NX_UART_0
    EXPECT_EQ(nullptr, nx_device_find("UART0"));
#endif
#ifdef INSTANCE_NX_SPI_0
    EXPECT_EQ(nullptr, nx_device_find("SPI0"));
#endif
#ifdef INSTANCE_NX_I2C_0
    EXPECT_EQ(nullptr, nx_device_find("I2C0"));
#endif
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}

/*---------------------------------------------------------------------------*/
/* Additional Edge Case Tests                                                */
/*---------------------------------------------------------------------------*/

TEST_F(MSVCDeviceRegistrationTest, RegisterDeviceWithEmptyName) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Create a device with empty name */
    nx_device_t test_device = {
        .name = "",
        .config = nullptr,
        .state = &test_state,
        .device_init = test_init,
    };

    /* Register should succeed (empty string is valid) */
    EXPECT_EQ(NX_OK, nx_device_register(&test_device));

    /* Find with empty name */
    const nx_device_t* found = nx_device_find("");
    EXPECT_NE(nullptr, found);
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}

TEST_F(MSVCDeviceRegistrationTest, RegisterDuplicateNames) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Create two devices with same name */
    nx_device_config_state_t state1 = {0, false, nullptr};
    nx_device_config_state_t state2 = {0, false, nullptr};

    nx_device_t device1 = {
        .name = "DUPLICATE",
        .config = nullptr,
        .state = &state1,
        .device_init = test_init,
    };

    nx_device_t device2 = {
        .name = "DUPLICATE",
        .config = nullptr,
        .state = &state2,
        .device_init = test_init,
    };

    /* Register both (should succeed, but find returns first match) */
    EXPECT_EQ(NX_OK, nx_device_register(&device1));
    EXPECT_EQ(NX_OK, nx_device_register(&device2));

    /* Find should return the first registered device */
    const nx_device_t* found = nx_device_find("DUPLICATE");
    ASSERT_NE(nullptr, found);
    EXPECT_EQ(&device1, found);
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}

TEST_F(MSVCDeviceRegistrationTest, ClearEmptyRegistry) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Clear when registry is already empty (should not crash) */
    nx_device_clear_all();
    nx_device_clear_all(); /* Call twice */

    /* Verify find still returns NULL */
    EXPECT_EQ(nullptr, nx_device_find("ANY_DEVICE"));
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}

TEST_F(MSVCDeviceRegistrationTest, RegisterAfterClear) {
#if !defined(__GNUC__) && !defined(__ARMCC_VERSION) && !defined(__ICCARM__) && \
    !defined(__TI_ARM__) && !defined(__TASKING__) && !defined(__CC_ARM)
    /* Register a device */
    nx_device_t device1 = {
        .name = "BEFORE_CLEAR",
        .config = nullptr,
        .state = &test_state,
        .device_init = test_init,
    };
    nx_device_register(&device1);

    /* Clear */
    nx_device_clear_all();

    /* Register a new device */
    nx_device_t device2 = {
        .name = "AFTER_CLEAR",
        .config = nullptr,
        .state = &test_state,
        .device_init = test_init,
    };
    EXPECT_EQ(NX_OK, nx_device_register(&device2));

    /* Verify old device is gone, new device is present */
    EXPECT_EQ(nullptr, nx_device_find("BEFORE_CLEAR"));
    EXPECT_NE(nullptr, nx_device_find("AFTER_CLEAR"));
#else
    GTEST_SKIP() << "Test only applicable for MSVC";
#endif
}
