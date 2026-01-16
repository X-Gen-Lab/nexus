/**
 * \file            test_osal_mem_properties.cpp
 * \brief           OSAL Memory Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Memory module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           OSAL Memory Property Test Fixture
 */
class OsalMemPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        osal_init();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        /* No specific cleanup needed */
    }

    /**
     * \brief       Generate random allocation size (1-8192 bytes)
     */
    size_t randomSize() {
        std::uniform_int_distribution<size_t> dist(1, 8192);
        return dist(rng);
    }

    /**
     * \brief       Generate random small allocation size (1-256 bytes)
     */
    size_t randomSmallSize() {
        std::uniform_int_distribution<size_t> dist(1, 256);
        return dist(rng);
    }

    /**
     * \brief       Generate random alignment (power of 2: 1, 2, 4, 8, 16, 32,
     * 64)
     */
    size_t randomAlignment() {
        const size_t alignments[] = {1, 2, 4, 8, 16, 32, 64};
        std::uniform_int_distribution<size_t> dist(0, 6);
        return alignments[dist(rng)];
    }

    /**
     * \brief       Generate random byte value
     */
    uint8_t randomByte() {
        std::uniform_int_distribution<int> dist(0, 255);
        return static_cast<uint8_t>(dist(rng));
    }
};

/*---------------------------------------------------------------------------*/
/* Property 8: Memory Allocation Round-Trip                                  */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 8: Memory Allocation Round-Trip
 *
 * *For any* valid allocation size, allocating memory and then freeing it
 * SHALL not cause memory leaks or corruption (free heap size should return
 * to approximately the same level).
 *
 * **Validates: Requirements 5.1, 5.4**
 */
