/**
 * \file            test_hal_i2c_properties.cpp
 * \brief           HAL I2C Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for I2C module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * **Validates: Requirements 4.4**
 */

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <random>

extern "C" {
#include "hal/hal_i2c.h"
#include "native_platform.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Maximum transfer size for tests
 */
static constexpr size_t MAX_TRANSFER_SIZE = 64;

/**
 * \brief           I2C Property Test Fixture
 */
class HalI2cPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        native_i2c_reset_all();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        native_i2c_reset_all();
    }

    hal_i2c_instance_t randomInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_I2C_MAX - 1);
        return static_cast<hal_i2c_instance_t>(dist(rng));
    }

    hal_i2c_speed_t randomSpeed() {
        std::uniform_int_distribution<int> dist(0, 2);
        return static_cast<hal_i2c_speed_t>(dist(rng));
    }

    uint16_t randomDeviceAddress() {
        std::uniform_int_distribution<int> dist(0x08,
                                                0x77);  // Valid 7-bit I2C range
        return static_cast<uint16_t>(dist(rng));
    }

    uint16_t randomMemoryAddress() {
        std::uniform_int_distribution<int> dist(0,
                                                255);  // Within buffer bounds
        return static_cast<uint16_t>(dist(rng));
    }

    size_t randomTransferSize() {
        std::uniform_int_distribution<size_t> dist(1, MAX_TRANSFER_SIZE);
        return dist(rng);
    }

    uint8_t randomByte() {
        std::uniform_int_distribution<int> dist(0, 255);
        return static_cast<uint8_t>(dist(rng));
    }

    void fillRandomData(uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            data[i] = randomByte();
        }
    }

    hal_i2c_config_t makeConfig(hal_i2c_speed_t speed) {
        return hal_i2c_config_t{
            .speed = speed, .addr_mode = HAL_I2C_ADDR_7BIT, .own_addr = 0x50};
    }
};

/**
 * Feature: phase2-core-platform, Property 9: I2C Protocol Compliance
 *
 * *For any* I2C master transmit operation, the sequence SHALL be:
 * START, ADDRESS+W, DATA bytes, STOP.
 *
 * **Validates: Requirements 4.4**
 */
TEST_F(HalI2cPropertyTest, Property9_I2cProtocolCompliance) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_i2c_reset_all();

        auto instance = randomInstance();
        auto speed = randomSpeed();
        auto dev_addr = randomDeviceAddress();
        auto transfer_len = randomTransferSize();

        hal_i2c_config_t config = makeConfig(speed);
        ASSERT_EQ(HAL_OK, hal_i2c_init(instance, &config))
            << "Iteration " << i << ": init failed for instance=" << instance
            << " speed=" << speed;

        // Add a simulated device
        ASSERT_TRUE(native_i2c_add_device(instance, dev_addr, true))
            << "Iteration " << i << ": failed to add device " << std::hex
            << dev_addr;

        // Generate random TX data
        uint8_t tx_data[MAX_TRANSFER_SIZE];
        fillRandomData(tx_data, transfer_len);

        // Perform master transmit
        ASSERT_EQ(HAL_OK, hal_i2c_master_transmit(instance, dev_addr, tx_data,
                                                  transfer_len, 1000))
            << "Iteration " << i
            << ": master_transmit failed for dev_addr=" << std::hex << dev_addr
            << " len=" << transfer_len;

        // Verify protocol compliance: the transaction should have recorded the
        // correct device address
        uint16_t recorded_dev_addr = native_i2c_get_last_dev_addr(instance);
        EXPECT_EQ(dev_addr, recorded_dev_addr)
            << "Iteration " << i << ": device address mismatch. "
            << "Expected=" << std::hex << dev_addr
            << " Got=" << recorded_dev_addr;

        // Verify the transmitted data was correctly recorded
        uint8_t read_back[MAX_TRANSFER_SIZE];
        size_t actual_len =
            native_i2c_get_last_tx_data(instance, read_back, sizeof(read_back));
        EXPECT_EQ(transfer_len, actual_len)
            << "Iteration " << i << ": TX length mismatch. "
            << "Expected=" << transfer_len << " Got=" << actual_len;

        EXPECT_EQ(0, memcmp(tx_data, read_back, transfer_len))
            << "Iteration " << i
            << ": TX data mismatch for len=" << transfer_len;

        hal_i2c_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 9 Extended: I2C Memory Write Protocol
 *
 * *For any* I2C memory write operation, the data SHALL be correctly written
 * to the specified memory address and be readable back.
 *
 * **Validates: Requirements 4.6, 4.7**
 */
