/**
 * \file            test_nx_device.cpp
 * \brief           Tests for nx_device.h device management infrastructure
 * \author          Nexus Team
 *
 * Unit tests for the device base class including:
 * - nx_device_get() / nx_device_put() reference counting
 * - nx_device_find() device lookup
 * - nx_device_register() / nx_device_unregister()
 * - nx_device_reinit() reinitialization
 *
 * **Validates: Requirements 2.1, 2.2, 2.6, 3.1, 3.2, 3.4**
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
}

/*===========================================================================*/
/* Mock Device Implementation                                                 */
/*===========================================================================*/

/**
 * \brief           Mock device configuration structure
 */
typedef struct mock_config_s {
    uint32_t param1;
    uint32_t param2;
} mock_config_t;

/**
 * \brief           Mock device interface structure
 */
typedef struct mock_interface_s {
    int value;
    bool initialized;
} mock_interface_t;

// Static storage for mock device
static mock_config_t g_mock_default_config = {.param1 = 100, .param2 = 200};
static mock_config_t g_mock_runtime_config;
static mock_interface_t g_mock_interface;
static int g_init_count = 0;
static int g_deinit_count = 0;
static int g_suspend_count = 0;
static int g_resume_count = 0;

/**
 * \brief           Mock device init function
 */
static void* mock_device_init(const nx_device_t* dev) {
    (void)dev;
    g_init_count++;
    g_mock_interface.value = 42;
    g_mock_interface.initialized = true;
    return &g_mock_interface;
}

/**
 * \brief           Mock device deinit function
 */
static nx_status_t mock_device_deinit(const nx_device_t* dev) {
    (void)dev;
    g_deinit_count++;
    g_mock_interface.initialized = false;
    return NX_OK;
}

/**
 * \brief           Mock device suspend function
 */
static nx_status_t mock_device_suspend(const nx_device_t* dev) {
    (void)dev;
    g_suspend_count++;
    return NX_OK;
}

/**
 * \brief           Mock device resume function
 */
static nx_status_t mock_device_resume(const nx_device_t* dev) {
    (void)dev;
    g_resume_count++;
    return NX_OK;
}

/**
 * \brief           Mock device that fails initialization
 */
static void* mock_device_init_fail(const nx_device_t* dev) {
    (void)dev;
    return NULL;  // Simulate init failure
}

/**
 * \brief           Test fixture for nx_device tests
 */
class NxDeviceTest : public ::testing::Test {
  protected:
    nx_device_t test_device;
    nx_device_t test_device2;

    void SetUp() override {
        // Reset counters
        g_init_count = 0;
        g_deinit_count = 0;
        g_suspend_count = 0;
        g_resume_count = 0;

        // Reset mock interface
        g_mock_interface.value = 0;
        g_mock_interface.initialized = false;

        // Reset runtime config
        memset(&g_mock_runtime_config, 0, sizeof(g_mock_runtime_config));

        // Initialize test device
        memset(&test_device, 0, sizeof(test_device));
        test_device.name = "test_device";
        test_device.default_config = &g_mock_default_config;
        test_device.runtime_config = &g_mock_runtime_config;
        test_device.config_size = sizeof(mock_config_t);
        test_device.device_init = mock_device_init;
        test_device.device_deinit = mock_device_deinit;
        test_device.device_suspend = mock_device_suspend;
        test_device.device_resume = mock_device_resume;
        test_device.state.initialized = 0;
        test_device.state.state = NX_DEV_STATE_UNINITIALIZED;
        test_device.state.ref_count = 0;
        test_device.priv = NULL;

        // Initialize second test device
        memset(&test_device2, 0, sizeof(test_device2));
        test_device2.name = "test_device2";
        test_device2.default_config = &g_mock_default_config;
        test_device2.runtime_config = &g_mock_runtime_config;
        test_device2.config_size = sizeof(mock_config_t);
        test_device2.device_init = mock_device_init;
        test_device2.device_deinit = mock_device_deinit;
        test_device2.state.initialized = 0;
        test_device2.state.state = NX_DEV_STATE_UNINITIALIZED;
        test_device2.state.ref_count = 0;
        test_device2.priv = NULL;
    }

    void TearDown() override {
        // Unregister devices if registered
        nx_device_unregister(&test_device);
        nx_device_unregister(&test_device2);
    }
};

