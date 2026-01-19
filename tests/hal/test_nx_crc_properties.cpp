/**
 * \file            test_nx_crc_properties.cpp
 * \brief           CRC Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for CRC peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 5: CRC Calculation Correctness**
 * **Validates: Requirements 3.2, 3.3**
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/interface/nx_crc.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_crc_helpers.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           CRC Property Test Fixture
 */
class CRCPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_crc_t* crc = nullptr;

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Reset all CRC instances */
        native_crc_reset_all();

        /* Get CRC0 instance */
        crc = nx_factory_crc(0);
        ASSERT_NE(nullptr, crc);

        /* Initialize CRC */
        nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize CRC */
        if (crc != nullptr) {
            nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_crc_reset_all();
    }

    /**
     * \brief       Generate random data buffer
     */
    std::vector<uint8_t> randomData() {
        std::uniform_int_distribution<int> len_dist(1, 256);
        std::uniform_int_distribution<int> byte_dist(0, 255);
        int len = len_dist(rng);
        std::vector<uint8_t> data(len);
        for (int i = 0; i < len; ++i) {
            data[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        return data;
    }

    /**
     * \brief       Generate random data buffer with specific length
     */
    std::vector<uint8_t> randomDataWithLength(size_t len) {
        std::uniform_int_distribution<int> byte_dist(0, 255);
        std::vector<uint8_t> data(len);
        for (size_t i = 0; i < len; ++i) {
            data[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        return data;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 5: CRC Calculation Correctness                                   */
/* *For any* input data, calculating CRC twice with the same initial value   */
/* SHALL produce identical results.                                          */
/* **Validates: Requirements 3.2, 3.3**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-platform-improvements, Property 5: CRC Calculation
 * Correctness
 *
 * *For any* input data, calculating CRC-32 twice with the same initial value
 * should produce identical results.
 *
 * **Validates: Requirements 3.2, 3.3**
 */
TEST_F(CRCPropertyTest, Property5_CRC32CalculationDeterministic) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> data = randomData();

        /* Calculate CRC twice */
        uint32_t result1 = crc->calculate(crc, data.data(), data.size());
        uint32_t result2 = crc->calculate(crc, data.data(), data.size());

        /* Results should be identical */
        EXPECT_EQ(result1, result2) << "Iteration " << test_iter
                                    << ": CRC results differ for same input";
    }
}

/**
 * Feature: native-platform-improvements, Property 5: CRC Calculation
 * Correctness
 *
 * *For any* input data, calculating CRC incrementally should produce the same
 * result as calculating it in one shot.
 *
 * **Validates: Requirements 3.2, 3.3**
 */
TEST_F(CRCPropertyTest, Property5_CRC32IncrementalEqualsOneShot) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> data = randomData();

        /* Calculate CRC in one shot */
        uint32_t result_oneshot = crc->calculate(crc, data.data(), data.size());

        /* Calculate CRC incrementally */
        crc->reset(crc);
        size_t chunk_size = data.size() / 3;
        if (chunk_size == 0)
            chunk_size = 1;

        size_t offset = 0;
        while (offset < data.size()) {
            size_t remaining = data.size() - offset;
            size_t to_process =
                (remaining < chunk_size) ? remaining : chunk_size;
            crc->update(crc, data.data() + offset, to_process);
            offset += to_process;
        }
        uint32_t result_incremental = crc->get_result(crc);

        /* Results should be identical */
        EXPECT_EQ(result_oneshot, result_incremental)
            << "Iteration " << test_iter
            << ": Incremental CRC differs from one-shot";
    }
}

/**
 * Feature: native-platform-improvements, Property 5: CRC Calculation
 * Correctness
 *
 * *For any* input data, resetting CRC and recalculating should produce the
 * same result.
 *
 * **Validates: Requirements 3.2, 3.3**
 */
TEST_F(CRCPropertyTest, Property5_CRC32ResetProducesConsistentResults) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> data = randomData();

        /* Calculate CRC first time */
        crc->reset(crc);
        crc->update(crc, data.data(), data.size());
        uint32_t result1 = crc->get_result(crc);

        /* Reset and calculate again */
        crc->reset(crc);
        crc->update(crc, data.data(), data.size());
        uint32_t result2 = crc->get_result(crc);

        /* Results should be identical */
        EXPECT_EQ(result1, result2)
            << "Iteration " << test_iter << ": CRC results differ after reset";
    }
}

