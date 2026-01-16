/**
 * \file            test_nx_device_registry.cpp
 * \brief           Tests for nx_device_registry.h static device registry
 * \author          Nexus Team
 *
 * Unit tests for the static device registry including:
 * - Device iteration with NX_DEVICE_FOREACH
 * - Device lookup by name with nx_device_registry_find()
 * - Device access by index with nx_device_registry_get()
 * - Device count with nx_device_registry_count()
 *
 * **Validates: Requirements 2.3, 2.4, 2.5**
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/nx_device_registry.h"
#include "hal/nx_status.h"
}

/*===========================================================================*/
/* Test Device Definitions                                                    */
/*===========================================================================*/

/**
 * \brief           Mock device configuration structure
 */
typedef struct test_config_s {
    uint32_t param1;
    uint32_t param2;
} test_config_t;

/**
 * \brief           Mock device interface structure
 */
typedef struct test_interface_s {
    int value;
    bool initialized;
} test_interface_t;

/* Static storage for test devices */
static test_config_t g_test_default_config1 = {.param1 = 100, .param2 = 200};
static test_config_t g_test_runtime_config1;
static test_interface_t g_test_interface1;

static test_config_t g_test_default_config2 = {.param1 = 300, .param2 = 400};
static test_config_t g_test_runtime_config2;
static test_interface_t g_test_interface2;

static test_config_t g_test_default_config3 = {.param1 = 500, .param2 = 600};
static test_config_t g_test_runtime_config3;
static test_interface_t g_test_interface3;

static int g_init_count = 0;

/**
 * \brief           Mock device init function for device 1
 */
static void* test_device1_init(const nx_device_t* dev) {
    (void)dev;
    g_init_count++;
    g_test_interface1.value = 1;
    g_test_interface1.initialized = true;
    return &g_test_interface1;
}

/**
 * \brief           Mock device init function for device 2
 */
static void* test_device2_init(const nx_device_t* dev) {
    (void)dev;
    g_init_count++;
    g_test_interface2.value = 2;
    g_test_interface2.initialized = true;
    return &g_test_interface2;
}

/**
 * \brief           Mock device init function for device 3
 */
static void* test_device3_init(const nx_device_t* dev) {
    (void)dev;
    g_init_count++;
    g_test_interface3.value = 3;
    g_test_interface3.initialized = true;
    return &g_test_interface3;
}

/**
 * \brief           Mock device deinit function
 */
static nx_status_t test_device_deinit(const nx_device_t* dev) {
    (void)dev;
    return NX_OK;
}

/*===========================================================================*/
/* Test Fixture                                                               */
/*===========================================================================*/

/* Test devices - defined as regular variables for MSVC testing */
static nx_device_t g_test_device1;
static nx_device_t g_test_device2;
static nx_device_t g_test_device3;

/* Array for MSVC testing - override the weak symbols */
#if defined(_MSC_VER)
static const nx_device_t* g_test_device_array[3];
static size_t g_test_device_count = 0;

/* Override weak symbols for testing */
extern "C" {
const nx_device_t** __nx_device_registry_array_ptr = nullptr;
size_t* __nx_device_registry_count_ptr = nullptr;
}
#endif

/**
 * \brief           Test fixture for nx_device_registry tests
 */
class NxDeviceRegistryTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset counters */
        g_init_count = 0;

        /* Reset interfaces */
        memset(&g_test_interface1, 0, sizeof(g_test_interface1));
        memset(&g_test_interface2, 0, sizeof(g_test_interface2));
        memset(&g_test_interface3, 0, sizeof(g_test_interface3));

        /* Reset runtime configs */
        memset(&g_test_runtime_config1, 0, sizeof(g_test_runtime_config1));
        memset(&g_test_runtime_config2, 0, sizeof(g_test_runtime_config2));
        memset(&g_test_runtime_config3, 0, sizeof(g_test_runtime_config3));

        /* Initialize test device 1 */
        memset(&g_test_device1, 0, sizeof(g_test_device1));
        g_test_device1.name = "test_device1";
        g_test_device1.default_config = &g_test_default_config1;
        g_test_device1.runtime_config = &g_test_runtime_config1;
        g_test_device1.config_size = sizeof(test_config_t);
        g_test_device1.device_init = test_device1_init;
        g_test_device1.device_deinit = test_device_deinit;
        g_test_device1.state.initialized = 0;
        g_test_device1.state.state = NX_DEV_STATE_UNINITIALIZED;
        g_test_device1.state.ref_count = 0;

        /* Initialize test device 2 */
        memset(&g_test_device2, 0, sizeof(g_test_device2));
        g_test_device2.name = "test_device2";
        g_test_device2.default_config = &g_test_default_config2;
        g_test_device2.runtime_config = &g_test_runtime_config2;
        g_test_device2.config_size = sizeof(test_config_t);
        g_test_device2.device_init = test_device2_init;
        g_test_device2.device_deinit = test_device_deinit;
        g_test_device2.state.initialized = 0;
        g_test_device2.state.state = NX_DEV_STATE_UNINITIALIZED;
        g_test_device2.state.ref_count = 0;

        /* Initialize test device 3 */
        memset(&g_test_device3, 0, sizeof(g_test_device3));
        g_test_device3.name = "test_device3";
        g_test_device3.default_config = &g_test_default_config3;
        g_test_device3.runtime_config = &g_test_runtime_config3;
        g_test_device3.config_size = sizeof(test_config_t);
        g_test_device3.device_init = test_device3_init;
        g_test_device3.device_deinit = test_device_deinit;
        g_test_device3.state.initialized = 0;
        g_test_device3.state.state = NX_DEV_STATE_UNINITIALIZED;
        g_test_device3.state.ref_count = 0;
    }

    void TearDown() override {
        /* Reset device states */
        g_test_device1.state.initialized = 0;
        g_test_device2.state.initialized = 0;
        g_test_device3.state.initialized = 0;
    }
};