/*===========================================================================*/
/* Device Registration Tests                                                  */
/*===========================================================================*/

/**
 * \brief           Test device registration
 */
TEST_F(NxDeviceTest, Register_Success) {
    nx_status_t status = nx_device_register(&test_device);
    EXPECT_EQ(status, NX_OK);

    // Verify device can be found
    const nx_device_t* found = nx_device_find("test_device");
    EXPECT_EQ(found, &test_device);
}

/**
 * \brief           Test registration with NULL device
 */
TEST_F(NxDeviceTest, Register_NullDevice) {
    nx_status_t status = nx_device_register(nullptr);
    EXPECT_EQ(status, NX_ERR_NULL_PTR);
}

/**
 * \brief           Test registration with NULL name
 */
TEST_F(NxDeviceTest, Register_NullName) {
    test_device.name = nullptr;
    nx_status_t status = nx_device_register(&test_device);
    EXPECT_EQ(status, NX_ERR_NULL_PTR);
}

/**
 * \brief           Test duplicate registration
 */
TEST_F(NxDeviceTest, Register_Duplicate) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    // Try to register again
    nx_status_t status = nx_device_register(&test_device);
    EXPECT_EQ(status, NX_ERR_ALREADY_INIT);
}

/**
 * \brief           Test device unregistration
 */
TEST_F(NxDeviceTest, Unregister_Success) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    nx_status_t status = nx_device_unregister(&test_device);
    EXPECT_EQ(status, NX_OK);

    // Verify device can no longer be found
    const nx_device_t* found = nx_device_find("test_device");
    EXPECT_EQ(found, nullptr);
}

/**
 * \brief           Test unregistration with NULL device
 */
TEST_F(NxDeviceTest, Unregister_NullDevice) {
    nx_status_t status = nx_device_unregister(nullptr);
    EXPECT_EQ(status, NX_ERR_NULL_PTR);
}

/**
 * \brief           Test unregistration of device with references
 */
TEST_F(NxDeviceTest, Unregister_WithReferences) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    // Get device (increments ref count)
    void* intf = nx_device_get("test_device");
    ASSERT_NE(intf, nullptr);

    // Try to unregister (should fail due to references)
    nx_status_t status = nx_device_unregister(&test_device);
    EXPECT_EQ(status, NX_ERR_BUSY);

    // Release device
    ASSERT_EQ(nx_device_put(intf), NX_OK);

    // Now unregister should succeed
    status = nx_device_unregister(&test_device);
    EXPECT_EQ(status, NX_OK);
}

/*===========================================================================*/
/* Device Find Tests                                                          */
/*===========================================================================*/

/**
 * \brief           Test nx_device_find() with valid name
 */
TEST_F(NxDeviceTest, Find_Success) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    const nx_device_t* found = nx_device_find("test_device");
    EXPECT_EQ(found, &test_device);
}

/**
 * \brief           Test nx_device_find() with NULL name
 */
TEST_F(NxDeviceTest, Find_NullName) {
    const nx_device_t* found = nx_device_find(nullptr);
    EXPECT_EQ(found, nullptr);
}

/**
 * \brief           Test nx_device_find() with non-existent device
 */
TEST_F(NxDeviceTest, Find_NotFound) {
    const nx_device_t* found = nx_device_find("non_existent_device");
    EXPECT_EQ(found, nullptr);
}

/*===========================================================================*/
/* Reference Counting Tests                                                   */
/*===========================================================================*/

/**
 * \brief           Test nx_device_get() initializes device and increments ref
 * count
 */
TEST_F(NxDeviceTest, Get_InitializesDevice) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    // Get device
    void* intf = nx_device_get("test_device");
    ASSERT_NE(intf, nullptr);

    // Verify device was initialized
    EXPECT_EQ(g_init_count, 1);
    EXPECT_TRUE(nx_device_is_initialized(&test_device));
    EXPECT_EQ(nx_device_get_ref_count(&test_device), 1);
    EXPECT_EQ(nx_device_get_state(&test_device), NX_DEV_STATE_RUNNING);

    // Verify interface
    mock_interface_t* mock_intf = static_cast<mock_interface_t*>(intf);
    EXPECT_EQ(mock_intf->value, 42);
    EXPECT_TRUE(mock_intf->initialized);

    // Cleanup
    ASSERT_EQ(nx_device_put(intf), NX_OK);
}

