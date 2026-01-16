/**
 * \file            test_osal_mem.cpp
 * \brief           OSAL Memory Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for OSAL Memory module.
 *                  Requirements: 5.1-5.6, 6.1-6.5, 7.1-7.4
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           OSAL Memory Test Fixture
 */
class OsalMemTest : public ::testing::Test {
  protected:
    void SetUp() override {
        osal_init();
    }

    void TearDown() override {
        /* No specific cleanup needed */
    }
};

/*---------------------------------------------------------------------------*/
/* Basic Allocation Tests - Requirements 5.1-5.5                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test memory allocation with valid size
 * \details         Requirements 5.1 - Allocation should succeed
 */
TEST_F(OsalMemTest, AllocValidSize) {
    void* ptr = osal_mem_alloc(100);
    EXPECT_NE(nullptr, ptr);

    /* Clean up */
    osal_mem_free(ptr);
}

/**
 * \brief           Test memory allocation with zero size
 * \details         Requirements 5.2 - Zero size should return NULL
 */
TEST_F(OsalMemTest, AllocZeroSize) {
    void* ptr = osal_mem_alloc(0);
    EXPECT_EQ(nullptr, ptr);
}

/**
 * \brief           Test memory allocation with various sizes
 */
TEST_F(OsalMemTest, AllocVariousSizes) {
    /* Small allocation */
    void* ptr1 = osal_mem_alloc(1);
    EXPECT_NE(nullptr, ptr1);

    /* Medium allocation */
    void* ptr2 = osal_mem_alloc(256);
    EXPECT_NE(nullptr, ptr2);

    /* Large allocation */
    void* ptr3 = osal_mem_alloc(4096);
    EXPECT_NE(nullptr, ptr3);

    /* Clean up */
    osal_mem_free(ptr1);
    osal_mem_free(ptr2);
    osal_mem_free(ptr3);
}

/**
 * \brief           Test memory free with valid pointer
 * \details         Requirements 5.4 - Free should succeed
 */
TEST_F(OsalMemTest, FreeValidPointer) {
    void* ptr = osal_mem_alloc(100);
    ASSERT_NE(nullptr, ptr);

    /* Should not crash */
    osal_mem_free(ptr);
}

/**
 * \brief           Test memory free with NULL pointer
 * \details         Requirements 5.5 - NULL free should be safe
 */
TEST_F(OsalMemTest, FreeNullPointer) {
    /* Should not crash */
    osal_mem_free(nullptr);
}

/**
 * \brief           Test multiple allocations and frees
 */
TEST_F(OsalMemTest, MultipleAllocFree) {
    const int count = 10;
    void* ptrs[count];

    /* Allocate multiple blocks */
    for (int i = 0; i < count; i++) {
        ptrs[i] = osal_mem_alloc(64);
        EXPECT_NE(nullptr, ptrs[i]);
    }

    /* Free all blocks */
    for (int i = 0; i < count; i++) {
        osal_mem_free(ptrs[i]);
    }
}

/**
 * \brief           Test memory can be written and read
 */
TEST_F(OsalMemTest, AllocWriteRead) {
    const size_t size = 100;
    uint8_t* ptr = static_cast<uint8_t*>(osal_mem_alloc(size));
    ASSERT_NE(nullptr, ptr);

    /* Write pattern */
    for (size_t i = 0; i < size; i++) {
        ptr[i] = static_cast<uint8_t>(i & 0xFF);
    }

    /* Verify pattern */
    for (size_t i = 0; i < size; i++) {
        EXPECT_EQ(static_cast<uint8_t>(i & 0xFF), ptr[i]);
    }

    osal_mem_free(ptr);
}

/*---------------------------------------------------------------------------*/
/* Calloc Tests - Requirements 6.1                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test calloc allocation
 * \details         Requirements 6.1 - Calloc should zero-initialize memory
 */