TEST_F(OsalMemPropertyTest, Property8_MemoryAllocationRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random allocation size */
        size_t alloc_size = randomSize();

        /* Get initial free heap size */
        size_t free_before = osal_mem_get_free_size();

        /* Allocate memory */
        void* ptr = osal_mem_alloc(alloc_size);
        ASSERT_NE(nullptr, ptr)
            << "Iteration " << test_iter << ": allocation failed for size "
            << alloc_size;

        /* Verify free size decreased */
        size_t free_after_alloc = osal_mem_get_free_size();
        EXPECT_LT(free_after_alloc, free_before)
            << "Iteration " << test_iter
            << ": free size should decrease after allocation";

        /* Free memory */
        osal_mem_free(ptr);

        /* Get free heap size after free */
        size_t free_after_free = osal_mem_get_free_size();

        /* Verify free size increased back (allowing for small
         * overhead/fragmentation) */
        EXPECT_GE(free_after_free, free_after_alloc)
            << "Iteration " << test_iter
            << ": free size should increase after free "
            << "(before=" << free_before << ", after_alloc=" << free_after_alloc
            << ", after_free=" << free_after_free << ")";

        /* Free size should be close to original (within reasonable overhead) */
        size_t diff = (free_before > free_after_free)
                          ? (free_before - free_after_free)
                          : (free_after_free - free_before);
        EXPECT_LE(diff, 128) << "Iteration " << test_iter
                             << ": free size should return to approximately "
                             << "the same level (diff=" << diff << " bytes)";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 9: Calloc Zero-Initialization                                    */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 9: Calloc Zero-Initialization
 *
 * *For any* calloc allocation, all bytes in the returned memory block
 * SHALL be initialized to zero.
 *
 * **Validates: Requirements 6.1**
 */
TEST_F(OsalMemPropertyTest, Property9_CallocZeroInitialization) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random count and size */
        size_t count = randomSmallSize();
        size_t size = randomSmallSize();

        /* Allocate memory with calloc */
        uint8_t* ptr = static_cast<uint8_t*>(osal_mem_calloc(count, size));
        ASSERT_NE(nullptr, ptr)
            << "Iteration " << test_iter
            << ": calloc failed for count=" << count << ", size=" << size;

        /* Verify all bytes are zero */
        size_t total_bytes = count * size;
        for (size_t i = 0; i < total_bytes; ++i) {
            EXPECT_EQ(0, ptr[i])
                << "Iteration " << test_iter << ": byte " << i
                << " is not zero "
                << "(count=" << count << ", size=" << size << ")";
        }

        /* Clean up */
        osal_mem_free(ptr);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 10: Realloc Data Preservation                                    */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 10: Realloc Data Preservation
 *
 * *For any* realloc operation that increases size, the original data in
 * the memory block SHALL be preserved in the new allocation.
 *
 * **Validates: Requirements 6.2**
 */
TEST_F(OsalMemPropertyTest, Property10_ReallocDataPreservation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random sizes (ensure new size > old size) */
        size_t old_size = randomSmallSize();
        size_t new_size = old_size + randomSmallSize();

        /* Allocate initial memory */
        uint8_t* ptr = static_cast<uint8_t*>(osal_mem_alloc(old_size));
        ASSERT_NE(nullptr, ptr)
            << "Iteration " << test_iter
            << ": initial allocation failed for size " << old_size;

        /* Fill with random pattern */
        std::vector<uint8_t> pattern(old_size);
        for (size_t i = 0; i < old_size; ++i) {
            pattern[i] = randomByte();
            ptr[i] = pattern[i];
        }

        /* Reallocate to larger size */
        uint8_t* new_ptr =
            static_cast<uint8_t*>(osal_mem_realloc(ptr, new_size));
        ASSERT_NE(nullptr, new_ptr)
            << "Iteration " << test_iter << ": realloc failed "
            << "(old_size=" << old_size << ", new_size=" << new_size << ")";

        /* Verify original data is preserved */
        for (size_t i = 0; i < old_size; ++i) {
            EXPECT_EQ(pattern[i], new_ptr[i])
                << "Iteration " << test_iter << ": byte " << i
                << " was not preserved "
                << "(old_size=" << old_size << ", new_size=" << new_size << ")";
        }

        /* Clean up */
        osal_mem_free(new_ptr);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 11: Aligned Allocation Alignment                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 11: Aligned Allocation Alignment
 *
 * *For any* aligned allocation with alignment A, the returned pointer
 * SHALL be divisible by A (i.e., `(uintptr_t)ptr % alignment == 0`).
 *
 * **Validates: Requirements 6.3**
 */
TEST_F(OsalMemPropertyTest, Property11_AlignedAllocationAlignment) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random alignment and size */
        size_t alignment = randomAlignment();
        size_t size = randomSize();

        /* Allocate aligned memory */
        void* ptr = osal_mem_alloc_aligned(alignment, size);
        ASSERT_NE(nullptr, ptr)
            << "Iteration " << test_iter << ": aligned allocation failed "
            << "(alignment=" << alignment << ", size=" << size << ")";

        /* Verify alignment */
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        EXPECT_EQ(0u, addr % alignment)
            << "Iteration " << test_iter << ": pointer is not aligned "
            << "(alignment=" << alignment << ", addr=0x" << std::hex << addr
            << std::dec << ")";

        /* Verify memory is usable (write and read) */
        uint8_t* byte_ptr = static_cast<uint8_t*>(ptr);
        uint8_t test_value = randomByte();
        byte_ptr[0] = test_value;
        EXPECT_EQ(test_value, byte_ptr[0])
            << "Iteration " << test_iter << ": aligned memory is not writable";

        /* Clean up */
        osal_mem_free(ptr);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 12: Memory Statistics Consistency                                */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-timer-memory, Property 12: Memory Statistics Consistency
 *
 * *For any* sequence of allocations and frees, the free heap size reported
 * by `osal_mem_get_free_size()` SHALL decrease after allocations and increase
 * after frees, and the minimum free size SHALL never increase.
 *
 * **Validates: Requirements 7.2, 7.3**
 */
TEST_F(OsalMemPropertyTest, Property12_MemoryStatisticsConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Get initial statistics */
        size_t initial_free = osal_mem_get_free_size();
        size_t initial_min_free = osal_mem_get_min_free_size();

        /* Generate random number of allocations (1-10) */
        std::uniform_int_distribution<int> count_dist(1, 10);
        int alloc_count = count_dist(rng);

        std::vector<void*> allocations;
        size_t prev_free = initial_free;
        size_t prev_min_free = initial_min_free;

        /* Perform allocations and verify statistics */
        for (int i = 0; i < alloc_count; ++i) {
            size_t alloc_size = randomSmallSize();
            void* ptr = osal_mem_alloc(alloc_size);

            if (ptr != nullptr) {
                allocations.push_back(ptr);

                /* Verify free size decreased */
                size_t current_free = osal_mem_get_free_size();
                EXPECT_LE(current_free, prev_free)
                    << "Iteration " << test_iter << ", alloc " << i
                    << ": free size should decrease or stay same after "
                       "allocation";

                /* Verify min free size never increases */
                size_t current_min_free = osal_mem_get_min_free_size();
                EXPECT_LE(current_min_free, prev_min_free)
                    << "Iteration " << test_iter << ", alloc " << i
                    << ": min free size should never increase";

                /* Verify min free size <= current free size */
                EXPECT_LE(current_min_free, current_free)
                    << "Iteration " << test_iter << ", alloc " << i
                    << ": min free size should be <= current free size";

                prev_free = current_free;
                prev_min_free = current_min_free;
            }
        }

        /* Free all allocations and verify statistics */
        for (void* ptr : allocations) {
            size_t free_before = osal_mem_get_free_size();

            osal_mem_free(ptr);

            /* Verify free size increased */
            size_t free_after = osal_mem_get_free_size();
            EXPECT_GE(free_after, free_before)
                << "Iteration " << test_iter
                << ": free size should increase after free";

            /* Verify min free size still never increases */
            size_t current_min_free = osal_mem_get_min_free_size();
            EXPECT_LE(current_min_free, prev_min_free)
                << "Iteration " << test_iter
                << ": min free size should never increase even after free";

            prev_min_free = current_min_free;
        }
    }
}
