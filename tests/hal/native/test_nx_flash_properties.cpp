/**
 * \file            test_nx_flash_properties.cpp
 * \brief           Flash Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Flash peripheral implementation.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Property 6: Flash Erase Before Write**
 * **Property 7: Flash Persistence Round Trip**
 * **Validates: Requirements 4.2, 4.9**
 */

#include <cstdio>
#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/interface/nx_flash.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_flash_helpers.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Flash Property Test Fixture
 */
class FlashPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;
    nx_internal_flash_t* flash = nullptr;
    std::string unique_filename;

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Generate unique filename for this test instance */
        /* Use test name and timestamp to ensure uniqueness */
        const ::testing::TestInfo* test_info =
            ::testing::UnitTest::GetInstance()->current_test_info();
        std::string test_name = test_info->name();

        /* Replace invalid filename characters */
        for (char& c : test_name) {
            if (c == ':' || c == '/' || c == '\\' || c == '*' || c == '?' ||
                c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }

        unique_filename = "flash_test_" + test_name + ".bin";

        /* Reset all Flash instances */
        native_flash_reset_all();

        /* Get Flash0 instance */
        flash = nx_factory_flash(0);
        ASSERT_NE(nullptr, flash);

        /* Set unique backing file before initialization */
        ASSERT_EQ(NX_OK,
                  native_flash_set_backing_file(0, unique_filename.c_str()));

        /* Initialize Flash */
        nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Unlock flash for testing */
        ASSERT_EQ(NX_OK, flash->unlock(flash));
    }

    void TearDown() override {
        /* Deinitialize Flash */
        if (flash != nullptr) {
            nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
            if (lifecycle != nullptr) {
                lifecycle->deinit(lifecycle);
            }
        }

        /* Reset all instances */
        native_flash_reset_all();

        /* Clean up test file */
        std::remove(unique_filename.c_str());
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
     * \brief       Generate random aligned address
     */
    uint32_t randomAlignedAddress() {
        size_t write_unit = flash->get_write_unit(flash);
        size_t page_size = flash->get_page_size(flash);
        std::uniform_int_distribution<uint32_t> addr_dist(
            0, (uint32_t)(page_size * 10 / write_unit) - 1);
        return addr_dist(rng) * (uint32_t)write_unit;
    }

    /**
     * \brief       Generate random aligned length
     */
    size_t randomAlignedLength() {
        size_t write_unit = flash->get_write_unit(flash);
        std::uniform_int_distribution<int> len_dist(1, 64);
        return len_dist(rng) * write_unit;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 6: Flash Erase Before Write                                      */
/* *For any* flash sector, attempting to write without prior erase SHALL     */
/* fail with NX_ERR_INVALID_STATE.                                           */
/* **Validates: Requirements 4.2**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-platform-improvements, Property 6: Flash Erase Before Write
 *
 * *For any* flash address and data, writing without prior erase should fail.
 *
 * **Validates: Requirements 4.2**
 */
TEST_F(FlashPropertyTest, Property6_WriteWithoutEraseFails) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random aligned address and length */
        uint32_t addr = randomAlignedAddress();
        size_t len = randomAlignedLength();

        /* Ensure address is valid */
        if (addr + len > flash->get_page_size(flash) * 128) {
            continue;
        }

        /* Generate random data */
        std::vector<uint8_t> data = randomData(len);

        /* Erase the sector first */
        ASSERT_EQ(NX_OK, flash->erase(flash, addr, static_cast<uint32_t>(len)));

        /* Write data once */
        ASSERT_EQ(NX_OK, flash->write(flash, addr, data.data(),
                                      static_cast<uint32_t>(len)));

        /* Attempt to write again without erase - should fail */
        EXPECT_EQ(
            NX_ERR_INVALID_STATE,
            flash->write(flash, addr, data.data(), static_cast<uint32_t>(len)))
            << "Iteration " << test_iter
            << ": Write without erase should fail at address " << addr;
    }
}

/**
 * Feature: native-platform-improvements, Property 6: Flash Erase Before Write
 *
 * *For any* flash address, after erase, the area should be marked as erased.
 *
 * **Validates: Requirements 4.2**
 */
TEST_F(FlashPropertyTest, Property6_EraseMarksAreaAsErased) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random aligned address and length */
        uint32_t addr = randomAlignedAddress();
        size_t len = randomAlignedLength();

        /* Ensure address is valid */
        if (addr + len > flash->get_page_size(flash) * 128) {
            continue;
        }

        /* Erase the area */
        ASSERT_EQ(NX_OK, flash->erase(flash, addr, static_cast<uint32_t>(len)));

        /* Verify area is erased */
        EXPECT_TRUE(native_flash_is_erased(0, addr, static_cast<uint32_t>(len)))
            << "Iteration " << test_iter << ": Area not erased at address "
            << addr;
    }
}

/**
 * Feature: native-platform-improvements, Property 6: Flash Erase Before Write
 *
 * *For any* flash address, writing after erase should succeed.
 *
 * **Validates: Requirements 4.2**
 */
TEST_F(FlashPropertyTest, Property6_WriteAfterEraseSucceeds) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random aligned address and length */
        uint32_t addr = randomAlignedAddress();
        size_t len = randomAlignedLength();

        /* Ensure address is valid */
        if (addr + len > flash->get_page_size(flash) * 128) {
            continue;
        }

        /* Generate random data */
        std::vector<uint8_t> data = randomData(len);

        /* Erase then write */
        ASSERT_EQ(NX_OK, flash->erase(flash, addr, static_cast<uint32_t>(len)));
        EXPECT_EQ(NX_OK, flash->write(flash, addr, data.data(),
                                      static_cast<uint32_t>(len)))
            << "Iteration " << test_iter
            << ": Write after erase should succeed at address " << addr;
    }
}

