/**
 * \file            test_hal_i2c.cpp
 * \brief           HAL I2C Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for I2C module.
 * Tests speed configuration and read/write operations.
 * Requirements: 4.1, 4.2, 4.3
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "hal/hal_i2c.h"
#include "native_platform.h"
}

/**
 * \brief           I2C Test Fixture
 */
class HalI2cTest : public ::testing::Test {
protected:
    void SetUp() override {
        native_i2c_reset_all();
    }

    void TearDown() override {
        native_i2c_reset_all();
    }

    hal_i2c_config_t makeDefaultConfig() {
        return hal_i2c_config_t{
            .speed      = HAL_I2C_SPEED_STANDARD,
            .addr_mode  = HAL_I2C_ADDR_7BIT,
            .own_addr   = 0x50
        };
    }
};

/**
 * \brief           Test I2C initialization with valid config
 * \details         Requirements 4.1 - init with valid config returns HAL_OK
 */
TEST_F(HalI2cTest, InitWithValidConfig) {
    hal_i2c_config_t config = makeDefaultConfig();

    EXPECT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));
    EXPECT_TRUE(native_i2c_is_initialized(HAL_I2C_0));
}

/**
 * \brief           Test I2C initialization with standard speed
 * \details         Requirements 4.2 - standard speed configures 100 kHz
 */
TEST_F(HalI2cTest, InitStandardSpeed) {
    hal_i2c_config_t config = makeDefaultConfig();
    config.speed = HAL_I2C_SPEED_STANDARD;

    EXPECT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));
    EXPECT_EQ(100000U, native_i2c_get_actual_speed(HAL_I2C_0));
}

/**
 * \brief           Test I2C initialization with fast speed
 * \details         Requirements 4.3 - fast speed configures 400 kHz
 */
TEST_F(HalI2cTest, InitFastSpeed) {
    hal_i2c_config_t config = makeDefaultConfig();
    config.speed = HAL_I2C_SPEED_FAST;

    EXPECT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));
    EXPECT_EQ(400000U, native_i2c_get_actual_speed(HAL_I2C_0));
}

/**
 * \brief           Test I2C initialization with fast plus speed
 */
TEST_F(HalI2cTest, InitFastPlusSpeed) {
    hal_i2c_config_t config = makeDefaultConfig();
    config.speed = HAL_I2C_SPEED_FAST_PLUS;

    EXPECT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));
    EXPECT_EQ(1000000U, native_i2c_get_actual_speed(HAL_I2C_0));
}

/**
 * \brief           Test I2C initialization with invalid parameters
 */
TEST_F(HalI2cTest, InitInvalidParams) {
    hal_i2c_config_t config = makeDefaultConfig();

    // Invalid instance
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_i2c_init(HAL_I2C_MAX, &config));

    // Null config
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_i2c_init(HAL_I2C_0, nullptr));

    // Invalid speed
    config.speed = static_cast<hal_i2c_speed_t>(99);
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_i2c_init(HAL_I2C_0, &config));
}

/**
 * \brief           Test I2C deinitialization
 */
TEST_F(HalI2cTest, Deinit) {
    hal_i2c_config_t config = makeDefaultConfig();

    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));
    EXPECT_TRUE(native_i2c_is_initialized(HAL_I2C_0));

    EXPECT_EQ(HAL_OK, hal_i2c_deinit(HAL_I2C_0));
    EXPECT_FALSE(native_i2c_is_initialized(HAL_I2C_0));
}

/**
 * \brief           Test I2C master transmit
 * \details         Requirements 4.4 - master transmit sends data to device
 */
TEST_F(HalI2cTest, MasterTransmit) {
    hal_i2c_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));

    // Add a device to simulate
    uint16_t dev_addr = 0x48;
    ASSERT_TRUE(native_i2c_add_device(HAL_I2C_0, dev_addr, true));

    uint8_t tx_data[] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(HAL_OK, hal_i2c_master_transmit(HAL_I2C_0, dev_addr, tx_data, sizeof(tx_data), 1000));

    // Verify transmitted data
    uint8_t read_back[4];
    size_t len = native_i2c_get_last_tx_data(HAL_I2C_0, read_back, sizeof(read_back));
    EXPECT_EQ(sizeof(tx_data), len);
    EXPECT_EQ(0, memcmp(tx_data, read_back, sizeof(tx_data)));
    EXPECT_EQ(dev_addr, native_i2c_get_last_dev_addr(HAL_I2C_0));
}

/**
 * \brief           Test I2C master transmit to non-existent device
 */