/**
 * \brief           Test nx_device_get() returns same interface for multiple
 * calls
 */
TEST_F(NxDeviceTest, Get_ReturnsSameInterface) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    // Get device twice
    void* intf1 = nx_device_get("test_device");
    void* intf2 = nx_device_get("test_device");

    ASSERT_NE(intf1, nullptr);
    ASSERT_NE(intf2, nullptr);

    // Should return same interface
    EXPECT_EQ(intf1, intf2);

    // Init should only be called once
    EXPECT_EQ(g_init_count, 1);

    // Ref count should be 2
    EXPECT_EQ(nx_device_get_ref_count(&test_device), 2);

    // Cleanup
    ASSERT_EQ(nx_device_put(intf1), NX_OK);
    ASSERT_EQ(nx_device_put(intf2), NX_OK);
}

/**
 * \brief           Test nx_device_get() with non-existent device
 */
TEST_F(NxDeviceTest, Get_NotFound) {
    void* intf = nx_device_get("non_existent_device");
    EXPECT_EQ(intf, nullptr);
}

/**
 * \brief           Test nx_device_put() decrements ref count
 */
TEST_F(NxDeviceTest, Put_DecrementsRefCount) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    // Get device twice
    void* intf1 = nx_device_get("test_device");
    void* intf2 = nx_device_get("test_device");
    ASSERT_NE(intf1, nullptr);
    EXPECT_EQ(nx_device_get_ref_count(&test_device), 2);

    // Put once
    ASSERT_EQ(nx_device_put(intf1), NX_OK);
    EXPECT_EQ(nx_device_get_ref_count(&test_device), 1);

    // Device should still be initialized
    EXPECT_TRUE(nx_device_is_initialized(&test_device));
    EXPECT_EQ(g_deinit_count, 0);

    // Put again (ref count reaches 0)
    ASSERT_EQ(nx_device_put(intf2), NX_OK);
    EXPECT_EQ(nx_device_get_ref_count(&test_device), 0);

    // Device should be deinitialized
    EXPECT_FALSE(nx_device_is_initialized(&test_device));
    EXPECT_EQ(g_deinit_count, 1);
}

/**
 * \brief           Test nx_device_put() with NULL pointer
 */
TEST_F(NxDeviceTest, Put_NullPointer) {
    nx_status_t status = nx_device_put(nullptr);
    EXPECT_EQ(status, NX_ERR_NULL_PTR);
}

/**
 * \brief           Test nx_device_put() with invalid interface
 */
TEST_F(NxDeviceTest, Put_InvalidInterface) {
    int dummy = 0;
    nx_status_t status = nx_device_put(&dummy);
    EXPECT_EQ(status, NX_ERR_NOT_FOUND);
}

/*===========================================================================*/
/* Device Reinitialization Tests                                              */
/*===========================================================================*/

/**
 * \brief           Test nx_device_reinit() with new configuration
 */
TEST_F(NxDeviceTest, Reinit_WithNewConfig) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    // Get device first
    void* intf = nx_device_get("test_device");
    ASSERT_NE(intf, nullptr);
    EXPECT_EQ(g_init_count, 1);

    // Verify default config was copied
    EXPECT_EQ(g_mock_runtime_config.param1, 100);
    EXPECT_EQ(g_mock_runtime_config.param2, 200);

    // Create new config
    mock_config_t new_config = {.param1 = 300, .param2 = 400};

    // Reinitialize with new config
    nx_status_t status = nx_device_reinit(&test_device, &new_config);
    EXPECT_EQ(status, NX_OK);

    // Verify deinit and init were called
    EXPECT_EQ(g_deinit_count, 1);
    EXPECT_EQ(g_init_count, 2);

    // Verify new config was applied
    EXPECT_EQ(g_mock_runtime_config.param1, 300);
    EXPECT_EQ(g_mock_runtime_config.param2, 400);

    // Verify ref count is preserved
    EXPECT_EQ(nx_device_get_ref_count(&test_device), 1);

    // Cleanup
    ASSERT_EQ(nx_device_put(test_device.priv), NX_OK);
}

/**
 * \brief           Test nx_device_reinit() with NULL config (uses default)
 */