TEST_F(OsalMemTest, CallocZeroInitialized) {
    const size_t count = 10;
    const size_t size = 4;
    uint32_t* ptr = static_cast<uint32_t*>(osal_mem_calloc(count, size));
    ASSERT_NE(nullptr, ptr);

    /* Verify all bytes are zero */
    for (size_t i = 0; i < count; i++) {
        EXPECT_EQ(0u, ptr[i]);
    }

    osal_mem_free(ptr);
}

/**
 * \brief           Test calloc with zero count
 */
TEST_F(OsalMemTest, CallocZeroCount) {
    void* ptr = osal_mem_calloc(0, 10);
    EXPECT_EQ(nullptr, ptr);
}

/**
 * \brief           Test calloc with zero size
 */
TEST_F(OsalMemTest, CallocZeroSize) {
    void* ptr = osal_mem_calloc(10, 0);
    EXPECT_EQ(nullptr, ptr);
}

/**
 * \brief           Test calloc with various sizes
 */
TEST_F(OsalMemTest, CallocVariousSizes) {
    /* Small allocation */
    uint8_t* ptr1 = static_cast<uint8_t*>(osal_mem_calloc(10, 1));
    ASSERT_NE(nullptr, ptr1);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(0, ptr1[i]);
    }
    osal_mem_free(ptr1);

    /* Medium allocation */
    uint32_t* ptr2 =
        static_cast<uint32_t*>(osal_mem_calloc(64, sizeof(uint32_t)));
    ASSERT_NE(nullptr, ptr2);
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(0u, ptr2[i]);
    }
    osal_mem_free(ptr2);
}

/*---------------------------------------------------------------------------*/
/* Realloc Tests - Requirements 6.2, 6.4, 6.5                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test realloc with NULL pointer (behaves like malloc)
 * \details         Requirements 6.5 - Realloc NULL should behave like malloc
 */
TEST_F(OsalMemTest, ReallocNullPointer) {
    void* ptr = osal_mem_realloc(nullptr, 100);
    EXPECT_NE(nullptr, ptr);
    osal_mem_free(ptr);
}

/**
 * \brief           Test realloc with zero size (behaves like free)
 * \details         Requirements 6.4 - Realloc zero size should free memory
 */
TEST_F(OsalMemTest, ReallocZeroSize) {
    void* ptr = osal_mem_alloc(100);
    ASSERT_NE(nullptr, ptr);

    void* new_ptr = osal_mem_realloc(ptr, 0);
    EXPECT_EQ(nullptr, new_ptr);
}

/**
 * \brief           Test realloc increases size and preserves data
 * \details         Requirements 6.2 - Realloc should preserve original data
 */
TEST_F(OsalMemTest, ReallocIncreaseSize) {
    const size_t old_size = 50;
    const size_t new_size = 100;

    uint8_t* ptr = static_cast<uint8_t*>(osal_mem_alloc(old_size));
    ASSERT_NE(nullptr, ptr);

    /* Write pattern to original memory */
    for (size_t i = 0; i < old_size; i++) {
        ptr[i] = static_cast<uint8_t>(i & 0xFF);
    }

    /* Reallocate to larger size */
    uint8_t* new_ptr = static_cast<uint8_t*>(osal_mem_realloc(ptr, new_size));
    ASSERT_NE(nullptr, new_ptr);

    /* Verify original data is preserved */
    for (size_t i = 0; i < old_size; i++) {
        EXPECT_EQ(static_cast<uint8_t>(i & 0xFF), new_ptr[i]);
    }

    osal_mem_free(new_ptr);
}

/**
 * \brief           Test realloc decreases size and preserves data
 */