TEST_F(HalI2cTest, MasterTransmitNoDevice) {
    hal_i2c_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));

    uint8_t tx_data[] = {0x01, 0x02};
    EXPECT_EQ(HAL_ERROR_TIMEOUT, hal_i2c_master_transmit(HAL_I2C_0, 0x48, tx_data, sizeof(tx_data), 1000));
}

/**
 * \brief           Test I2C master transmit on uninitialized instance
 */
TEST_F(HalI2cTest, MasterTransmitNotInit) {
    uint8_t tx_data[] = {0x01, 0x02};
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_i2c_master_transmit(HAL_I2C_0, 0x48, tx_data, sizeof(tx_data), 1000));
}

/**
 * \brief           Test I2C master receive
 * \details         Requirements 4.5 - master receive gets data from device
 */
TEST_F(HalI2cTest, MasterReceive) {
    hal_i2c_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));

    // Add a device and pre-fill its memory
    uint16_t dev_addr = 0x48;
    ASSERT_TRUE(native_i2c_add_device(HAL_I2C_0, dev_addr, true));
    
    uint8_t device_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    ASSERT_TRUE(native_i2c_write_device_memory(HAL_I2C_0, dev_addr, 0, device_data, sizeof(device_data)));

    // Receive data
    uint8_t rx_data[4];
    EXPECT_EQ(HAL_OK, hal_i2c_master_receive(HAL_I2C_0, dev_addr, rx_data, sizeof(rx_data), 1000));
    EXPECT_EQ(0, memcmp(device_data, rx_data, sizeof(device_data)));
    EXPECT_EQ(dev_addr, native_i2c_get_last_dev_addr(HAL_I2C_0));
}

/**
 * \brief           Test I2C master receive from non-existent device
 */
TEST_F(HalI2cTest, MasterReceiveNoDevice) {
    hal_i2c_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));

    uint8_t rx_data[4];
    EXPECT_EQ(HAL_ERROR_TIMEOUT, hal_i2c_master_receive(HAL_I2C_0, 0x48, rx_data, sizeof(rx_data), 1000));
}

/**
 * \brief           Test I2C memory write
 * \details         Requirements 4.6 - memory write to specific address
 */
TEST_F(HalI2cTest, MemoryWrite) {
    hal_i2c_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));

    // Add a device
    uint16_t dev_addr = 0x50;
    uint16_t mem_addr = 0x10;
    ASSERT_TRUE(native_i2c_add_device(HAL_I2C_0, dev_addr, true));

    uint8_t write_data[] = {0x11, 0x22, 0x33, 0x44};
    EXPECT_EQ(HAL_OK, hal_i2c_mem_write(HAL_I2C_0, dev_addr, mem_addr, 1, write_data, sizeof(write_data), 1000));

    // Verify data was written to device memory
    uint8_t read_back[4];
    ASSERT_TRUE(native_i2c_read_device_memory(HAL_I2C_0, dev_addr, mem_addr, read_back, sizeof(read_back)));
    EXPECT_EQ(0, memcmp(write_data, read_back, sizeof(write_data)));
    
    // Verify transaction details
    EXPECT_EQ(dev_addr, native_i2c_get_last_dev_addr(HAL_I2C_0));
    EXPECT_EQ(mem_addr, native_i2c_get_last_mem_addr(HAL_I2C_0));
}

/**
 * \brief           Test I2C memory read
 * \details         Requirements 4.7 - memory read from specific address
 */
TEST_F(HalI2cTest, MemoryRead) {
    hal_i2c_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));

    // Add a device and pre-fill memory
    uint16_t dev_addr = 0x50;
    uint16_t mem_addr = 0x20;
    ASSERT_TRUE(native_i2c_add_device(HAL_I2C_0, dev_addr, true));
    
    uint8_t device_data[] = {0x55, 0x66, 0x77, 0x88};
    ASSERT_TRUE(native_i2c_write_device_memory(HAL_I2C_0, dev_addr, mem_addr, device_data, sizeof(device_data)));

    // Read from memory
    uint8_t rx_data[4];
    EXPECT_EQ(HAL_OK, hal_i2c_mem_read(HAL_I2C_0, dev_addr, mem_addr, 1, rx_data, sizeof(rx_data), 1000));
    EXPECT_EQ(0, memcmp(device_data, rx_data, sizeof(device_data)));
    
    // Verify transaction details
    EXPECT_EQ(dev_addr, native_i2c_get_last_dev_addr(HAL_I2C_0));
    EXPECT_EQ(mem_addr, native_i2c_get_last_mem_addr(HAL_I2C_0));
}

/**
 * \brief           Test I2C memory operations with 2-byte address
 */