TEST_F(NxDeviceTest, Reinit_WithNullConfig) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    // Get device and modify runtime config
    void* intf = nx_device_get("test_device");
    ASSERT_NE(intf, nullptr);
    g_mock_runtime_config.param1 = 999;

    // Reinitialize with NULL config (should use default)
    nx_status_t status = nx_device_reinit(&test_device, nullptr);
    EXPECT_EQ(status, NX_OK);

    // Verify default config was restored
    EXPECT_EQ(g_mock_runtime_config.param1, 100);
    EXPECT_EQ(g_mock_runtime_config.param2, 200);

    // Cleanup
    ASSERT_EQ(nx_device_put(test_device.priv), NX_OK);
}

/**
 * \brief           Test nx_device_reinit() with NULL device
 */
TEST_F(NxDeviceTest, Reinit_NullDevice) {
    nx_status_t status = nx_device_reinit(nullptr, nullptr);
    EXPECT_EQ(status, NX_ERR_NULL_PTR);
}

/*===========================================================================*/
/* Device State Query Tests                                                   */
/*===========================================================================*/

/**
 * \brief           Test nx_device_get_ref_count() with NULL device
 */
TEST_F(NxDeviceTest, GetRefCount_NullDevice) {
    uint8_t count = nx_device_get_ref_count(nullptr);
    EXPECT_EQ(count, 0);
}

/**
 * \brief           Test nx_device_get_state() with NULL device
 */
TEST_F(NxDeviceTest, GetState_NullDevice) {
    nx_device_state_t state = nx_device_get_state(nullptr);
    EXPECT_EQ(state, NX_DEV_STATE_UNINITIALIZED);
}

/**
 * \brief           Test nx_device_is_initialized() with NULL device
 */
TEST_F(NxDeviceTest, IsInitialized_NullDevice) {
    bool initialized = nx_device_is_initialized(nullptr);
    EXPECT_FALSE(initialized);
}

/**
 * \brief           Test device state transitions
 */
TEST_F(NxDeviceTest, StateTransitions) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    // Initial state
    EXPECT_EQ(nx_device_get_state(&test_device), NX_DEV_STATE_UNINITIALIZED);
    EXPECT_FALSE(nx_device_is_initialized(&test_device));

    // After get (init)
    void* intf = nx_device_get("test_device");
    ASSERT_NE(intf, nullptr);
    EXPECT_EQ(nx_device_get_state(&test_device), NX_DEV_STATE_RUNNING);
    EXPECT_TRUE(nx_device_is_initialized(&test_device));

    // After put (deinit when ref count reaches 0)
    ASSERT_EQ(nx_device_put(intf), NX_OK);
    EXPECT_EQ(nx_device_get_state(&test_device), NX_DEV_STATE_UNINITIALIZED);
    EXPECT_FALSE(nx_device_is_initialized(&test_device));
}

/*===========================================================================*/
/* Default Config Copy Tests                                                  */
/*===========================================================================*/

/**
 * \brief           Test that default config is copied to runtime config on init
 */
TEST_F(NxDeviceTest, DefaultConfigCopy) {
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    // Runtime config should be zero initially
    EXPECT_EQ(g_mock_runtime_config.param1, 0);
    EXPECT_EQ(g_mock_runtime_config.param2, 0);

    // Get device (triggers init)
    void* intf = nx_device_get("test_device");
    ASSERT_NE(intf, nullptr);

    // Runtime config should now have default values
    EXPECT_EQ(g_mock_runtime_config.param1, 100);
    EXPECT_EQ(g_mock_runtime_config.param2, 200);

    // Cleanup
    ASSERT_EQ(nx_device_put(intf), NX_OK);
}

/*===========================================================================*/
/* Init Failure Tests                                                         */
/*===========================================================================*/

/**
 * \brief           Test nx_device_get() when init fails
 */
TEST_F(NxDeviceTest, Get_InitFails) {
    // Use failing init function
    test_device.device_init = mock_device_init_fail;
    ASSERT_EQ(nx_device_register(&test_device), NX_OK);

    // Get should fail
    void* intf = nx_device_get("test_device");
    EXPECT_EQ(intf, nullptr);

    // Device should not be initialized
    EXPECT_FALSE(nx_device_is_initialized(&test_device));
    EXPECT_EQ(nx_device_get_ref_count(&test_device), 0);
}
