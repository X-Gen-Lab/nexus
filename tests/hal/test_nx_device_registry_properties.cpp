/**
 * \file            test_nx_device_registry_properties.cpp
 * \brief           Property-based tests for nx_device_registry.h
 * \author          Nexus Team
 *
 * Property-based tests for the static device registry.
 * These tests verify universal properties that should hold across
 * all valid executions.
 *
 * **Feature: static-registry**
 */

#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include "hal/nx_device_registry.h"
#include "hal/nx_status.h"
}

/*---------------------------------------------------------------------------*/
/* Test Fixture                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test fixture for property-based tests
 */
class NxDeviceRegistryPropertiesTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset any state if needed */
    }

    void TearDown() override {
        /* Cleanup */
    }
};

/*---------------------------------------------------------------------------*/
/* Property 3: Device Registry Iteration Completeness                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Property test: Iteration visits all devices exactly once
 *
 * **Feature: static-registry, Property 3: Device Registry Iteration
 * Completeness**
 * **Validates: Requirements 2.3, 2.5**
 *
 * Property: For any set of registered devices, iterating with
 * NX_DEVICE_FOREACH shall visit each device exactly once, and
 * NX_DEVICE_COUNT() shall equal the number of devices visited.
 */
TEST_F(NxDeviceRegistryPropertiesTest, Property3_IterationCompleteness) {
    /* Get the expected count */
    size_t expected_count = nx_device_registry_count();

    /* Count devices visited during iteration */
    size_t visited_count = 0;

    /* Iterate using the get function (since MSVC doesn't support linker
     * sections) */
    for (size_t i = 0; i < expected_count; i++) {
        const nx_device_t* dev = nx_device_registry_get(i);
        if (dev != nullptr) {
            visited_count++;
        }
    }

    /* Property: visited_count == expected_count */
    EXPECT_EQ(visited_count, expected_count)
        << "Iteration should visit exactly NX_DEVICE_COUNT() devices";
}

/**
 * \brief           Property test: Count matches iteration
 *
 * **Feature: static-registry, Property 3: Device Registry Iteration
 * Completeness**
 * **Validates: Requirements 2.3, 2.5**
 *
 * Property: NX_DEVICE_COUNT() must equal the number of devices
 * that can be accessed via nx_device_registry_get().
 */
TEST_F(NxDeviceRegistryPropertiesTest, Property3_CountMatchesIteration) {
    size_t count = nx_device_registry_count();

    /* All indices from 0 to count-1 should return valid devices */
    for (size_t i = 0; i < count; i++) {
        const nx_device_t* dev = nx_device_registry_get(i);
        EXPECT_NE(dev, nullptr)
            << "Device at index " << i << " should not be NULL";
    }

    /* Index equal to count should return NULL */
    const nx_device_t* dev = nx_device_registry_get(count);
    EXPECT_EQ(dev, nullptr) << "Index equal to count should return NULL";
}

/**
 * \brief           Property test: No duplicate devices in iteration
 *
 * **Feature: static-registry, Property 3: Device Registry Iteration
 * Completeness**
 * **Validates: Requirements 2.3, 2.5**
 *
 * Property: Each device should appear exactly once during iteration.
 * No device pointer should be returned more than once.
 */
TEST_F(NxDeviceRegistryPropertiesTest, Property3_NoDuplicates) {
    size_t count = nx_device_registry_count();
    std::set<const nx_device_t*> seen_devices;

    for (size_t i = 0; i < count; i++) {
        const nx_device_t* dev = nx_device_registry_get(i);
        if (dev != nullptr) {
            /* Check if we've seen this device before */
            auto result = seen_devices.insert(dev);
            EXPECT_TRUE(result.second)
                << "Device at index " << i << " is a duplicate";
        }
    }

    /* Number of unique devices should equal count */
    EXPECT_EQ(seen_devices.size(), count)
        << "Number of unique devices should equal count";
}

/**
 * \brief           Property test: Iteration order is deterministic
 *
 * **Feature: static-registry, Property 3: Device Registry Iteration
 * Completeness**
 * **Validates: Requirements 2.3, 2.5**
 *
 * Property: Multiple iterations should return devices in the same order.
 */