TEST_F(HalI2cTest, MemoryOperations2ByteAddr) {
    hal_i2c_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));

    uint16_t dev_addr = 0x50;
    uint16_t mem_addr = 0x00F0;  // Use smaller address within buffer bounds
    ASSERT_TRUE(native_i2c_add_device(HAL_I2C_0, dev_addr, true));

    uint8_t write_data[] = {0xAB, 0xCD};
    
    // Write with 2-byte memory address
    EXPECT_EQ(HAL_OK, hal_i2c_mem_write(HAL_I2C_0, dev_addr, mem_addr, 2, write_data, sizeof(write_data), 1000));
    
    // Read back with 2-byte memory address
    uint8_t rx_data[2];
    EXPECT_EQ(HAL_OK, hal_i2c_mem_read(HAL_I2C_0, dev_addr, mem_addr, 2, rx_data, sizeof(rx_data), 1000));
    EXPECT_EQ(0, memcmp(write_data, rx_data, sizeof(write_data)));
}

/**
 * \brief           Test I2C device ready check
 */
TEST_F(HalI2cTest, IsDeviceReady) {
    hal_i2c_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));

    uint16_t dev_addr = 0x48;
    
    // Device not present
    EXPECT_EQ(HAL_ERROR_TIMEOUT, hal_i2c_is_device_ready(HAL_I2C_0, dev_addr, 3, 100));
    
    // Add device but not ready
    ASSERT_TRUE(native_i2c_add_device(HAL_I2C_0, dev_addr, false));
    EXPECT_EQ(HAL_ERROR_TIMEOUT, hal_i2c_is_device_ready(HAL_I2C_0, dev_addr, 3, 100));
    
    // Set device ready
    ASSERT_TRUE(native_i2c_set_device_ready(HAL_I2C_0, dev_addr, true));
    EXPECT_EQ(HAL_OK, hal_i2c_is_device_ready(HAL_I2C_0, dev_addr, 3, 100));
}

/**
 * \brief           Test I2C operations with invalid parameters
 */
TEST_F(HalI2cTest, InvalidParameters) {
    hal_i2c_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));

    uint8_t data[4];
    
    // Invalid instance
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_i2c_master_transmit(HAL_I2C_MAX, 0x48, data, sizeof(data), 1000));
    
    // Null pointer
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_i2c_master_transmit(HAL_I2C_0, 0x48, nullptr, sizeof(data), 1000));
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_i2c_master_receive(HAL_I2C_0, 0x48, nullptr, sizeof(data), 1000));
    
    // Zero length
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_i2c_master_transmit(HAL_I2C_0, 0x48, data, 0, 1000));
    
    // Invalid memory address size
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_i2c_mem_write(HAL_I2C_0, 0x48, 0x10, 3, data, sizeof(data), 1000));
}

/**
 * \brief           Test multiple I2C instances
 */
TEST_F(HalI2cTest, MultipleInstances) {
    hal_i2c_config_t config0 = makeDefaultConfig();
    config0.speed = HAL_I2C_SPEED_STANDARD;

    hal_i2c_config_t config1 = makeDefaultConfig();
    config1.speed = HAL_I2C_SPEED_FAST;

    EXPECT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config0));
    EXPECT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_1, &config1));

    EXPECT_TRUE(native_i2c_is_initialized(HAL_I2C_0));
    EXPECT_TRUE(native_i2c_is_initialized(HAL_I2C_1));

    EXPECT_EQ(100000U, native_i2c_get_actual_speed(HAL_I2C_0));
    EXPECT_EQ(400000U, native_i2c_get_actual_speed(HAL_I2C_1));
}

/**
 * \brief           Test I2C callback registration
 */
TEST_F(HalI2cTest, CallbackRegistration) {
    hal_i2c_config_t config = makeDefaultConfig();
    ASSERT_EQ(HAL_OK, hal_i2c_init(HAL_I2C_0, &config));

    bool callback_called = false;
    auto callback = [](hal_i2c_instance_t instance, uint32_t event, void* context) {
        (void)instance;
        (void)event;
        bool* flag = static_cast<bool*>(context);
        *flag = true;
    };

    EXPECT_EQ(HAL_OK, hal_i2c_set_callback(HAL_I2C_0, callback, &callback_called));

    // Add device and perform operation to trigger callback
    uint16_t dev_addr = 0x48;
    ASSERT_TRUE(native_i2c_add_device(HAL_I2C_0, dev_addr, true));
    
    uint8_t tx_data[] = {0x01, 0x02};
    EXPECT_EQ(HAL_OK, hal_i2c_master_transmit(HAL_I2C_0, dev_addr, tx_data, sizeof(tx_data), 1000));
    
    EXPECT_TRUE(callback_called);
}