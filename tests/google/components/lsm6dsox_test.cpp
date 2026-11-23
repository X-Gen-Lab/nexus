/**
 * \file            lsm6dsox_test.cpp
 * \brief           Official LSM6DSOX selected-profile and transport contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/lsm6dsox.h"
#include "nexus/components/lsm6dsox_i2c.h"
#include "nexus/components/lsm6dsox_spi.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>

namespace {
struct Sensor {
    std::array<uint8_t, 256> registers = {};
    nx_time_us_t now = 100;
    size_t writes = 0;
    int reset_reads = 0;
    int failed_register = -1;
    int ignored_register = -1;
    bool fail_write = false;

    Sensor() {
        registers[0x0f] = 0x6c;
    }
    static nx_time_us_t clock(void* context) {
        return static_cast<Sensor*>(context)->now;
    }
    static nx_result_t read(void* context, uint8_t reg, uint8_t* data,
                            size_t bytes, nx_time_us_t deadline) {
        auto& self = *static_cast<Sensor*>(context);
        if (self.now >= deadline) {
            return NX_ERROR_TIMEOUT;
        }
        if (reg == self.failed_register) {
            return NX_ERROR_IO;
        }
        if (data == nullptr || bytes == 0 || bytes > 256U - reg) {
            return NX_ERROR_INVALID;
        }
        std::memcpy(data, self.registers.data() + reg, bytes);
        if (reg == 0x12 && self.reset_reads > 0) {
            --self.reset_reads;
            if (self.reset_reads == 0) {
                self.registers[0x12] = 0;
            }
        }
        return NX_SUCCESS;
    }
    static nx_result_t write(void* context, uint8_t reg, const uint8_t* data,
                             size_t bytes, nx_time_us_t deadline) {
        auto& self = *static_cast<Sensor*>(context);
        if (self.now >= deadline) {
            return NX_ERROR_TIMEOUT;
        }
        if (data == nullptr || bytes == 0 || bytes > 256U - reg) {
            return NX_ERROR_INVALID;
        }
        ++self.writes;
        if (reg == self.ignored_register) {
            return NX_SUCCESS;
        }
        std::memcpy(self.registers.data() + reg, data, bytes);
        if (reg == 0x12 && (data[0] & 1U) != 0) {
            self.registers.fill(0);
            self.registers[0x0f] = 0x6c;
            self.registers[0x12] = 1;
            self.reset_reads = 1;
        }
        return self.fail_write ? NX_ERROR_IO : NX_SUCCESS;
    }
    nx_lsm6dsox_transport_port_t port() {
        return {this, read, write, {clock, this}};
    }
    void word(uint8_t address, int16_t value) {
        const auto raw = static_cast<uint16_t>(value);
        registers[address] = static_cast<uint8_t>(raw);
        registers[static_cast<size_t>(address) + 1U] =
            static_cast<uint8_t>(raw >> 8U);
    }
};

void start(Sensor& sensor, nx_lsm6dsox_t& device,
           nx_lsm6dsox_mode_t mode = NX_LSM6DSOX_DIRECT) {
    const nx_lsm6dsox_config_t config = {1, 2, mode};
    ASSERT_EQ(nx_lsm6dsox_init(&device, sensor.port(), &config, 10000),
              NX_SUCCESS);
    ASSERT_EQ(nx_lsm6dsox_reset_begin(&device, 10000), NX_SUCCESS);
    ASSERT_EQ(nx_lsm6dsox_reset_poll(&device), NX_ERROR_BUSY);
    ASSERT_EQ(nx_lsm6dsox_reset_poll(&device), NX_SUCCESS);
    ASSERT_EQ(nx_lsm6dsox_start(&device, 10000), NX_SUCCESS);
}

TEST(Lsm6dsox, IdentityRejectsWithoutWritesAndDoesNotBind) {
    Sensor sensor;
    sensor.registers[0x0f] = 0;
    nx_lsm6dsox_t device = {};
    const nx_lsm6dsox_config_t config = {1, 2, NX_LSM6DSOX_DIRECT};
    EXPECT_EQ(nx_lsm6dsox_init(&device, sensor.port(), &config, 10000),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(sensor.writes, 0U);
    EXPECT_FALSE(device.initialized);
    EXPECT_EQ(device.transport.context, nullptr);
}

TEST(Lsm6dsox, ExplicitResetStartAndFreshDirectSample) {
    Sensor sensor;
    nx_lsm6dsox_t device = {};
    start(sensor, device);
    EXPECT_EQ(sensor.registers[0x10] & 0xfcU, 0x48U);
    EXPECT_EQ(sensor.registers[0x11] & 0xfeU, 0x44U);
    EXPECT_EQ(sensor.registers[0x12] & 0x44U, 0x44U);
    sensor.registers[0x1e] = 3;
    sensor.word(0x22, 1000);
    sensor.word(0x24, -1000);
    sensor.word(0x26, 0);
    sensor.word(0x28, 1000);
    sensor.word(0x2a, -1000);
    sensor.word(0x2c, 0);
    nx_lsm6dsox_sample_t sample = {};
    ASSERT_EQ(nx_lsm6dsox_read(&device, &sample, 10000), NX_SUCCESS);
    EXPECT_NEAR(sample.acceleration_m_s2[0], 1.1964113F, 1e-6F);
    EXPECT_NEAR(sample.acceleration_m_s2[1], -1.1964113F, 1e-6F);
    EXPECT_NEAR(sample.angular_rate_rad_s[0], 0.30543262F, 1e-6F);
    EXPECT_EQ(sample.sequence, 1U);
    EXPECT_EQ(sample.generation, 1U);
    EXPECT_EQ(sample.observed.source, NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    EXPECT_EQ(sample.observed.domain, 1U);
    EXPECT_EQ(sample.observed.epoch, 2U);
    EXPECT_FALSE(sample.observed.uncertainty_known);
    const auto before = sample;
    sensor.registers[0x1e] = 0;
    EXPECT_EQ(nx_lsm6dsox_read(&device, &sample, 10000), NX_ERROR_BUSY);
    EXPECT_EQ(std::memcmp(&sample, &before, sizeof(sample)), 0);
    EXPECT_EQ(device.sequence, 1U);
}

TEST(Lsm6dsox, ReadFailureClosesPublicationAndStopProvesOdrOff) {
    Sensor sensor;
    nx_lsm6dsox_t device = {};
    start(sensor, device);
    sensor.registers[0x1e] = 3;
    sensor.failed_register = 0x28;
    nx_lsm6dsox_sample_t sample = {};
    sample.sequence = 91;
    EXPECT_EQ(nx_lsm6dsox_read(&device, &sample, 10000), NX_ERROR_IO);
    EXPECT_EQ(sample.sequence, 91U);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(nx_lsm6dsox_read(&device, &sample, 10000), NX_ERROR_STATE);
    sensor.failed_register = -1;
    ASSERT_EQ(nx_lsm6dsox_stop(&device, 10000), NX_SUCCESS);
    EXPECT_EQ(sensor.registers[0x10] & 0xf0U, 0U);
    EXPECT_EQ(sensor.registers[0x11] & 0xf0U, 0U);
    EXPECT_FALSE(device.uncertain);
    EXPECT_EQ(nx_lsm6dsox_start(&device, 10000), NX_ERROR_STATE);
}

TEST(Lsm6dsox, FailedResetWriteAndClockRollbackRetainRecoveryDuty) {
    Sensor sensor;
    nx_lsm6dsox_t device = {};
    const nx_lsm6dsox_config_t config = {1, 2, NX_LSM6DSOX_DIRECT};
    ASSERT_EQ(nx_lsm6dsox_init(&device, sensor.port(), &config, 10000),
              NX_SUCCESS);
    sensor.fail_write = true;
    EXPECT_EQ(nx_lsm6dsox_reset_begin(&device, 10000), NX_ERROR_IO);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(device.transport.context, &sensor);
    sensor.fail_write = false;
    sensor.now = 99;
    EXPECT_EQ(nx_lsm6dsox_stop(&device, 10000), NX_ERROR_IO);
    EXPECT_TRUE(device.time_fault);
    sensor.now = 101;
    EXPECT_EQ(nx_lsm6dsox_stop(&device, 10000), NX_ERROR_STATE);
}

TEST(Lsm6dsox, TaggedFifoDoesNotInventPairsAndOverflowIsSticky) {
    Sensor sensor;
    nx_lsm6dsox_t device = {};
    start(sensor, device, NX_LSM6DSOX_TAGGED_FIFO);
    sensor.registers[0x3a] = 1;
    sensor.registers[0x78] = 2U << 3U;
    sensor.word(0x79, 1000);
    nx_lsm6dsox_fifo_sample_t sample = {};
    ASSERT_EQ(nx_lsm6dsox_fifo_pop(&device, &sample, 10000), NX_SUCCESS);
    EXPECT_EQ(sample.kind, NX_LSM6DSOX_ACCELERATION);
    EXPECT_EQ(sample.raw[0], 1000);
    EXPECT_NEAR(sample.values[0], 1.1964113F, 1e-6F);
    sensor.registers[0x78] = 4U << 3U;
    sensor.registers[0x79] = 0x78;
    sensor.registers[0x7a] = 0x56;
    sensor.registers[0x7b] = 0x34;
    sensor.registers[0x7c] = 0x12;
    ASSERT_EQ(nx_lsm6dsox_fifo_pop(&device, &sample, 10000), NX_SUCCESS);
    EXPECT_EQ(sample.kind, NX_LSM6DSOX_SENSOR_TIMESTAMP);
    EXPECT_EQ(sample.sensor_ticks, 0x12345678U);
    EXPECT_EQ(sample.sequence, 2U);
    const auto before = sample;
    sensor.registers[0x3b] = 0x40;
    EXPECT_EQ(nx_lsm6dsox_fifo_pop(&device, &sample, 10000), NX_ERROR_OVERFLOW);
    EXPECT_EQ(std::memcmp(&sample, &before, sizeof(sample)), 0);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(nx_lsm6dsox_fifo_pop(&device, &sample, 10000), NX_ERROR_STATE);
}

TEST(Lsm6dsox, ExpiredResetBudgetAndFailedStopRemainExplicit) {
    Sensor sensor;
    nx_lsm6dsox_t device = {};
    start(sensor, device);
    sensor.fail_write = true;
    EXPECT_EQ(nx_lsm6dsox_stop(&device, 10000), NX_ERROR_IO);
    EXPECT_TRUE(device.uncertain);
    sensor.fail_write = false;
    EXPECT_EQ(nx_lsm6dsox_stop(&device, 10000), NX_SUCCESS);
    ASSERT_EQ(nx_lsm6dsox_reset_begin(&device, 200), NX_SUCCESS);
    sensor.now = 200;
    EXPECT_EQ(nx_lsm6dsox_reset_poll(&device), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(device.uncertain);
    EXPECT_FALSE(device.prepared);
}

TEST(Lsm6dsox, FifoBatchReadbackRejectsAnAcknowledgedIgnoredWrite) {
    Sensor sensor;
    nx_lsm6dsox_t device = {};
    const nx_lsm6dsox_config_t config = {1, 2, NX_LSM6DSOX_TAGGED_FIFO};
    ASSERT_EQ(nx_lsm6dsox_init(&device, sensor.port(), &config, 10000),
              NX_SUCCESS);
    ASSERT_EQ(nx_lsm6dsox_reset_begin(&device, 10000), NX_SUCCESS);
    ASSERT_EQ(nx_lsm6dsox_reset_poll(&device), NX_ERROR_BUSY);
    ASSERT_EQ(nx_lsm6dsox_reset_poll(&device), NX_SUCCESS);
    sensor.ignored_register = 0x09;
    EXPECT_EQ(nx_lsm6dsox_start(&device, 10000), NX_ERROR_IO);
    EXPECT_FALSE(device.running);
    EXPECT_TRUE(device.uncertain);
}

struct RegisterBus {
    Sensor sensor;
    bool short_transfer = false;
    size_t calls = 0;
    uint8_t command = 0;
    nx_time_us_t deadline = 0;

    static nx_result_t i2c(void* context, nx_i2c_message_t* messages,
                           size_t count, nx_time_us_t limit,
                           size_t* transferred) {
        auto& self = *static_cast<RegisterBus*>(context);
        ++self.calls;
        self.deadline = limit;
        EXPECT_FALSE(messages[0].read);
        self.command = messages[0].buffer.tx[0];
        if (count == 2 && messages[0].length == 1 && messages[1].read) {
            const nx_result_t result =
                Sensor::read(&self.sensor, self.command, messages[1].buffer.rx,
                             messages[1].length, limit);
            *transferred = messages[1].length + 1;
            if (self.short_transfer) {
                --*transferred;
            }
            return result;
        }
        if (count == 1 && messages[0].length > 1) {
            *transferred = messages[0].length;
            return Sensor::write(&self.sensor, self.command,
                                 messages[0].buffer.tx + 1,
                                 messages[0].length - 1, limit);
        }
        return NX_ERROR_INVALID;
    }
    static nx_result_t spi(void* context, const uint8_t* tx, uint8_t* rx,
                           size_t bytes, nx_time_us_t limit,
                           size_t* transferred) {
        auto& self = *static_cast<RegisterBus*>(context);
        ++self.calls;
        self.deadline = limit;
        self.command = tx[0];
        *transferred = bytes - (self.short_transfer ? 1U : 0U);
        if ((tx[0] & 0x80U) != 0 && rx != nullptr && bytes > 1) {
            return Sensor::read(&self.sensor, tx[0] & 0x7fU, rx + 1, bytes - 1,
                                limit);
        }
        return bytes > 1 ? Sensor::write(&self.sensor, tx[0], tx + 1, bytes - 1,
                                         limit)
                         : NX_ERROR_INVALID;
    }
};

TEST(Lsm6dsox, I2cTransportUsesOneRepeatedStartAndChecksCompleteProgress) {
    RegisterBus bus;
    const nx_i2c_endpoint_ops_t ops = {RegisterBus::i2c, nullptr, nullptr,
                                       nullptr};
    const nx_i2c_endpoint_t endpoint = {&ops, &bus};
    const auto transport =
        nx_lsm6dsox_i2c_transport(&endpoint, {Sensor::clock, &bus.sensor});
    uint8_t value = 0;
    ASSERT_EQ(transport.read(transport.context, 0x0f, &value, 1, 500),
              NX_SUCCESS);
    EXPECT_EQ(value, 0x6c);
    EXPECT_EQ(bus.calls, 1U);
    EXPECT_EQ(bus.deadline, 500U);
    value = 0x48;
    ASSERT_EQ(transport.write(transport.context, 0x10, &value, 1, 500),
              NX_SUCCESS);
    EXPECT_EQ(bus.sensor.registers[0x10], 0x48);
    bus.short_transfer = true;
    EXPECT_EQ(transport.read(transport.context, 0x0f, &value, 1, 500),
              NX_ERROR_IO);
    EXPECT_EQ(nx_lsm6dsox_i2c_transport(nullptr, {}).read, nullptr);
}

TEST(Lsm6dsox, SpiTransportKeepsCommandAndPayloadInOneSettledTransfer) {
    RegisterBus bus;
    const nx_spi_endpoint_ops_t ops = {RegisterBus::spi, nullptr, nullptr,
                                       nullptr, nullptr};
    const nx_spi_endpoint_t endpoint = {&ops, &bus};
    const auto transport =
        nx_lsm6dsox_spi_transport(&endpoint, {Sensor::clock, &bus.sensor});
    uint8_t value = 0;
    ASSERT_EQ(transport.read(transport.context, 0x0f, &value, 1, 500),
              NX_SUCCESS);
    EXPECT_EQ(value, 0x6c);
    EXPECT_EQ(bus.command, 0x8f);
    EXPECT_EQ(bus.calls, 1U);
    value = 0x44;
    ASSERT_EQ(transport.write(transport.context, 0x11, &value, 1, 500),
              NX_SUCCESS);
    EXPECT_EQ(bus.command, 0x11);
    EXPECT_EQ(bus.sensor.registers[0x11], 0x44);
    bus.short_transfer = true;
    EXPECT_EQ(transport.read(transport.context, 0x0f, &value, 1, 500),
              NX_ERROR_IO);
    EXPECT_EQ(value, 0x44);
    EXPECT_EQ(nx_lsm6dsox_spi_transport(nullptr, {}).read, nullptr);
}
} /* namespace */