TEST_F(OsalMemTest, ReallocDecreaseSize) {
    const size_t old_size = 100;
    const size_t new_size = 50;

    uint8_t* ptr = static_cast<uint8_t*>(osal_mem_alloc(old_size));
    ASSERT_NE(nullptr, ptr);

    /* Write pattern to original memory */
    for (size_t i = 0; i < old_size; i++) {
        ptr[i] = static_cast<uint8_t>(i & 0xFF);
    }

    /* Reallocate to smaller size */
    uint8_t* new_ptr = static_cast<uint8_t*>(osal_mem_realloc(ptr, new_size));
    ASSERT_NE(nullptr, new_ptr);

    /* Verify data up to new size is preserved */
    for (size_t i = 0; i < new_size; i++) {
        EXPECT_EQ(static_cast<uint8_t>(i & 0xFF), new_ptr[i]);
    }

    osal_mem_free(new_ptr);
}

/**
 * \brief           Test realloc with same size
 */
TEST_F(OsalMemTest, ReallocSameSize) {
    const size_t size = 100;

    uint8_t* ptr = static_cast<uint8_t*>(osal_mem_alloc(size));
    ASSERT_NE(nullptr, ptr);

    /* Write pattern */
    for (size_t i = 0; i < size; i++) {
        ptr[i] = static_cast<uint8_t>(i & 0xFF);
    }

    /* Reallocate to same size */
    uint8_t* new_ptr = static_cast<uint8_t*>(osal_mem_realloc(ptr, size));
    ASSERT_NE(nullptr, new_ptr);

    /* Verify data is preserved */
    for (size_t i = 0; i < size; i++) {
        EXPECT_EQ(static_cast<uint8_t>(i & 0xFF), new_ptr[i]);
    }

    osal_mem_free(new_ptr);
}

/*---------------------------------------------------------------------------*/
/* Aligned Allocation Tests - Requirements 6.3                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test aligned allocation with valid alignment
 * \details         Requirements 6.3 - Aligned allocation should return aligned
 * pointer
 */
TEST_F(OsalMemTest, AllocAlignedValid) {
    const size_t alignments[] = {1, 2, 4, 8, 16, 32, 64};

    for (size_t alignment : alignments) {
        void* ptr = osal_mem_alloc_aligned(alignment, 100);
        ASSERT_NE(nullptr, ptr);

        /* Verify alignment */
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        EXPECT_EQ(0u, addr % alignment)
            << "Alignment " << alignment << " failed";

        osal_mem_free(ptr);
    }
}

/**
 * \brief           Test aligned allocation with invalid alignment (not power of
 * 2)
 */
TEST_F(OsalMemTest, AllocAlignedInvalidAlignment) {
    /* 3 is not a power of 2 */
    void* ptr = osal_mem_alloc_aligned(3, 100);
    EXPECT_EQ(nullptr, ptr);

    /* 5 is not a power of 2 */
    ptr = osal_mem_alloc_aligned(5, 100);
    EXPECT_EQ(nullptr, ptr);

    /* 0 is not a valid alignment */
    ptr = osal_mem_alloc_aligned(0, 100);
    EXPECT_EQ(nullptr, ptr);
}

/**
 * \brief           Test aligned allocation with zero size
 */
TEST_F(OsalMemTest, AllocAlignedZeroSize) {
    void* ptr = osal_mem_alloc_aligned(16, 0);
    EXPECT_EQ(nullptr, ptr);
}

/**
 * \brief           Test aligned memory can be written and read
 */
TEST_F(OsalMemTest, AllocAlignedWriteRead) {
    const size_t alignment = 16;
    const size_t size = 100;

    uint8_t* ptr =
        static_cast<uint8_t*>(osal_mem_alloc_aligned(alignment, size));
    ASSERT_NE(nullptr, ptr);

    /* Verify alignment */
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(0u, addr % alignment);

    /* Write pattern */
    for (size_t i = 0; i < size; i++) {
        ptr[i] = static_cast<uint8_t>(i & 0xFF);
    }

    /* Verify pattern */
    for (size_t i = 0; i < size; i++) {
        EXPECT_EQ(static_cast<uint8_t>(i & 0xFF), ptr[i]);
    }

    osal_mem_free(ptr);
}