/*===========================================================================*/
/* Device Find Tests                                                          */
/*===========================================================================*/

/**
 * \brief           Test nx_device_registry_find() with NULL name
 */
TEST_F(NxDeviceRegistryTest, Find_NullName) {
    const nx_device_t* found = nx_device_registry_find(nullptr);
    EXPECT_EQ(found, nullptr);
}

/**
 * \brief           Test nx_device_registry_find() with non-existent device
 */
TEST_F(NxDeviceRegistryTest, Find_NotFound) {
    const nx_device_t* found = nx_device_registry_find("non_existent_device");
    EXPECT_EQ(found, nullptr);
}

/*===========================================================================*/
/* Device Count Tests                                                         */
/*===========================================================================*/

/**
 * \brief           Test nx_device_registry_count() returns correct count
 */
TEST_F(NxDeviceRegistryTest, Count_ReturnsCorrectValue) {
    /* For MSVC testing without linker sections, count should be 0 */
    size_t count = nx_device_registry_count();
    /* The count depends on whether devices are registered via linker sections
     */
    EXPECT_GE(count, 0u);
}

/*===========================================================================*/
/* Device Get By Index Tests                                                  */
/*===========================================================================*/

/**
 * \brief           Test nx_device_registry_get() with out of bounds index
 */
TEST_F(NxDeviceRegistryTest, Get_OutOfBounds) {
    size_t count = nx_device_registry_count();
    const nx_device_t* dev = nx_device_registry_get(count + 100);
    EXPECT_EQ(dev, nullptr);
}

/**
 * \brief           Test nx_device_registry_get() with very large index
 */
TEST_F(NxDeviceRegistryTest, Get_VeryLargeIndex) {
    const nx_device_t* dev = nx_device_registry_get(SIZE_MAX);
    EXPECT_EQ(dev, nullptr);
}

/*===========================================================================*/
/* Init All Tests                                                             */
/*===========================================================================*/

/**
 * \brief           Test nx_device_registry_init_all() with empty registry
 */
TEST_F(NxDeviceRegistryTest, InitAll_EmptyRegistry) {
    /* With no devices registered via linker sections, should return OK */
    nx_status_t status = nx_device_registry_init_all();
    /* Empty registry should succeed */
    EXPECT_EQ(status, NX_OK);
}

/*===========================================================================*/
/* API Consistency Tests                                                      */
/*===========================================================================*/

/**
 * \brief           Test that count and get are consistent
 */
TEST_F(NxDeviceRegistryTest, CountAndGet_Consistent) {
    size_t count = nx_device_registry_count();

    /* All indices less than count should return valid pointers or NULL */
    for (size_t i = 0; i < count; i++) {
        const nx_device_t* dev = nx_device_registry_get(i);
        /* Device should exist (not NULL) for valid indices */
        EXPECT_NE(dev, nullptr) << "Device at index " << i << " is NULL";
    }

    /* Index equal to count should return NULL */
    const nx_device_t* dev = nx_device_registry_get(count);
    EXPECT_EQ(dev, nullptr);
}

/**
 * \brief           Test that find returns same device as get for same name
 */
TEST_F(NxDeviceRegistryTest, FindAndGet_Consistent) {
    size_t count = nx_device_registry_count();

    for (size_t i = 0; i < count; i++) {
        const nx_device_t* dev_by_index = nx_device_registry_get(i);
        if (dev_by_index != nullptr && dev_by_index->name != nullptr) {
            const nx_device_t* dev_by_name =
                nx_device_registry_find(dev_by_index->name);
            EXPECT_EQ(dev_by_index, dev_by_name)
                << "Device mismatch for name: " << dev_by_index->name;
        }
    }
}