TEST_F(NxDeviceRegistryPropertiesTest, Property3_DeterministicOrder) {
    size_t count = nx_device_registry_count();

    /* First iteration */
    std::vector<const nx_device_t*> first_iteration;
    for (size_t i = 0; i < count; i++) {
        first_iteration.push_back(nx_device_registry_get(i));
    }

    /* Second iteration */
    std::vector<const nx_device_t*> second_iteration;
    for (size_t i = 0; i < count; i++) {
        second_iteration.push_back(nx_device_registry_get(i));
    }

    /* Property: Both iterations should be identical */
    EXPECT_EQ(first_iteration.size(), second_iteration.size());
    for (size_t i = 0; i < first_iteration.size(); i++) {
        EXPECT_EQ(first_iteration[i], second_iteration[i])
            << "Device at index " << i << " differs between iterations";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 4: Device Lookup Correctness                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Property test: Find returns correct device
 *
 * **Feature: static-registry, Property 4: Device Lookup Correctness**
 * **Validates: Requirements 2.4**
 *
 * Property: For any registered device with name N,
 * nx_device_registry_find(N) shall return a pointer to that device.
 */
TEST_F(NxDeviceRegistryPropertiesTest, Property4_FindReturnsCorrectDevice) {
    size_t count = nx_device_registry_count();

    for (size_t i = 0; i < count; i++) {
        const nx_device_t* dev = nx_device_registry_get(i);
        if (dev != nullptr && dev->name != nullptr) {
            /* Find the device by name */
            const nx_device_t* found = nx_device_registry_find(dev->name);

            /* Property: Found device should be the same as the original */
            EXPECT_EQ(found, dev)
                << "Find should return the same device for name: " << dev->name;
        }
    }
}

/**
 * \brief           Property test: Find returns NULL for non-existent names
 *
 * **Feature: static-registry, Property 4: Device Lookup Correctness**
 * **Validates: Requirements 2.4**
 *
 * Property: For any name M not registered,
 * nx_device_registry_find(M) shall return NULL.
 */
TEST_F(NxDeviceRegistryPropertiesTest,
       Property4_FindReturnsNullForNonExistent) {
    /* Test with various non-existent names */
    std::vector<const char*> non_existent_names = {
        "non_existent_device_1",
        "non_existent_device_2",
        "xyz_device_abc",
        "test_device_999",
        "", /* Empty string */
    };

    for (const char* name : non_existent_names) {
        const nx_device_t* found = nx_device_registry_find(name);
        EXPECT_EQ(found, nullptr)
            << "Find should return NULL for non-existent name: " << name;
    }
}

/**
 * \brief           Property test: Find with NULL returns NULL
 *
 * **Feature: static-registry, Property 4: Device Lookup Correctness**
 * **Validates: Requirements 2.4**
 *
 * Property: nx_device_registry_find(NULL) shall return NULL.
 */
TEST_F(NxDeviceRegistryPropertiesTest, Property4_FindNullReturnsNull) {
    const nx_device_t* found = nx_device_registry_find(nullptr);
    EXPECT_EQ(found, nullptr) << "Find should return NULL for NULL name";
}

/**
 * \brief           Property test: Find is idempotent
 *
 * **Feature: static-registry, Property 4: Device Lookup Correctness**
 * **Validates: Requirements 2.4**
 *
 * Property: Multiple calls to nx_device_registry_find() with the same
 * name should return the same result.
 */
TEST_F(NxDeviceRegistryPropertiesTest, Property4_FindIsIdempotent) {
    size_t count = nx_device_registry_count();

    for (size_t i = 0; i < count; i++) {
        const nx_device_t* dev = nx_device_registry_get(i);
        if (dev != nullptr && dev->name != nullptr) {
            /* Find the device multiple times */
            const nx_device_t* found1 = nx_device_registry_find(dev->name);
            const nx_device_t* found2 = nx_device_registry_find(dev->name);
            const nx_device_t* found3 = nx_device_registry_find(dev->name);

            /* Property: All finds should return the same result */
            EXPECT_EQ(found1, found2);
            EXPECT_EQ(found2, found3);
        }
    }
}

/**
 * \brief           Property test: Device names are unique
 *
 * **Feature: static-registry, Property 4: Device Lookup Correctness**
 * **Validates: Requirements 2.4**
 *
 * Property: No two devices should have the same name.
 */
TEST_F(NxDeviceRegistryPropertiesTest, Property4_UniqueNames) {
    size_t count = nx_device_registry_count();
    std::set<std::string> seen_names;

    for (size_t i = 0; i < count; i++) {
        const nx_device_t* dev = nx_device_registry_get(i);
        if (dev != nullptr && dev->name != nullptr) {
            std::string name(dev->name);
            auto result = seen_names.insert(name);
            EXPECT_TRUE(result.second)
                << "Duplicate device name found: " << name;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Property 6: Device Alignment                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Property test: Device alignment
 *
 * **Feature: static-registry, Property 6: Device Alignment**
 * **Validates: Requirements 3.2**
 *
 * Property: For any registered device, its memory address shall be
 * aligned to sizeof(void*) boundary.
 */
TEST_F(NxDeviceRegistryPropertiesTest, Property6_DeviceAlignment) {
    size_t count = nx_device_registry_count();
    size_t alignment = sizeof(void*);

    for (size_t i = 0; i < count; i++) {
        const nx_device_t* dev = nx_device_registry_get(i);
        if (dev != nullptr) {
            /* Check alignment */
            uintptr_t addr = reinterpret_cast<uintptr_t>(dev);
            EXPECT_EQ(addr % alignment, 0u)
                << "Device at index " << i << " is not properly aligned. "
                << "Address: " << std::hex << addr
                << ", Required alignment: " << std::dec << alignment;
        }
    }
}

/**
 * \brief           Property test: Device structure size is aligned
 *
 * **Feature: static-registry, Property 6: Device Alignment**
 * **Validates: Requirements 3.2**
 *
 * Property: The size of nx_device_t should be a multiple of sizeof(void*)
 * to ensure proper alignment when devices are stored in an array.
 */
TEST_F(NxDeviceRegistryPropertiesTest, Property6_StructureSizeAligned) {
    size_t alignment = sizeof(void*);
    size_t device_size = sizeof(nx_device_t);

    /* Property: Device size should be a multiple of pointer size */
    EXPECT_EQ(device_size % alignment, 0u)
        << "nx_device_t size (" << device_size
        << ") should be a multiple of pointer size (" << alignment << ")";
}

/*---------------------------------------------------------------------------*/
/* Additional Consistency Properties                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Property test: Get and count are consistent
 *
 * **Feature: static-registry**
 * **Validates: Requirements 2.3, 2.5**
 *
 * Property: nx_device_registry_get(i) returns non-NULL for all i < count
 * and NULL for all i >= count.
 */
TEST_F(NxDeviceRegistryPropertiesTest, GetAndCountConsistent) {
    size_t count = nx_device_registry_count();

    /* All valid indices should return non-NULL */
    for (size_t i = 0; i < count; i++) {
        const nx_device_t* dev = nx_device_registry_get(i);
        EXPECT_NE(dev, nullptr)
            << "Index " << i << " should return valid device";
    }

    /* Invalid indices should return NULL */
    EXPECT_EQ(nx_device_registry_get(count), nullptr);
    EXPECT_EQ(nx_device_registry_get(count + 1), nullptr);
    EXPECT_EQ(nx_device_registry_get(SIZE_MAX), nullptr);
}

/**
 * \brief           Property test: Count is stable
 *
 * **Feature: static-registry**
 * **Validates: Requirements 2.5**
 *
 * Property: Multiple calls to nx_device_registry_count() should return
 * the same value (count doesn't change during runtime for static registry).
 */
TEST_F(NxDeviceRegistryPropertiesTest, CountIsStable) {
    size_t count1 = nx_device_registry_count();
    size_t count2 = nx_device_registry_count();
    size_t count3 = nx_device_registry_count();

    EXPECT_EQ(count1, count2);
    EXPECT_EQ(count2, count3);
}