/*---------------------------------------------------------------------------*/
/* Memory Statistics Tests - Requirements 7.1-7.4                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test memory statistics retrieval
 * \details         Requirements 7.1-7.3 - Statistics should be retrievable
 */
TEST_F(OsalMemTest, GetStats) {
    osal_mem_stats_t stats;
    EXPECT_EQ(OSAL_OK, osal_mem_get_stats(&stats));

    /* Stats should have reasonable values */
    EXPECT_GT(stats.total_size, 0u);
    EXPECT_GT(stats.free_size, 0u);
    EXPECT_LE(stats.free_size, stats.total_size);
    EXPECT_LE(stats.min_free_size, stats.free_size);
}

/**
 * \brief           Test memory statistics with NULL pointer
 */
TEST_F(OsalMemTest, GetStatsNullPointer) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_mem_get_stats(nullptr));
}

/**
 * \brief           Test get free size
 * \details         Requirements 7.2 - Free size should be retrievable
 */
TEST_F(OsalMemTest, GetFreeSize) {
    size_t free_size = osal_mem_get_free_size();
    EXPECT_GT(free_size, 0u);
}

/**
 * \brief           Test get minimum free size
 * \details         Requirements 7.3 - Minimum free size should be retrievable
 */
TEST_F(OsalMemTest, GetMinFreeSize) {
    size_t min_free_size = osal_mem_get_min_free_size();
    EXPECT_GT(min_free_size, 0u);

    /* Min free size should be <= current free size */
    size_t free_size = osal_mem_get_free_size();
    EXPECT_LE(min_free_size, free_size);
}

/**
 * \brief           Test statistics consistency after allocation
 */
TEST_F(OsalMemTest, StatsAfterAllocation) {
    size_t free_before = osal_mem_get_free_size();

    /* Allocate memory */
    void* ptr = osal_mem_alloc(1000);
    ASSERT_NE(nullptr, ptr);

    size_t free_after = osal_mem_get_free_size();

    /* Free size should decrease after allocation */
    EXPECT_LT(free_after, free_before);

    osal_mem_free(ptr);
}

/**
 * \brief           Test statistics consistency after free
 */
TEST_F(OsalMemTest, StatsAfterFree) {
    /* Allocate memory */
    void* ptr = osal_mem_alloc(1000);
    ASSERT_NE(nullptr, ptr);

    size_t free_before = osal_mem_get_free_size();

    /* Free memory */
    osal_mem_free(ptr);

    size_t free_after = osal_mem_get_free_size();

    /* Free size should increase after free */
    EXPECT_GT(free_after, free_before);
}

/**
 * \brief           Test minimum free size watermark
 */
TEST_F(OsalMemTest, MinFreeSizeWatermark) {
    size_t initial_min = osal_mem_get_min_free_size();

    /* Allocate large block to potentially lower watermark */
    void* ptr = osal_mem_alloc(5000);
    ASSERT_NE(nullptr, ptr);

    size_t after_alloc_min = osal_mem_get_min_free_size();

    /* Watermark should not increase */
    EXPECT_LE(after_alloc_min, initial_min);

    /* Free memory */
    osal_mem_free(ptr);

    size_t after_free_min = osal_mem_get_min_free_size();

    /* Watermark should remain at lowest point (not increase after free) */
    EXPECT_EQ(after_alloc_min, after_free_min);
}

/**
 * \brief           Test statistics structure fields
 */
TEST_F(OsalMemTest, StatsStructureFields) {
    osal_mem_stats_t stats;
    EXPECT_EQ(OSAL_OK, osal_mem_get_stats(&stats));

    /* Verify relationships between fields */
    EXPECT_LE(stats.free_size, stats.total_size);
    EXPECT_LE(stats.min_free_size, stats.free_size);
    EXPECT_LE(stats.min_free_size, stats.total_size);

    /* Individual getters should match struct values */
    EXPECT_EQ(stats.free_size, osal_mem_get_free_size());
    EXPECT_EQ(stats.min_free_size, osal_mem_get_min_free_size());
}