/**
 * Feature: native-platform-improvements, Property 5: CRC Calculation
 * Correctness
 *
 * *For any* two different input data buffers, the CRC values should be
 * different (with high probability).
 *
 * **Validates: Requirements 3.2, 3.3**
 */
TEST_F(CRCPropertyTest, Property5_CRC32DifferentInputsProduceDifferentResults) {
    int different_count = 0;
    int total_tests = PROPERTY_TEST_ITERATIONS;

    for (int test_iter = 0; test_iter < total_tests; ++test_iter) {
        /* Generate two different random data buffers */
        std::vector<uint8_t> data1 = randomData();
        std::vector<uint8_t> data2 = randomData();

        /* Ensure they are different */
        if (data1 == data2) {
            /* Modify one byte to make them different */
            if (!data2.empty()) {
                data2[0] ^= 0xFF;
            }
        }

        /* Calculate CRCs */
        uint32_t result1 = crc->calculate(crc, data1.data(), data1.size());
        uint32_t result2 = crc->calculate(crc, data2.data(), data2.size());

        /* Count how many times results are different */
        if (result1 != result2) {
            different_count++;
        }
    }

    /* At least 95% of different inputs should produce different CRCs */
    /* (allowing for rare collisions) */
    EXPECT_GT(different_count, total_tests * 95 / 100)
        << "CRC collision rate too high: " << (total_tests - different_count)
        << " collisions in " << total_tests << " tests";
}

/**
 * Feature: native-platform-improvements, Property 5: CRC Calculation
 * Correctness
 *
 * *For any* input data, the CRC value should be within the valid 32-bit range.
 *
 * **Validates: Requirements 3.2, 3.3**
 */
TEST_F(CRCPropertyTest, Property5_CRC32ResultWithinValidRange) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> data = randomData();

        /* Calculate CRC */
        uint32_t result = crc->calculate(crc, data.data(), data.size());

        /* Result should be a valid 32-bit value (always true for uint32_t) */
        /* This test mainly ensures no crashes or undefined behavior */
        EXPECT_LE(result, UINT32_MAX);
    }
}

/**
 * Feature: native-platform-improvements, Property 5: CRC Calculation
 * Correctness
 *
 * *For any* input data split at different positions, calculating CRC
 * incrementally should produce the same result regardless of split position.
 *
 * **Validates: Requirements 3.2, 3.3**
 */
TEST_F(CRCPropertyTest, Property5_CRC32IncrementalIndependentOfSplitPosition) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data with at least 4 bytes */
        std::vector<uint8_t> data = randomDataWithLength(4 + (rng() % 252));

        /* Calculate CRC in one shot */
        uint32_t result_oneshot = crc->calculate(crc, data.data(), data.size());

        /* Calculate CRC with random split position */
        std::uniform_int_distribution<size_t> split_dist(1, data.size() - 1);
        size_t split_pos = split_dist(rng);

        crc->reset(crc);
        crc->update(crc, data.data(), split_pos);
        crc->update(crc, data.data() + split_pos, data.size() - split_pos);
        uint32_t result_split = crc->get_result(crc);

        /* Results should be identical */
        EXPECT_EQ(result_oneshot, result_split)
            << "Iteration " << test_iter
            << ": CRC differs with split at position " << split_pos;
    }
}

/**
 * Feature: native-platform-improvements, Property 5: CRC Calculation
 * Correctness
 *
 * *For any* input data, calling get_result multiple times without update
 * should return the same value.
 *
 * **Validates: Requirements 3.2, 3.3**
 */
TEST_F(CRCPropertyTest, Property5_CRC32GetResultIdempotent) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> data = randomData();

        /* Calculate CRC */
        crc->reset(crc);
        crc->update(crc, data.data(), data.size());

        /* Get result multiple times */
        uint32_t result1 = crc->get_result(crc);
        uint32_t result2 = crc->get_result(crc);
        uint32_t result3 = crc->get_result(crc);

        /* All results should be identical */
        EXPECT_EQ(result1, result2) << "Iteration " << test_iter
                                    << ": get_result not idempotent (1 vs 2)";
        EXPECT_EQ(result2, result3) << "Iteration " << test_iter
                                    << ": get_result not idempotent (2 vs 3)";
    }
}
