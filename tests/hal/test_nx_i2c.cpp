/**
 * \file            test_nx_i2c.cpp
 * \brief           Nexus I2C Interface Tests
 * \author          Nexus Team
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/base/nx_device.h"
#include "hal/interface/nx_i2c.h"
}

/* Forward declaration of native I2C factory function */
extern "C" nx_i2c_t* nx_i2c_native_get(uint8_t index);

/**
 * \brief           I2C Test Fixture
 */
class NxI2cTest : public ::testing::Test {
  protected:
    nx_i2c_t* i2c;

    void SetUp() override {
        i2c = nx_i2c_native_get(0);
        ASSERT_NE(nullptr, i2c);

        /* Initialize I2C */
        nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
        ASSERT_NE(nullptr, lifecycle);
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        if (i2c) {
            nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
            if (lifecycle) {
                lifecycle->deinit(lifecycle);
            }
        }
    }
};

/**
 * \brief           Test I2C initialization
 */
TEST_F(NxI2cTest, Initialization) {
    nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
    ASSERT_NE(nullptr, lifecycle);

    nx_device_state_t state = lifecycle->get_state(lifecycle);
    EXPECT_EQ(NX_DEV_STATE_RUNNING, state);
}

/**
 * \brief           Test I2C master transmit
 */
TEST_F(NxI2cTest, MasterTransmit) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    nx_status_t status =
        i2c->master_transmit(i2c, 0x50, data, sizeof(data), 100);
    EXPECT_EQ(NX_OK, status);
}

/**
 * \brief           Test I2C master receive
 */
TEST_F(NxI2cTest, MasterReceive) {
    uint8_t data[4] = {0};
    nx_status_t status =
        i2c->master_receive(i2c, 0x50, data, sizeof(data), 100);
    EXPECT_EQ(NX_OK, status);
}

/**
 * \brief           Test I2C memory write
 */
TEST_F(NxI2cTest, MemoryWrite) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    nx_status_t status =
        i2c->mem_write(i2c, 0x50, 0x0010, 2, data, sizeof(data), 100);
    EXPECT_EQ(NX_OK, status);
}

/**
 * \brief           Test I2C memory read
 */
TEST_F(NxI2cTest, MemoryRead) {
    uint8_t data[4] = {0};
    nx_status_t status =
        i2c->mem_read(i2c, 0x50, 0x0020, 2, data, sizeof(data), 100);
    EXPECT_EQ(NX_OK, status);
}

/**
 * \brief           Test I2C probe
 */
TEST_F(NxI2cTest, Probe) {
    /* Probe existing device (simulated) */
    nx_status_t status = i2c->probe(i2c, 0x50, 100);
    EXPECT_EQ(NX_OK, status);

    /* Probe non-existing device */
    status = i2c->probe(i2c, 0x20, 100);
    EXPECT_NE(NX_OK, status);
}

/**
 * \brief           Test I2C scan
 */
TEST_F(NxI2cTest, Scan) {
    uint8_t addr_list[10];
    size_t found = 0;

    nx_status_t status = i2c->scan(i2c, addr_list, 10, &found);
    EXPECT_EQ(NX_OK, status);
    EXPECT_GT(found, 0);
}

/**
 * \brief           Test I2C speed configuration
 */
TEST_F(NxI2cTest, SetSpeed) {
    nx_status_t status = i2c->set_speed(i2c, NX_I2C_SPEED_FAST);
    EXPECT_EQ(NX_OK, status);

    nx_i2c_config_t config;
    status = i2c->get_config(i2c, &config);
    EXPECT_EQ(NX_OK, status);
    EXPECT_EQ(NX_I2C_SPEED_FAST, config.speed);
}

/**
 * \brief           Test I2C configuration get/set
 */
TEST_F(NxI2cTest, ConfigGetSet) {
    nx_i2c_config_t config;
    nx_status_t status = i2c->get_config(i2c, &config);
    EXPECT_EQ(NX_OK, status);

    config.speed = NX_I2C_SPEED_FAST_PLUS;
    config.own_addr = 0x42;

    status = i2c->set_config(i2c, &config);
    EXPECT_EQ(NX_OK, status);

    nx_i2c_config_t new_config;
    status = i2c->get_config(i2c, &new_config);
    EXPECT_EQ(NX_OK, status);
    EXPECT_EQ(NX_I2C_SPEED_FAST_PLUS, new_config.speed);
    EXPECT_EQ(0x42, new_config.own_addr);
}

/**
 * \brief           Test I2C statistics
 */
TEST_F(NxI2cTest, Statistics) {
    uint8_t data[] = {0x01, 0x02};
    i2c->master_transmit(i2c, 0x50, data, sizeof(data), 100);

    nx_i2c_stats_t stats;
    nx_status_t status = i2c->get_stats(i2c, &stats);
    EXPECT_EQ(NX_OK, status);
    EXPECT_EQ(2, stats.tx_count);
}

/**
 * \brief           Test I2C lifecycle suspend/resume
 */
TEST_F(NxI2cTest, SuspendResume) {
    nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
    ASSERT_NE(nullptr, lifecycle);

    nx_status_t status = lifecycle->suspend(lifecycle);
    EXPECT_EQ(NX_OK, status);
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    status = lifecycle->resume(lifecycle);
    EXPECT_EQ(NX_OK, status);
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));
}

/**
 * \brief           Test I2C power management
 */
TEST_F(NxI2cTest, PowerManagement) {
    nx_power_t* power = i2c->get_power(i2c);
    ASSERT_NE(nullptr, power);

    EXPECT_TRUE(power->is_enabled(power));

    nx_status_t status = power->disable(power);
    EXPECT_EQ(NX_OK, status);
    EXPECT_FALSE(power->is_enabled(power));

    status = power->enable(power);
    EXPECT_EQ(NX_OK, status);
    EXPECT_TRUE(power->is_enabled(power));
}
