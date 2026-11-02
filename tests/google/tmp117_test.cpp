/**
 * \file            tmp117_test.cpp
 * \brief           Official TMP117 protocol, settled lifetime and owner tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/bus_owner.h"
#include "nexus/components/tmp117.h"
#include "nexus/components/tmp117_i2c.h"
#include <array>
#include <cstring>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using testing::_;
using testing::Invoke;
using testing::StrictMock;

/** \brief Model only external settled register exchanges, not the driver. */
class Bus {
  public:
    MOCK_METHOD(nx_result_t, Exchange,
                (const uint8_t*, size_t, uint8_t*, size_t, nx_time_us_t));
    testing::Sequence order;
    nx_time_us_t now{};
    unsigned clock_reads{};
    static nx_result_t exchange(void* context, const uint8_t* tx,
                                size_t tx_bytes, uint8_t* rx, size_t rx_bytes,
                                nx_time_us_t deadline) {
        return static_cast<Bus*>(context)->Exchange(tx, tx_bytes, rx, rx_bytes,
                                                    deadline);
    }
    static nx_time_us_t clock(void* context) {
        auto* bus = static_cast<Bus*>(context);
        ++bus->clock_reads;
        return bus->now;
    }
    nx_tmp117_transport_port_t port() {
        return {this, exchange, {clock, this}};
    }
    void read(uint8_t reg, uint16_t word, nx_result_t result = NX_SUCCESS,
              std::function<void()> after = {}) {
        EXPECT_CALL(*this, Exchange(_, 1U, _, 2U, _))
            .InSequence(order)
            .WillOnce(Invoke([=](const uint8_t* tx, size_t, uint8_t* rx, size_t,
                                 nx_time_us_t) {
                EXPECT_EQ(tx[0], reg);
                if (result == NX_SUCCESS) {
                    rx[0] = static_cast<uint8_t>(word >> 8U);
                    rx[1] = static_cast<uint8_t>(word & 0xffU);
                }
                if (after) {
                    after();
                }
                return result;
            }));
    }
    void write(uint16_t word, nx_result_t result = NX_SUCCESS,
               std::function<void()> after = {}) {
        EXPECT_CALL(*this, Exchange(_, 3U, nullptr, 0U, _))
            .InSequence(order)
            .WillOnce(Invoke(
                [=](const uint8_t* tx, size_t, uint8_t*, size_t, nx_time_us_t) {
                    EXPECT_EQ(tx[0], 1U);
                    EXPECT_EQ(tx[1], static_cast<uint8_t>(word >> 8U));
                    EXPECT_EQ(tx[2], static_cast<uint8_t>(word & 0xffU));
                    if (after) {
                        after();
                    }
                    return result;
                }));
    }
    void initialize(int16_t offset = 0) {
        read(0x0fU, 0x0117U);
        read(0x01U, 0x0220U);
        read(0x04U, 0U);
        read(0x07U, static_cast<uint16_t>(offset));
        write(0x0600U);
        read(0x01U, 0x0600U);
    }
    void begin() {
        read(0x01U, 0x2600U);
        write(0x0e00U);
    }
    void ready(uint16_t raw) {
        read(0x01U, 0x2600U);
        read(0x00U, raw);
    }
    void cancel() {
        read(0x04U, 0U);
        write(0x0600U);
        read(0x01U, 0x0600U);
    }
};

class TMP117 : public testing::Test {
  protected:
    StrictMock<Bus> bus;
    nx_tmp117_t device{};
    nx_tmp117_config_t config{7U, 3U, 0};
    nx_tmp117_sample_t output{};
    void initialize() {
        bus.initialize(config.expected_offset_lsb);
        ASSERT_EQ(nx_tmp117_init(&device, bus.port(), &config, 100000U),
                  NX_SUCCESS);
    }
    void begin(nx_time_us_t deadline = 100000U) {
        bus.begin();
        ASSERT_EQ(nx_tmp117_begin(&device, deadline), NX_SUCCESS);
    }
    std::array<uint8_t, sizeof(output)> poison() {
        std::memset(&output, 0xa5, sizeof(output));
        std::array<uint8_t, sizeof(output)> before{};
        std::memcpy(before.data(), &output, sizeof(output));
        return before;
    }
    void unchanged(const std::array<uint8_t, sizeof(output)>& before) {
        EXPECT_EQ(std::memcmp(&output, before.data(), sizeof(output)), 0);
    }
};

