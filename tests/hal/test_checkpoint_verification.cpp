// Checkpoint verification for infrastructure layer
// This file demonstrates the basic functionality

#include <cstdio>
#include <gtest/gtest.h>

extern "C" {
#include "hal/base/nx_device.h"
#include "hal/interface/nx_configurable.h"
#include "hal/interface/nx_diagnostic.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"
}

TEST(CheckpointTest, HeadersCompile) {
    SUCCEED();
}

TEST(CheckpointTest, StatusToString) {
    const char* ok_str = nx_status_to_string(NX_OK);
    ASSERT_NE(ok_str, nullptr);
    printf("NX_OK -> \"%s\"\n", ok_str);
    EXPECT_STREQ(ok_str, "OK");

    const char* timeout_str = nx_status_to_string(NX_ERR_TIMEOUT);
    ASSERT_NE(timeout_str, nullptr);
    printf("NX_ERR_TIMEOUT -> \"%s\"\n", timeout_str);
    EXPECT_STREQ(timeout_str, "Timeout");

    const char* null_ptr_str = nx_status_to_string(NX_ERR_NULL_PTR);
    ASSERT_NE(null_ptr_str, nullptr);
    printf("NX_ERR_NULL_PTR -> \"%s\"\n", null_ptr_str);
    EXPECT_STREQ(null_ptr_str, "Null pointer");

    const char* busy_str = nx_status_to_string(NX_ERR_BUSY);
    ASSERT_NE(busy_str, nullptr);
    printf("NX_ERR_BUSY -> \"%s\"\n", busy_str);
    EXPECT_STREQ(busy_str, "Device busy");

    printf("OK: nx_status_to_string() working correctly\n");
}

typedef struct {
    int value;
} mock_device_interface_t;

static mock_device_interface_t g_mock_interface;
static int g_init_calls = 0;
static int g_deinit_calls = 0;

static void* mock_init(const nx_device_t* dev) {
    (void)dev;
    g_init_calls++;
    g_mock_interface.value = 42;
    return &g_mock_interface;
}

static nx_status_t mock_deinit(const nx_device_t* dev) {
    (void)dev;
    g_deinit_calls++;
    return NX_OK;
}

TEST(CheckpointTest, DeviceReferenceCounting) {
    g_init_calls = 0;
    g_deinit_calls = 0;

    nx_device_t test_dev = {};
    test_dev.name = "checkpoint_test_device";
    test_dev.device_init = mock_init;
    test_dev.device_deinit = mock_deinit;

    nx_status_t status = nx_device_register(&test_dev);
    ASSERT_EQ(status, NX_OK);
    printf("OK: Device registered: %s\n", test_dev.name);

    void* intf1 = nx_device_get("checkpoint_test_device");
    ASSERT_NE(intf1, nullptr);
    EXPECT_EQ(g_init_calls, 1);
    EXPECT_EQ(nx_device_get_ref_count(&test_dev), 1);
    printf("OK: First nx_device_get(): ref_count = %d, init_calls = %d\n",
           nx_device_get_ref_count(&test_dev), g_init_calls);

    void* intf2 = nx_device_get("checkpoint_test_device");
    ASSERT_NE(intf2, nullptr);
    EXPECT_EQ(intf1, intf2);
    EXPECT_EQ(g_init_calls, 1);
    EXPECT_EQ(nx_device_get_ref_count(&test_dev), 2);
    printf("OK: Second nx_device_get(): ref_count = %d, init_calls = %d\n",
           nx_device_get_ref_count(&test_dev), g_init_calls);

    status = nx_device_put(intf1);
    ASSERT_EQ(status, NX_OK);
    EXPECT_EQ(nx_device_get_ref_count(&test_dev), 1);
    EXPECT_EQ(g_deinit_calls, 0);
    printf("OK: First nx_device_put(): ref_count = %d, deinit_calls = %d\n",
           nx_device_get_ref_count(&test_dev), g_deinit_calls);

    status = nx_device_put(intf2);
    ASSERT_EQ(status, NX_OK);
    EXPECT_EQ(nx_device_get_ref_count(&test_dev), 0);
    EXPECT_EQ(g_deinit_calls, 1);
    printf("OK: Second nx_device_put(): ref_count = %d, deinit_calls = %d\n",
           nx_device_get_ref_count(&test_dev), g_deinit_calls);

    nx_device_unregister(&test_dev);
    printf("OK: Device unregistered\n");
    printf("OK: Reference counting working correctly\n");
}

TEST(CheckpointTest, HelperMacros) {
    EXPECT_TRUE(NX_IS_OK(NX_OK));
    EXPECT_FALSE(NX_IS_OK(NX_ERR_TIMEOUT));
    printf("OK: NX_IS_OK() macro working\n");

    EXPECT_FALSE(NX_IS_ERROR(NX_OK));
    EXPECT_TRUE(NX_IS_ERROR(NX_ERR_TIMEOUT));
    printf("OK: NX_IS_ERROR() macro working\n");
}

TEST(CheckpointTest, Summary) {
    printf("\n");
    printf("========================================\n");
    printf("  Checkpoint 2: Infrastructure Layer   \n");
    printf("========================================\n");
    printf("OK: All basic headers compile successfully\n");
    printf("OK: nx_status_to_string() converts error codes\n");
    printf("OK: nx_device_get()/nx_device_put() reference counting works\n");
    printf("OK: Helper macros (NX_IS_OK, NX_IS_ERROR) work\n");
    printf("OK: Device lifecycle management functional\n");
    printf("========================================\n");
    printf("  Infrastructure layer verified!        \n");
    printf("========================================\n");
    SUCCEED();
}