TEST_F(HalI2cPropertyTest, Property9Extended_I2cMemoryWriteProtocol) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_i2c_reset_all();

        auto instance = randomInstance();
        auto speed = randomSpeed();
        auto dev_addr = randomDeviceAddress();
        auto mem_addr = randomMemoryAddress();
        auto transfer_len =
            std::min(randomTransferSize(),
                     size_t(256 - mem_addr));  // Stay within bounds

        hal_i2c_config_t config = makeConfig(speed);
        ASSERT_EQ(HAL_OK, hal_i2c_init(instance, &config))
            << "Iteration " << i << ": init failed";

        // Add a simulated device
        ASSERT_TRUE(native_i2c_add_device(instance, dev_addr, true))
            << "Iteration " << i << ": failed to add device";

        // Generate random data
        uint8_t write_data[MAX_TRANSFER_SIZE];
        fillRandomData(write_data, transfer_len);

        // Write to memory
        ASSERT_EQ(HAL_OK, hal_i2c_mem_write(instance, dev_addr, mem_addr, 1,
                                            write_data, transfer_len, 1000))
            << "Iteration " << i
            << ": mem_write failed for dev_addr=" << std::hex << dev_addr
            << " mem_addr=" << mem_addr << " len=" << transfer_len;

        // Verify the memory write recorded correct transaction details
        EXPECT_EQ(dev_addr, native_i2c_get_last_dev_addr(instance))
            << "Iteration " << i << ": device address mismatch in mem_write";
        EXPECT_EQ(mem_addr, native_i2c_get_last_mem_addr(instance))
            << "Iteration " << i << ": memory address mismatch in mem_write";

        // Read back from memory to verify write
        uint8_t read_data[MAX_TRANSFER_SIZE];
        ASSERT_EQ(HAL_OK, hal_i2c_mem_read(instance, dev_addr, mem_addr, 1,
                                           read_data, transfer_len, 1000))
            << "Iteration " << i << ": mem_read failed";

        // Verify data integrity (round-trip property)
        EXPECT_EQ(0, memcmp(write_data, read_data, transfer_len))
            << "Iteration " << i
            << ": memory round-trip data mismatch for len=" << transfer_len
            << " at mem_addr=" << mem_addr;

        hal_i2c_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 9 Extended: I2C Speed Configuration
 *
 * *For any* I2C speed configuration, the actual speed SHALL match the
 * expected frequency for that speed mode.
 *
 * **Validates: Requirements 4.2, 4.3**
 */
TEST_F(HalI2cPropertyTest, Property9Extended_I2cSpeedConfiguration) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_i2c_reset_all();

        auto instance = randomInstance();
        auto speed = randomSpeed();

        hal_i2c_config_t config = makeConfig(speed);
        ASSERT_EQ(HAL_OK, hal_i2c_init(instance, &config))
            << "Iteration " << i << ": init failed for speed=" << speed;

        // Verify the speed was correctly configured
        uint32_t actual_speed = native_i2c_get_actual_speed(instance);
        uint32_t expected_speed;

        switch (speed) {
            case HAL_I2C_SPEED_STANDARD:
                expected_speed = 100000;  // 100 kHz
                break;
            case HAL_I2C_SPEED_FAST:
                expected_speed = 400000;  // 400 kHz
                break;
            case HAL_I2C_SPEED_FAST_PLUS:
                expected_speed = 1000000;  // 1 MHz
                break;
            default:
                expected_speed = 100000;
                break;
        }

        EXPECT_EQ(expected_speed, actual_speed)
            << "Iteration " << i << ": speed mismatch for mode=" << speed
            << ". Expected=" << expected_speed << " Got=" << actual_speed;

        hal_i2c_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 9 Extended: I2C Device Ready Check
 *
 * *For any* I2C device, the ready check SHALL return HAL_OK if and only if
 * the device is present and ready to respond.
 *
 * **Validates: Requirements 4.8**
 */
TEST_F(HalI2cPropertyTest, Property9Extended_I2cDeviceReadyCheck) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_i2c_reset_all();

        auto instance = randomInstance();
        auto speed = randomSpeed();
        auto dev_addr = randomDeviceAddress();

        hal_i2c_config_t config = makeConfig(speed);
        ASSERT_EQ(HAL_OK, hal_i2c_init(instance, &config))
            << "Iteration " << i << ": init failed";

        // Initially no device should be present
        EXPECT_EQ(HAL_ERROR_TIMEOUT,
                  hal_i2c_is_device_ready(instance, dev_addr, 1, 10))
            << "Iteration " << i << ": device should not be ready initially";

        // Add device but not ready
        ASSERT_TRUE(native_i2c_add_device(instance, dev_addr, false))
            << "Iteration " << i << ": failed to add device";
        EXPECT_EQ(HAL_ERROR_TIMEOUT,
                  hal_i2c_is_device_ready(instance, dev_addr, 1, 10))
            << "Iteration " << i
            << ": device should not be ready when not ready flag set";

        // Set device ready
        ASSERT_TRUE(native_i2c_set_device_ready(instance, dev_addr, true))
            << "Iteration " << i << ": failed to set device ready";
        EXPECT_EQ(HAL_OK, hal_i2c_is_device_ready(instance, dev_addr, 1, 10))
            << "Iteration " << i
            << ": device should be ready when ready flag set";

        // Set device not ready again
        ASSERT_TRUE(native_i2c_set_device_ready(instance, dev_addr, false))
            << "Iteration " << i << ": failed to set device not ready";
        EXPECT_EQ(HAL_ERROR_TIMEOUT,
                  hal_i2c_is_device_ready(instance, dev_addr, 1, 10))
            << "Iteration " << i
            << ": device should not be ready after clearing ready flag";

        hal_i2c_deinit(instance);
    }
}