/*---------------------------------------------------------------------------*/
/* Property 7: Flash Persistence Round Trip                                  */
/* *For any* flash data, writing to flash, saving to file, loading from      */
/* file, and reading should return the same data.                            */
/* **Validates: Requirements 4.9**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-platform-improvements, Property 7: Flash Persistence Round
 * Trip
 *
 * *For any* flash data, write-save-load-read should preserve the data.
 *
 * **Validates: Requirements 4.9**
 */
TEST_F(FlashPropertyTest, Property7_PersistenceRoundTrip) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random aligned address and length */
        uint32_t addr = randomAlignedAddress();
        size_t len = randomAlignedLength();

        /* Ensure address is valid */
        if (addr + len > flash->get_page_size(flash) * 128) {
            continue;
        }

        /* Generate random data */
        std::vector<uint8_t> write_data = randomData(len);

        /* Erase and write data */
        ASSERT_EQ(NX_OK, flash->erase(flash, addr, static_cast<uint32_t>(len)));
        ASSERT_EQ(NX_OK, flash->write(flash, addr, write_data.data(),
                                      static_cast<uint32_t>(len)));

        /* Save to file (happens automatically on deinit) */
        nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
        ASSERT_EQ(NX_OK, lifecycle->deinit(lifecycle));

        /* Reinitialize (loads from file) */
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
        ASSERT_EQ(NX_OK, flash->unlock(flash));

        /* Read data back */
        std::vector<uint8_t> read_data(len);
        ASSERT_EQ(NX_OK, flash->read(flash, addr, read_data.data(),
                                     static_cast<uint32_t>(len)));

        /* Verify data matches */
        EXPECT_EQ(write_data, read_data)
            << "Iteration " << test_iter
            << ": Persistence round trip failed at address " << addr;
    }
}

/**
 * Feature: native-platform-improvements, Property 7: Flash Persistence Round
 * Trip
 *
 * *For any* flash data, multiple write-save-load cycles should preserve data.
 *
 * **Validates: Requirements 4.9**
 */
TEST_F(FlashPropertyTest, Property7_MultiplePersistenceCycles) {
    /* Use fixed address for this test */
    uint32_t addr = 0;
    size_t len = flash->get_write_unit(flash) * 16;

    for (int test_iter = 0; test_iter < 10; ++test_iter) {
        /* Generate random data */
        std::vector<uint8_t> write_data = randomData(len);

        /* Erase and write data */
        ASSERT_EQ(NX_OK, flash->erase(flash, addr, static_cast<uint32_t>(len)));
        ASSERT_EQ(NX_OK, flash->write(flash, addr, write_data.data(),
                                      static_cast<uint32_t>(len)));

        /* Save and reload */
        nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
        ASSERT_EQ(NX_OK, lifecycle->deinit(lifecycle));
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
        ASSERT_EQ(NX_OK, flash->unlock(flash));

        /* Read data back */
        std::vector<uint8_t> read_data(len);
        ASSERT_EQ(NX_OK, flash->read(flash, addr, read_data.data(),
                                     static_cast<uint32_t>(len)));

        /* Verify data matches */
        EXPECT_EQ(write_data, read_data)
            << "Cycle " << test_iter << ": Data mismatch after persistence";
    }
}

/**
 * Feature: native-platform-improvements, Property 7: Flash Persistence Round
 * Trip
 *
 * *For any* flash data, reading after persistence should return the same data
 * as before persistence.
 *
 * **Validates: Requirements 4.9**
 */
TEST_F(FlashPropertyTest,
       Property7_ReadAfterPersistenceMatchesBeforePersistence) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random aligned address and length */
        uint32_t addr = randomAlignedAddress();
        size_t len = randomAlignedLength();

        /* Ensure address is valid */
        if (addr + len > flash->get_page_size(flash) * 128) {
            continue;
        }

        /* Generate random data */
        std::vector<uint8_t> write_data = randomData(len);

        /* Erase and write data */
        ASSERT_EQ(NX_OK, flash->erase(flash, addr, static_cast<uint32_t>(len)));
        ASSERT_EQ(NX_OK, flash->write(flash, addr, write_data.data(),
                                      static_cast<uint32_t>(len)));

        /* Read before persistence */
        std::vector<uint8_t> read_before(len);
        ASSERT_EQ(NX_OK, flash->read(flash, addr, read_before.data(),
                                     static_cast<uint32_t>(len)));

        /* Save and reload */
        nx_lifecycle_t* lifecycle = flash->get_lifecycle(flash);
        ASSERT_EQ(NX_OK, lifecycle->deinit(lifecycle));
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
        ASSERT_EQ(NX_OK, flash->unlock(flash));

        /* Read after persistence */
        std::vector<uint8_t> read_after(len);
        ASSERT_EQ(NX_OK, flash->read(flash, addr, read_after.data(),
                                     static_cast<uint32_t>(len)));

        /* Verify data matches before and after */
        EXPECT_EQ(read_before, read_after)
            << "Iteration " << test_iter
            << ": Data changed after persistence at address " << addr;
    }
}
