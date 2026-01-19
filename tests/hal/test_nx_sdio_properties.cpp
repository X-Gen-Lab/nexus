/**
 * \file            test_nx_sdio_properties.cpp
 * \brief           SDIO Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for SDIO peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 13: SDIO Block Read/Write Round Trip**
 * **Validates: Requirements 8.3, 8.4**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/interface/nx_sdio.h"
#include "native_sdio_test.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           SDIO Property Test Fixture
 */
class SDIOPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_sdio_t* sdio = nullptr;

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Reset all SDIO instances */
        nx_sdio_native_reset_all();

        /* Get SDIO0 instance */
        sdio = nx_sdio_native_get(0);
        ASSERT_NE(nullptr, sdio);

        /* Set card present */
        ASSERT_EQ(NX_OK, nx_sdio_native_set_card_present(0, true));

        /* Initialize SDIO */
        nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize SDIO */
        if (sdio != nullptr) {
            nx_lifecycle_t* lifecycle = sdio->get_lifecycle(sdio);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        nx_sdio_native_reset_all();
    }

    /**
     * \brief       Generate random data buffer
     */
    std::vector<uint8_t> randomData(size_t len) {
        std::uniform_int_distribution<int> byte_dist(0, 255);
        std::vector<uint8_t> data(len);
        for (size_t i = 0; i < len; ++i) {
            data[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        return data;
    }

    /**
     * \brief       Generate random block number
     */
    uint32_t randomBlockNumber(size_t block_count = 1) {
        /* Get capacity and block size */
        uint64_t capacity = sdio->get_capacity(sdio);
        size_t block_size = sdio->get_block_size(sdio);
        uint32_t max_blocks = (uint32_t)(capacity / block_size);

        /* Generate random block number that leaves room for block_count */
        std::uniform_int_distribution<uint32_t> block_dist(
            0, max_blocks - (uint32_t)block_count);
        return block_dist(rng);
    }

    /**
     * \brief       Generate random block count
     */
    size_t randomBlockCount() {
        /* Get capacity and block size */
        uint64_t capacity = sdio->get_capacity(sdio);
        size_t block_size = sdio->get_block_size(sdio);
        uint32_t max_blocks = (uint32_t)(capacity / block_size);

        /* Generate random block count (1 to 10 blocks) */
        std::uniform_int_distribution<size_t> count_dist(
            1, std::min(10u, max_blocks));
        return count_dist(rng);
    }
};

/*---------------------------------------------------------------------------*/
/* Property 13: SDIO Block Read/Write Round Trip                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Property 13: SDIO Block Read/Write Round Trip
 *
 * Feature: native-platform-improvements, Property 13:
 * For any block data, writing to an SDIO block and then reading it
 * should return the same data.
 *
 * Validates: Requirements 8.3, 8.4
 */
TEST_F(SDIOPropertyTest, Property13_BlockReadWriteRoundTrip) {
    size_t block_size = sdio->get_block_size(sdio);

    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration) {
        /* Generate random block number and count */
        size_t block_count = randomBlockCount();
        uint32_t block_num = randomBlockNumber(block_count);

        /* Generate random data */
        size_t data_size = block_size * block_count;
        std::vector<uint8_t> write_data = randomData(data_size);
        std::vector<uint8_t> read_data(data_size);

        /* Write blocks */
        nx_status_t write_status =
            sdio->write(sdio, block_num, write_data.data(), block_count);
        ASSERT_EQ(NX_OK, write_status)
            << "Iteration " << iteration << ": Write failed for block "
            << block_num << " count " << block_count;

        /* Read blocks */
        nx_status_t read_status =
            sdio->read(sdio, block_num, read_data.data(), block_count);
        ASSERT_EQ(NX_OK, read_status)
            << "Iteration " << iteration << ": Read failed for block "
            << block_num << " count " << block_count;

        /* Verify data matches (round trip property) */
        ASSERT_EQ(write_data, read_data)
            << "Iteration " << iteration
            << ": Round trip failed - data mismatch for block " << block_num
            << " count " << block_count;
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Single Block Round Trip                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Additional Property: Single Block Round Trip
 *
 * Feature: native-platform-improvements
 * For any single block, writing and reading should preserve data.
 *
 * Validates: Requirements 8.3, 8.4
 */
TEST_F(SDIOPropertyTest, Property_SingleBlockRoundTrip) {
    size_t block_size = sdio->get_block_size(sdio);

    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration) {
        /* Generate random block number */
        uint32_t block_num = randomBlockNumber(1);

        /* Generate random data */
        std::vector<uint8_t> write_data = randomData(block_size);
        std::vector<uint8_t> read_data(block_size);

        /* Write single block */
        ASSERT_EQ(NX_OK, sdio->write(sdio, block_num, write_data.data(), 1))
            << "Iteration " << iteration << ": Write failed for block "
            << block_num;

        /* Read single block */
        ASSERT_EQ(NX_OK, sdio->read(sdio, block_num, read_data.data(), 1))
            << "Iteration " << iteration << ": Read failed for block "
            << block_num;

        /* Verify data matches */
        ASSERT_EQ(write_data, read_data)
            << "Iteration " << iteration << ": Data mismatch for block "
            << block_num;
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Erase Then Read                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Additional Property: Erase Then Read
 *
 * Feature: native-platform-improvements
 * For any block range, erasing and then reading should return 0xFF.
 *
 * Validates: Requirements 8.3, 8.5
 */
TEST_F(SDIOPropertyTest, Property_EraseThenRead) {
    size_t block_size = sdio->get_block_size(sdio);

    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration) {
        /* Generate random block number and count */
        size_t block_count = randomBlockCount();
        uint32_t block_num = randomBlockNumber(block_count);

        /* Write some data first */
        size_t data_size = block_size * block_count;
        std::vector<uint8_t> write_data = randomData(data_size);
        ASSERT_EQ(NX_OK,
                  sdio->write(sdio, block_num, write_data.data(), block_count))
            << "Iteration " << iteration << ": Write failed";

        /* Erase blocks */
        ASSERT_EQ(NX_OK, sdio->erase(sdio, block_num, block_count))
            << "Iteration " << iteration << ": Erase failed for block "
            << block_num << " count " << block_count;

        /* Read blocks */
        std::vector<uint8_t> read_data(data_size);
        ASSERT_EQ(NX_OK,
                  sdio->read(sdio, block_num, read_data.data(), block_count))
            << "Iteration " << iteration << ": Read failed";

        /* Verify all bytes are 0xFF (erased state) */
        for (size_t i = 0; i < data_size; ++i) {
            ASSERT_EQ(0xFF, read_data[i])
                << "Iteration " << iteration << ": Byte " << i
                << " not erased (expected 0xFF, got " << std::hex
                << (int)read_data[i] << ")";
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Multiple Writes Same Block                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Additional Property: Multiple Writes Same Block
 *
 * Feature: native-platform-improvements
 * For any block, multiple writes should preserve the last written data.
 *
 * Validates: Requirements 8.4
 */
TEST_F(SDIOPropertyTest, Property_MultipleWritesSameBlock) {
    size_t block_size = sdio->get_block_size(sdio);

    for (int iteration = 0; iteration < PROPERTY_TEST_ITERATIONS; ++iteration) {
        /* Generate random block number */
        uint32_t block_num = randomBlockNumber(1);

        /* Write multiple times */
        std::vector<uint8_t> final_data;
        for (int write_num = 0; write_num < 3; ++write_num) {
            final_data = randomData(block_size);
            ASSERT_EQ(NX_OK, sdio->write(sdio, block_num, final_data.data(), 1))
                << "Iteration " << iteration << ", write " << write_num
                << ": Write failed";
        }

        /* Read and verify last written data */
        std::vector<uint8_t> read_data(block_size);
        ASSERT_EQ(NX_OK, sdio->read(sdio, block_num, read_data.data(), 1))
            << "Iteration " << iteration << ": Read failed";

        ASSERT_EQ(final_data, read_data)
            << "Iteration " << iteration << ": Last written data not preserved";
    }
}