TEST_F(TMP117, InitializationVerifiesIdentityCalibrationAndShutdown) {
    initialize();
    EXPECT_TRUE(device.initialized);
    EXPECT_FALSE(device.measuring);
    EXPECT_FALSE(device.uncertain);
    EXPECT_EQ(device.base_configuration, 0x0200U);
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, InvalidInputsAndKnownAliasesCauseNoExchange) {
    auto port = bus.port();
    auto bad = config;
    bad.clock_domain = 0U;
    EXPECT_EQ(nx_tmp117_init(&device, port, &bad, 100U), NX_ERROR_INVALID);
    bad = config;
    bad.clock_epoch = 0U;
    EXPECT_EQ(nx_tmp117_init(&device, port, &bad, 100U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_tmp117_init(nullptr, port, &config, 100U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_tmp117_init(&device, port, &config, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_tmp117_init(&device, port,
                             reinterpret_cast<nx_tmp117_config_t*>(&device),
                             100U),
              NX_ERROR_INVALID);
    port.context = nullptr;
    EXPECT_EQ(nx_tmp117_init(&device, port, &config, 100U), NX_ERROR_INVALID);
    EXPECT_EQ(device.transport.context, nullptr);
}

TEST_F(TMP117, UnsupportedIdentityNeverWritesOrRetainsNewBinding) {
    bus.read(0x0fU, 0x1116U);
    EXPECT_EQ(nx_tmp117_init(&device, bus.port(), &config, 100U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(device.initialized);
    EXPECT_EQ(device.transport.context, nullptr);
}

TEST_F(TMP117, PowerUpAndUnlockedEepromNeverTriggerConfigurationWrites) {
    bus.read(0x0fU, 0x0117U);
    bus.read(0x01U, 0x1220U);
    EXPECT_EQ(nx_tmp117_init(&device, bus.port(), &config, 100U),
              NX_ERROR_BUSY);
    bus.read(0x0fU, 0x0117U);
    bus.read(0x01U, 0x0220U);
    bus.read(0x04U, 0x4000U);
    EXPECT_EQ(nx_tmp117_init(&device, bus.port(), &config, 100U),
              NX_ERROR_BUSY);
    bus.read(0x0fU, 0x0117U);
    bus.read(0x01U, 0x0220U);
    bus.read(0x04U, 0x8000U);
    EXPECT_EQ(nx_tmp117_init(&device, bus.port(), &config, 100U),
              NX_ERROR_PERMISSION);
}

TEST_F(TMP117, OffsetMismatchRejectsWithoutDiscardingCalibration) {
    bus.read(0x0fU, 0x0117U);
    bus.read(0x01U, 0x0220U);
    bus.read(0x04U, 0U);
    bus.read(0x07U, 1U);
    EXPECT_EQ(nx_tmp117_init(&device, bus.port(), &config, 100U),
              NX_ERROR_STATE);
    EXPECT_FALSE(device.initialized);
    EXPECT_EQ(device.transport.context, nullptr);
}

TEST_F(TMP117, RefusedShutdownReadbackDoesNotPublishInitialization) {
    bus.read(0x0fU, 0x0117U);
    bus.read(0x01U, 0x0220U);
    bus.read(0x04U, 0U);
    bus.read(0x07U, 0U);
    bus.write(0x0600U);
    bus.read(0x01U, 0x0620U);
    EXPECT_EQ(nx_tmp117_init(&device, bus.port(), &config, 100U),
              NX_ERROR_STATE);
    EXPECT_FALSE(device.initialized);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(device.transport.context, &bus);
    EXPECT_EQ(device.base_configuration, 0x0200U);
    bus.cancel();
    EXPECT_EQ(nx_tmp117_cancel(&device, 200U), NX_SUCCESS);
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, InitializationFailuresRetainOnlyIssuedHardwareWrites) {
    for (size_t cut = 0U; cut < 6U; ++cut) {
        SCOPED_TRACE(cut);
        StrictMock<Bus> failing;
        failing.now = 10U + cut;
        nx_tmp117_t fresh{};
        const uint8_t registers[] = {0x0fU, 0x01U, 0x04U, 0x07U, 0x01U, 0x01U};
        const uint16_t words[] = {0x0117U, 0x0220U, 0U, 0U, 0x0600U, 0x0600U};
        for (size_t step = 0U; step <= cut; ++step) {
            const nx_result_t result = step == cut ? NX_ERROR_IO : NX_SUCCESS;
            if (step == 4U) {
                failing.write(words[step], result);
            } else {
                failing.read(registers[step], words[step], result);
            }
        }
        EXPECT_EQ(nx_tmp117_init(&fresh, failing.port(), &config, 100U),
                  NX_ERROR_IO);
        EXPECT_FALSE(fresh.initialized);
        EXPECT_FALSE(fresh.measuring);
        EXPECT_FALSE(fresh.in_call);
        if (cut < 4U) {
            EXPECT_EQ(fresh.transport.context, nullptr);
            EXPECT_EQ(fresh.transport.exchange, nullptr);
        } else {
            ASSERT_EQ(fresh.transport.context, &failing);
            EXPECT_TRUE(fresh.uncertain);
            EXPECT_EQ(fresh.config.clock_domain, config.clock_domain);
            EXPECT_EQ(fresh.config.clock_epoch, config.clock_epoch);
            EXPECT_EQ(fresh.base_configuration, 0x0200U);
            EXPECT_EQ(fresh.last_clock, failing.now);
            StrictMock<Bus> other;
            EXPECT_EQ(nx_tmp117_init(&fresh, other.port(), &config, 200U),
                      NX_ERROR_STATE);
            auto other_config = config;
            ++other_config.clock_epoch;
            EXPECT_EQ(
                nx_tmp117_init(&fresh, failing.port(), &other_config, 200U),
                NX_ERROR_STATE);
            failing.cancel();
            ASSERT_EQ(nx_tmp117_cancel(&fresh, 200U), NX_SUCCESS);
            EXPECT_FALSE(fresh.uncertain);
            EXPECT_FALSE(fresh.initialized);
            EXPECT_EQ(nx_tmp117_begin(&fresh, 200U), NX_ERROR_STATE);
            failing.initialize();
            ASSERT_EQ(nx_tmp117_init(&fresh, failing.port(), &config, 200U),
                      NX_SUCCESS);
        }
        nx_tmp117_stop(&fresh);
    }
}

TEST_F(TMP117, ExpiredWritePreflightDoesNotRetainANewBinding) {
    bus.read(0x0fU, 0x0117U);
    bus.read(0x01U, 0x0220U);
    bus.read(0x04U, 0U);
    bus.read(0x07U, 0U);
    auto port = bus.port();
    port.clock.read = [](void* context) -> nx_time_us_t {
        auto* clock = static_cast<Bus*>(context);
        ++clock->clock_reads;
        return clock->clock_reads >= 9U ? 100U : 0U;
    };
    EXPECT_EQ(nx_tmp117_init(&device, port, &config, 100U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(bus.clock_reads, 9U);
    EXPECT_FALSE(device.initialized);
    EXPECT_FALSE(device.uncertain);
    EXPECT_FALSE(device.in_call);
    EXPECT_EQ(device.transport.context, nullptr);
    EXPECT_EQ(device.transport.exchange, nullptr);
}

TEST_F(TMP117, TransportBusyIsTerminalUnlikeSuccessfulNotReadyObservation) {
    initialize();
    begin();
    const auto before = poison();
    bus.read(0x01U, 0U, NX_ERROR_BUSY);
    EXPECT_EQ(nx_tmp117_poll(&device, &output), NX_ERROR_BUSY);
    EXPECT_FALSE(device.initialized);
    EXPECT_FALSE(device.measuring);
    EXPECT_TRUE(device.uncertain);
    unchanged(before);
    EXPECT_EQ(nx_tmp117_poll(&device, &output), NX_ERROR_STATE);
    unchanged(before);
    bus.cancel();
    ASSERT_EQ(nx_tmp117_cancel(&device, 200000U), NX_SUCCESS);
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, OneShotPublishesOnlyFreshIndependentProvenance) {
    initialize();
    begin();
    const auto before = poison();
    bus.now = 1000U;
    bus.read(0x01U, 0x0e00U);
    EXPECT_EQ(nx_tmp117_poll(&device, &output), NX_ERROR_BUSY);
    unchanged(before);
    bus.now = 20000U;
    bus.ready(0x0c80U);
    ASSERT_EQ(nx_tmp117_poll(&device, &output), NX_SUCCESS);
    EXPECT_EQ(output.micro_celsius, 25000000);
    EXPECT_EQ(output.raw_lsb, 3200);
    EXPECT_EQ(output.offset_lsb, 0);
    EXPECT_EQ(output.conversion_started_at, 0U);
    EXPECT_EQ(output.observed.ticks, 20000U);
    EXPECT_EQ(output.observed.ticks_per_second, 1000000U);
    EXPECT_EQ(output.observed.domain, 7U);
    EXPECT_EQ(output.observed.epoch, 3U);
    EXPECT_EQ(output.observed.source, NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    EXPECT_FALSE(output.observed.uncertainty_known);
    EXPECT_EQ(output.observed.uncertainty_ticks, 0U);
    const auto copy = output;
    EXPECT_EQ(nx_tmp117_poll(&device, &output), NX_ERROR_STATE);
    nx_tmp117_stop(&device);
    EXPECT_EQ(copy.micro_celsius, 25000000);
    nx_io_timestamp_t lower = copy.observed;
    lower.ticks = copy.conversion_started_at;
    nx_io_timestamp_t now = copy.observed;
    now.ticks = 30000U;
    nx_io_timestamp_delta_t age{};
    ASSERT_EQ(nx_io_timestamp_elapsed(&lower, &now, &age), NX_SUCCESS);
    EXPECT_GT(age.ticks, 25000U);
    now.epoch = 4U;
    EXPECT_EQ(nx_io_timestamp_elapsed(&lower, &now, &age), NX_ERROR_STATE);
}

TEST_F(TMP117, TiSignedValuesUseNearestMicrodegreeTiesAwayFromZero) {
    initialize();
    const std::pair<uint16_t, int32_t> values[] = {
        {0x0000U, 0},         {0x0001U, 7813},      {0xffffU, -7813},
        {0x0010U, 125000},    {0xfff0U, -125000},   {0x0c80U, 25000000},
        {0xf380U, -25000000}, {0x3200U, 100000000}, {0xe480U, -55000000},
        {0x4b00U, 150000000}};
    for (const auto& value : values) {
        SCOPED_TRACE(value.first);
        begin();
        bus.ready(value.first);
        ASSERT_EQ(nx_tmp117_poll(&device, &output), NX_SUCCESS);
        EXPECT_EQ(output.micro_celsius, value.second);
    }
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, OnChipCalibrationIsReportedAndNeverAddedTwice) {
    config.expected_offset_lsb = 128;
    initialize();
    begin();
    bus.ready(0x0c80U);
    ASSERT_EQ(nx_tmp117_poll(&device, &output), NX_SUCCESS);
    EXPECT_EQ(output.micro_celsius, 25000000);
    EXPECT_EQ(output.offset_lsb, 128);
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, PorSaturationAndOutOfScopeValuesCannotAlterCallerSample) {
    const uint16_t values[]{0x8000U, 0x7fffU, 0x8001U,
                            0x7ffeU, 0xe47fU, 0x4b01U};
    for (uint16_t raw : values) {
        SCOPED_TRACE(raw);
        initialize();
        begin();
        const auto before = poison();
        bus.ready(raw);
        EXPECT_EQ(nx_tmp117_poll(&device, &output), NX_ERROR_IO);
        unchanged(before);
        EXPECT_FALSE(device.initialized);
        EXPECT_TRUE(device.uncertain);
        nx_tmp117_stop(&device);
    }
}

TEST_F(TMP117, ReadyClearThenReadFailureIsTerminalAndNeverCachedZero) {
    initialize();
    begin();
    const auto before = poison();
    bus.read(0x01U, 0x2600U);
    bus.read(0x00U, 0U, NX_ERROR_NACK);
    EXPECT_EQ(nx_tmp117_poll(&device, &output), NX_ERROR_NACK);
    unchanged(before);
    EXPECT_FALSE(device.initialized);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(nx_tmp117_poll(&device, &output), NX_ERROR_STATE);
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, ExpiredAdmissionAndLateStartNeverInventAConversionBudget) {
    initialize();
    bus.now = 10U;
    EXPECT_EQ(nx_tmp117_begin(&device, 10U), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(device.initialized);
    bus.read(0x01U, 0x0600U);
    bus.write(0x0e00U, NX_SUCCESS, [&] { bus.now = 2000U; });
    EXPECT_EQ(nx_tmp117_begin(&device, 1000U), NX_ERROR_TIMEOUT);
    EXPECT_FALSE(device.initialized);
    EXPECT_TRUE(device.uncertain);
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, ExpiredAndLatePollNeverPublishEvenValidTemperature) {
    initialize();
    begin(1000U);
    const auto before = poison();
    bus.now = 1000U;
    EXPECT_EQ(nx_tmp117_poll(&device, &output), NX_ERROR_TIMEOUT);
    unchanged(before);
    nx_tmp117_stop(&device);
    bus.now = 0U;
    initialize();
    begin(1000U);
    bus.read(0x01U, 0x2600U);
    bus.read(0x00U, 0x0c80U, NX_SUCCESS, [&] { bus.now = 1000U; });
    EXPECT_EQ(nx_tmp117_poll(&device, &output), NX_ERROR_TIMEOUT);
    unchanged(before);
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, ReverseClockClosesSamplingWithoutTouchingOutput) {
    bus.now = 100U;
    initialize();
    begin();
    const auto before = poison();
    bus.now = 99U;
    EXPECT_EQ(nx_tmp117_poll(&device, &output), NX_ERROR_IO);
    unchanged(before);
    EXPECT_TRUE(device.uncertain);
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, AliasedOutputRejectsBeforeExchangeAndPreservesConversion) {
    initialize();
    begin();
    EXPECT_EQ(
        nx_tmp117_poll(&device, reinterpret_cast<nx_tmp117_sample_t*>(&device)),
        NX_ERROR_INVALID);
    EXPECT_TRUE(device.measuring);
    bus.cancel();
    EXPECT_EQ(nx_tmp117_cancel(&device, 1000U), NX_SUCCESS);
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, CancelUsesIndependentDeadlineAndVerifiesShutdown) {
    initialize();
    begin(1000U);
    bus.now = 1000U;
    bus.cancel();
    EXPECT_EQ(nx_tmp117_cancel(&device, 2000U), NX_SUCCESS);
    EXPECT_TRUE(device.initialized);
    EXPECT_FALSE(device.measuring);
    EXPECT_FALSE(device.uncertain);
    begin(3000U);
    bus.cancel();
    EXPECT_EQ(nx_tmp117_cancel(&device, 4000U), NX_SUCCESS);
    nx_tmp117_stop(&device);
}

TEST_F(TMP117, FailedCancelRetainsExplicitRecoveryUntilVerifiedRetry) {
    initialize();
    begin();
    bus.read(0x04U, 0U);
    bus.write(0x0600U, NX_ERROR_IO);
    EXPECT_EQ(nx_tmp117_cancel(&device, 1000U), NX_ERROR_IO);
    EXPECT_FALSE(device.initialized);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(device.transport.context, &bus);
    bus.cancel();
    EXPECT_EQ(nx_tmp117_cancel(&device, 2000U), NX_SUCCESS);
    EXPECT_FALSE(device.uncertain);
    EXPECT_FALSE(device.initialized);
    EXPECT_EQ(nx_tmp117_begin(&device, 3000U), NX_ERROR_STATE);
    nx_tmp117_stop(&device);
    EXPECT_EQ(device.transport.context, nullptr);
}

TEST_F(TMP117, CallbackCannotReenterOrDetachInProgressInitialization) {
    bus.read(0x0fU, 0x0117U, NX_SUCCESS, [&] {
        EXPECT_EQ(nx_tmp117_begin(&device, 100U), NX_ERROR_BUSY);
        nx_tmp117_stop(&device);
        EXPECT_TRUE(device.in_call);
    });
    bus.read(0x01U, 0x0220U);
    bus.read(0x04U, 0U);
    bus.read(0x07U, 0U);
    bus.write(0x0600U);
    bus.read(0x01U, 0x0600U);
    ASSERT_EQ(nx_tmp117_init(&device, bus.port(), &config, 100U), NX_SUCCESS);
    nx_tmp117_stop(&device);
}

TEST(TMP117Instances, DistinctCalibrationClocksAndConversionsNeverLeak) {
    StrictMock<Bus> first_bus;
    StrictMock<Bus> second_bus;
    nx_tmp117_t first{};
    nx_tmp117_t second{};
    const nx_tmp117_config_t first_config{1U, 1U, 128};
    const nx_tmp117_config_t second_config{2U, 4U, -128};
    first_bus.initialize(128);
    second_bus.initialize(-128);
    ASSERT_EQ(nx_tmp117_init(&first, first_bus.port(), &first_config, 100000U),
              NX_SUCCESS);
    ASSERT_EQ(
        nx_tmp117_init(&second, second_bus.port(), &second_config, 100000U),
        NX_SUCCESS);
    first_bus.begin();
    second_bus.begin();
    ASSERT_EQ(nx_tmp117_begin(&first, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_tmp117_begin(&second, 100000U), NX_SUCCESS);
    first_bus.ready(0x0c80U);
    nx_tmp117_sample_t sample{};
    ASSERT_EQ(nx_tmp117_poll(&first, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.offset_lsb, 128);
    EXPECT_EQ(sample.observed.domain, 1U);
    second_bus.read(0x01U, 0x0e00U);
    EXPECT_EQ(nx_tmp117_poll(&second, &sample), NX_ERROR_BUSY);
    EXPECT_EQ(sample.micro_celsius, 25000000);
    EXPECT_EQ(sample.observed.domain, 1U);
    second_bus.cancel();
    EXPECT_EQ(nx_tmp117_cancel(&second, 100000U), NX_SUCCESS);
    nx_tmp117_stop(&first);
    nx_tmp117_stop(&second);
}

/** \brief Mock the external settled I2C endpoint, never an async request. */
class Endpoint {
  public:
    MOCK_METHOD(nx_result_t, Transaction,
                (nx_i2c_message_t*, size_t, nx_time_us_t, size_t*));
    static nx_result_t transaction(void* context, nx_i2c_message_t* messages,
                                   size_t count, nx_time_us_t deadline,
                                   size_t* transferred) {
        return static_cast<Endpoint*>(context)->Transaction(
            messages, count, deadline, transferred);
    }
};

TEST(TMP117I2C, ExactRepeatedStartReadAndWritesAreSettledAndCounted) {
    StrictMock<Endpoint> provider;
    nx_i2c_endpoint_ops_t ops{};
    ops.transaction = Endpoint::transaction;
    const nx_i2c_endpoint_t endpoint{&ops, &provider};
    StrictMock<Bus> clock;
    const auto port = nx_tmp117_i2c_transport(&endpoint, {Bus::clock, &clock});
    ASSERT_NE(port.exchange, nullptr);
    uint8_t pointer = 0x0fU;
    uint8_t read[2]{};
    EXPECT_CALL(provider, Transaction(_, 2U, 100U, _))
        .WillOnce(Invoke([&](nx_i2c_message_t* messages, size_t, nx_time_us_t,
                             size_t* transferred) {
            EXPECT_FALSE(messages[0].read);
            EXPECT_EQ(messages[0].length, 1U);
            EXPECT_EQ(messages[0].data[0], pointer);
            EXPECT_TRUE(messages[1].read);
            EXPECT_EQ(messages[1].length, 2U);
            messages[1].data[0] = 0x01U;
            messages[1].data[1] = 0x17U;
            *transferred = 3U;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(port.exchange(port.context, &pointer, 1U, read, 2U, 100U),
              NX_SUCCESS);
    EXPECT_EQ(read[1], 0x17U);
    const uint8_t write[3]{1U, 6U, 0U};
    EXPECT_CALL(provider, Transaction(_, 1U, 100U, _))
        .WillOnce(Invoke([&](nx_i2c_message_t* messages, size_t, nx_time_us_t,
                             size_t* transferred) {
            EXPECT_FALSE(messages[0].read);
            EXPECT_EQ(messages[0].length, 3U);
            EXPECT_EQ(std::memcmp(messages[0].data, write, 3U), 0);
            *transferred = 3U;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(port.exchange(port.context, write, 3U, nullptr, 0U, 100U),
              NX_SUCCESS);
}

TEST(TMP117I2C, ShortSuccessAndTransportErrorsNeverPretendFullRegisters) {
    StrictMock<Endpoint> provider;
    nx_i2c_endpoint_ops_t ops{};
    ops.transaction = Endpoint::transaction;
    const nx_i2c_endpoint_t endpoint{&ops, &provider};
    StrictMock<Bus> clock;
    const auto port = nx_tmp117_i2c_transport(&endpoint, {Bus::clock, &clock});
    ASSERT_NE(port.exchange, nullptr);
    uint8_t pointer = 0U;
    uint8_t read[2]{};
    EXPECT_CALL(provider, Transaction(_, 2U, 100U, _))
        .WillOnce(Invoke(
            [](nx_i2c_message_t*, size_t, nx_time_us_t, size_t* transferred) {
                *transferred = 2U;
                return NX_SUCCESS;
            }));
    EXPECT_EQ(port.exchange(port.context, &pointer, 1U, read, 2U, 100U),
              NX_ERROR_IO);
    EXPECT_CALL(provider, Transaction(_, 2U, 100U, _))
        .WillOnce(Invoke(
            [](nx_i2c_message_t*, size_t, nx_time_us_t, size_t* transferred) {
                *transferred = 1U;
                return NX_ERROR_ARBITRATION;
            }));
    EXPECT_EQ(port.exchange(port.context, &pointer, 1U, read, 2U, 100U),
              NX_ERROR_ARBITRATION);
}

TEST(TMP117I2C, AsyncOnlyEndpointAndInvalidSpansHaveNoBorrowOrFallback) {
    nx_i2c_endpoint_ops_t empty{};
    int context{};
    const nx_i2c_endpoint_t endpoint{&empty, &context};
    StrictMock<Bus> clock;
    EXPECT_EQ(nx_tmp117_i2c_transport(&endpoint, {Bus::clock, &clock}).exchange,
              nullptr);
    EXPECT_EQ(nx_tmp117_i2c_transport(nullptr, {Bus::clock, &clock}).exchange,
              nullptr);
    StrictMock<Endpoint> provider;
    empty.transaction = Endpoint::transaction;
    const nx_i2c_endpoint_t valid{&empty, &provider};
    const auto port = nx_tmp117_i2c_transport(&valid, {Bus::clock, &clock});
    ASSERT_NE(port.exchange, nullptr);
    const uint8_t tx[3]{1U, 6U, 0U};
    uint8_t rx[2]{};
    EXPECT_EQ(port.exchange(port.context, tx, 0U, rx, 2U, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(port.exchange(port.context, tx, 3U, rx, 2U, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(port.exchange(port.context, tx, 1U, nullptr, 2U, 100U),
              NX_ERROR_INVALID);
}

/** \brief Integration-only executor; product scheduling stays external. */
struct Owner {
    nx_tmp117_t* device;
    nx_tmp117_sample_t* output;
    nx_time_us_t cleanup_deadline;
    bool cancelled{};
    nx_result_t cancellation_result{NX_SUCCESS};
    static nx_result_t start(void* context, void*, nx_time_us_t deadline) {
        auto* owner = static_cast<Owner*>(context);
        return nx_tmp117_begin(owner->device, deadline);
    }
    static nx_result_t service(void* context, nx_result_t* result,
                               size_t* count) {
        auto* owner = static_cast<Owner*>(context);
        if (owner->cancelled) {
            *result = NX_ERROR_CANCELLED;
            *count = 0U;
            return NX_SUCCESS;
        }
        if (owner->cancellation_result != NX_SUCCESS) {
            return owner->cancellation_result;
        }
        nx_result_t status = nx_tmp117_poll(owner->device, owner->output);
        if (status == NX_SUCCESS) {
            *result = NX_SUCCESS;
            *count = 1U;
        }
        return status;
    }
    static nx_result_t cancel(void* context) {
        auto* owner = static_cast<Owner*>(context);
        nx_result_t status =
            nx_tmp117_cancel(owner->device, owner->cleanup_deadline);
        owner->cancelled = status == NX_SUCCESS;
        owner->cancellation_result = status;
        return status;
    }
    static uintptr_t enter(void*) {
        return 0U;
    }
    static void leave(void*, uintptr_t) {
    }
};

TEST_F(TMP117, RealGenericOwnerRetainsCancelledLoanUntilDeviceCleanup) {
    initialize();
    const auto before = poison();
    Owner executor{&device, &output, 100000U};
    nx_bus_owner_t owner{};
    nx_owner_slot_t slots[1]{};
    ASSERT_EQ(nx_bus_owner_init(&owner, slots, 1U,
                                {nullptr, Owner::enter, Owner::leave},
                                {&executor, Owner::start, Owner::service,
                                 Owner::cancel, true},
                                {Bus::clock, &bus}, nullptr),
              NX_SUCCESS);
    nx_request_t request{};
    nx_request_initialize(&request);
    ASSERT_EQ(nx_request_prepare(&request, 100000U), NX_SUCCESS);
    nx_owner_ticket_t ticket{};
    int operation{};
    ASSERT_EQ(
        nx_bus_owner_submit(&owner, &request, &operation, nullptr, &ticket),
        NX_SUCCESS);
    bus.begin();
    ASSERT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    bus.read(0x01U, 0x0e00U);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_BUSY);
    nx_bus_owner_stop(&owner);
    bus.read(0x04U, 0U);
    bus.write(0x0600U, NX_ERROR_IO);
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_request_prepare(&request, 200000U), NX_ERROR_STATE);
    unchanged(before);
    bus.cancel();
    EXPECT_EQ(nx_bus_owner_service(&owner), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request), NX_REQUEST_SETTLED);
    nx_result_t result{};
    size_t count{};
    ASSERT_EQ(nx_request_result(&request, &result, &count), NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_CANCELLED);
    EXPECT_EQ(count, 0U);
    EXPECT_TRUE(nx_bus_owner_idle(&owner));
    unchanged(before);
    nx_tmp117_stop(&device);
}
} /* namespace */
