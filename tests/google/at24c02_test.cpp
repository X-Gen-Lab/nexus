/**
 * \file            at24c02_test.cpp
 * \brief           EEPROM page boundaries, ready observation and failure loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/at24c02.h"
#include <array>
#include <cstring>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

namespace {
using testing::_;
using testing::Invoke;
using testing::StrictMock;

/** \brief Mock only the addressed, fully settled public bus and its clock. */
class Bus {
  public:
    MOCK_METHOD(nx_result_t, Transaction,
                (nx_i2c_message_t*, size_t, nx_time_us_t, size_t*));
    testing::Sequence order;
    nx_time_us_t now{};
    nx_time_us_t step{};
    static nx_result_t transaction(void* context, nx_i2c_message_t* messages,
                                   size_t count, nx_time_us_t deadline,
                                   size_t* transferred) {
        return static_cast<Bus*>(context)->Transaction(messages, count,
                                                       deadline, transferred);
    }
    static nx_time_us_t clock(void* context) {
        auto* bus = static_cast<Bus*>(context);
        nx_time_us_t result = bus->now;
        bus->now += bus->step;
        return result;
    }
    static const nx_i2c_endpoint_ops_t ops;
    nx_i2c_endpoint_t endpoint{&ops, this};
    nx_clock_t time() {
        return {clock, this};
    }
    void page(uint8_t address, std::vector<uint8_t> bytes,
              nx_result_t outcome = NX_SUCCESS,
              std::function<void()> after = {}) {
        EXPECT_CALL(*this, Transaction(_, 1U, _, _))
            .InSequence(order)
            .WillOnce(Invoke([=](nx_i2c_message_t* messages, size_t,
                                 nx_time_us_t, size_t* transferred) {
                EXPECT_FALSE(messages[0].read);
                EXPECT_EQ(messages[0].length, bytes.size() + 1U);
                EXPECT_EQ(messages[0].buffer.tx[0], address);
                EXPECT_EQ(std::vector<uint8_t>(messages[0].buffer.tx + 1,
                                               messages[0].buffer.tx +
                                                   messages[0].length),
                          bytes);
                EXPECT_LE((static_cast<size_t>(address) & 7U) + bytes.size(),
                          8U);
                *transferred = outcome == NX_SUCCESS ? bytes.size() + 1U : 0U;
                if (after) {
                    after();
                }
                return outcome;
            }));
    }
    void ready(nx_result_t outcome = NX_SUCCESS,
               std::function<void()> after = {}, bool short_success = false) {
        EXPECT_CALL(*this, Transaction(_, 1U, _, _))
            .InSequence(order)
            .WillOnce(Invoke([=](nx_i2c_message_t* messages, size_t,
                                 nx_time_us_t, size_t* transferred) {
                EXPECT_TRUE(messages[0].read);
                EXPECT_EQ(messages[0].length, 1U);
                messages[0].buffer.rx[0] = 0xa5U;
                *transferred =
                    outcome == NX_SUCCESS && !short_success ? 1U : 0U;
                if (after) {
                    after();
                }
                return outcome;
            }));
    }
};

const nx_i2c_endpoint_ops_t Bus::ops{Bus::transaction, nullptr, nullptr,
                                     nullptr};

class At24C02Test : public testing::Test {
  protected:
    StrictMock<Bus> bus;
    nx_at24c02_t device{};
    void initialize(uint16_t probes = 4U) {
        ASSERT_EQ(nx_at24c02_init(&device, &bus.endpoint, bus.time(), probes),
                  NX_SUCCESS);
    }
    void random_read(size_t offset, size_t bytes,
                     nx_result_t outcome = NX_SUCCESS,
                     std::function<void()> after = {},
                     bool short_success = false) {
        EXPECT_CALL(bus, Transaction(_, 2U, _, _))
            .InSequence(bus.order)
            .WillOnce(Invoke([=](nx_i2c_message_t* messages, size_t,
                                 nx_time_us_t, size_t* transferred) {
                EXPECT_FALSE(messages[0].read);
                EXPECT_EQ(messages[0].length, 1U);
                EXPECT_EQ(messages[0].buffer.tx[0], offset);
                EXPECT_TRUE(messages[1].read);
                EXPECT_EQ(messages[1].length, bytes);
                std::memset(messages[1].buffer.rx, 0x5a, bytes);
                *transferred = short_success ? bytes : bytes + 1U;
                if (after) {
                    after();
                }
                return outcome;
            }));
    }
};

TEST_F(At24C02Test, InitAndIdleStopPerformNoBusAccess) {
    initialize();
    EXPECT_TRUE(device.initialized);
    EXPECT_EQ(nx_at24c02_stop(&device), NX_SUCCESS);
    EXPECT_FALSE(device.initialized);
    EXPECT_EQ(nx_at24c02_stop(&device), NX_SUCCESS);
}

TEST_F(At24C02Test, RejectsMissingMethodsClockAndZeroProbeBudget) {
    EXPECT_EQ(nx_at24c02_init(nullptr, &bus.endpoint, bus.time(), 4U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_init(&device, nullptr, bus.time(), 4U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_init(&device, &bus.endpoint, {}, 4U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_init(&device, &bus.endpoint, bus.time(), 0U),
              NX_ERROR_INVALID);
    nx_i2c_endpoint_ops_t async_ops{};
    nx_i2c_endpoint_t async_only{&async_ops, &bus};
    EXPECT_EQ(nx_at24c02_init(&device, &async_only, bus.time(), 4U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(device.initialized);
}

TEST_F(At24C02Test, SecondInitCannotReplaceALiveBinding) {
    initialize();
    EXPECT_EQ(nx_at24c02_init(&device, &bus.endpoint, bus.time(), 3U),
              NX_ERROR_BUSY);
    EXPECT_EQ(device.max_ready_probes, 4U);
}

TEST_F(At24C02Test, FullRangeUsesOneOffsetAndRepeatedStartRead) {
    initialize();
    std::array<uint8_t, 256> data{};
    random_read(0U, data.size());
    EXPECT_EQ(nx_at24c02_read(&device, 0U, data.data(), data.size(), 100U),
              NX_SUCCESS);
    EXPECT_EQ(data.front(), 0x5aU);
    EXPECT_EQ(data.back(), 0x5aU);
    EXPECT_EQ(nx_at24c02_stop(&device), NX_SUCCESS);
}

TEST_F(At24C02Test, ReadRejectsEmptyOverflowAndControlAliasesBeforeIo) {
    initialize();
    std::array<uint8_t, 2> data{0xa5U, 0xa5U};
    EXPECT_EQ(nx_at24c02_read(&device, 255U, data.data(), 2U, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_read(&device, SIZE_MAX, data.data(), 1U, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_read(&device, 0U, data.data(), 0U, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_read(&device, 0U, nullptr, 1U, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_read(&device, 0U, reinterpret_cast<uint8_t*>(&device),
                              1U, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_read(&device, 0U, data.data(), 1U, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(data[0], 0xa5U);
}

TEST_F(At24C02Test, ReadErrorReleasesAllLocalSpansAndDoesNotRetry) {
    initialize();
    uint8_t byte{};
    random_read(255U, 1U, NX_ERROR_ARBITRATION);
    EXPECT_EQ(nx_at24c02_read(&device, 255U, &byte, 1U, 100U),
              NX_ERROR_ARBITRATION);
    EXPECT_FALSE(device.in_call);
    EXPECT_FALSE(device.write_pending);
    EXPECT_EQ(nx_at24c02_stop(&device), NX_SUCCESS);
}

TEST_F(At24C02Test, ShortSuccessfulReadIsNotCompleteData) {
    initialize();
    uint8_t byte{};
    random_read(0U, 1U, NX_SUCCESS, {}, true);
    EXPECT_EQ(nx_at24c02_read(&device, 0U, &byte, 1U, 100U), NX_ERROR_IO);
}

TEST_F(At24C02Test, WriteSplitsAtEveryPhysicalPageBoundary) {
    initialize();
    std::array<uint8_t, 10> data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    bus.page(7U, {0});
    bus.ready();
    bus.page(8U, {1, 2, 3, 4, 5, 6, 7, 8});
    bus.ready();
    bus.page(16U, {9});
    bus.ready();
    nx_at24c02_progress_t progress{99U, true};
    EXPECT_EQ(nx_at24c02_write(&device, 7U, data.data(), data.size(), 100U,
                               &progress),
              NX_SUCCESS);
    EXPECT_EQ(progress.confirmed_bytes, data.size());
    EXPECT_FALSE(progress.uncertain);
    EXPECT_FALSE(device.write_pending);
}

TEST_F(At24C02Test, FullDeviceWriteIsThirtyTwoEightBytePages) {
    initialize();
    std::array<uint8_t, 256> data{};
    for (size_t i = 0U; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    for (size_t i = 0U; i < data.size(); i += 8U) {
        bus.page(static_cast<uint8_t>(i),
                 {data.begin() + static_cast<ptrdiff_t>(i),
                  data.begin() + static_cast<ptrdiff_t>(i + 8U)});
        bus.ready();
    }
    nx_at24c02_progress_t progress{};
    EXPECT_EQ(nx_at24c02_write(&device, 0U, data.data(), data.size(), 100U,
                               &progress),
              NX_SUCCESS);
    EXPECT_EQ(progress.confirmed_bytes, 256U);
}

TEST_F(At24C02Test, FinalDeviceByteDoesNotWrapToZero) {
    initialize();
    const uint8_t data = 0x43U;
    bus.page(255U, {data});
    bus.ready();
    nx_at24c02_progress_t progress{};
    EXPECT_EQ(nx_at24c02_write(&device, 255U, &data, 1U, 100U, &progress),
              NX_SUCCESS);
    EXPECT_EQ(progress.confirmed_bytes, 1U);
}

TEST_F(At24C02Test, WriteRejectsBadSpansWithoutChangingProgress) {
    initialize();
    const std::array<uint8_t, 2> data{1U, 2U};
    nx_at24c02_progress_t progress{73U, true};
    EXPECT_EQ(nx_at24c02_write(&device, 255U, data.data(), 2U, 100U, &progress),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_write(&device, 0U, data.data(), 0U, 100U, &progress),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_write(&device, 0U, nullptr, 1U, 100U, &progress),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_write(&device, 0U, data.data(), 1U, 100U, nullptr),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_at24c02_write(&device, 0U,
                               reinterpret_cast<const uint8_t*>(&progress), 1U,
                               100U, &progress),
              NX_ERROR_INVALID);
    EXPECT_EQ(progress.confirmed_bytes, 73U);
    EXPECT_TRUE(progress.uncertain);
}

TEST_F(At24C02Test, NackDuringOwnWriteCycleCanBeRetried) {
    initialize();
    const uint8_t data = 0x24U;
    bus.page(0U, {data});
    bus.ready(NX_ERROR_NACK);
    bus.ready(NX_ERROR_NACK);
    bus.ready();
    nx_at24c02_progress_t progress{};
    EXPECT_EQ(nx_at24c02_write(&device, 0U, &data, 1U, 100U, &progress),
              NX_SUCCESS);
    EXPECT_EQ(progress.confirmed_bytes, 1U);
}

TEST_F(At24C02Test, ArbitrationIsNeverReclassifiedAsWriteCycleBusy) {
    initialize();
    const uint8_t data = 0x24U;
    bus.page(0U, {data});
    bus.ready(NX_ERROR_ARBITRATION);
    nx_at24c02_progress_t progress{};
    EXPECT_EQ(nx_at24c02_write(&device, 0U, &data, 1U, 100U, &progress),
              NX_ERROR_ARBITRATION);
    EXPECT_EQ(progress.confirmed_bytes, 0U);
    EXPECT_TRUE(progress.uncertain);
    EXPECT_TRUE(device.write_pending);
    EXPECT_EQ(nx_at24c02_stop(&device), NX_ERROR_BUSY);
}

TEST_F(At24C02Test, BusIoErrorAndPageNackAreNotAutomaticRetries) {
    initialize();
    const uint8_t data = 0x24U;
    bus.page(0U, {data}, NX_ERROR_NACK);
    nx_at24c02_progress_t progress{};
    EXPECT_EQ(nx_at24c02_write(&device, 0U, &data, 1U, 100U, &progress),
              NX_ERROR_NACK);
    EXPECT_TRUE(progress.uncertain);
    bus.ready(NX_ERROR_IO);
    EXPECT_EQ(nx_at24c02_sync(&device, 200U), NX_ERROR_IO);
    EXPECT_TRUE(device.write_pending);
}

TEST_F(At24C02Test, ProbeLimitBoundsAFrozenClockAndCleanupNeverReplaysData) {
    initialize(2U);
    const uint8_t data = 0x24U;
    bus.page(0U, {data});
    bus.ready(NX_ERROR_NACK);
    bus.ready(NX_ERROR_NACK);
    nx_at24c02_progress_t progress{};
    EXPECT_EQ(nx_at24c02_write(&device, 0U, &data, 1U, 100U, &progress),
              NX_ERROR_BUSY);
    EXPECT_EQ(progress.confirmed_bytes, 0U);
    EXPECT_TRUE(progress.uncertain);
    EXPECT_EQ(nx_at24c02_stop(&device), NX_ERROR_BUSY);
    uint8_t read{};
    EXPECT_EQ(nx_at24c02_read(&device, 0U, &read, 1U, 100U), NX_ERROR_BUSY);
    bus.ready();
    EXPECT_EQ(nx_at24c02_sync(&device, 200U), NX_SUCCESS);
    EXPECT_FALSE(device.write_pending);
    EXPECT_TRUE(progress.uncertain);
    EXPECT_EQ(nx_at24c02_stop(&device), NX_SUCCESS);
}

TEST_F(At24C02Test, DeadlineExpiresAcrossTheIssuedWriteCycle) {
    initialize();
    const uint8_t data = 0x24U;
    bus.page(0U, {data}, NX_SUCCESS, [&] { bus.now = 100U; });
    nx_at24c02_progress_t progress{};
    EXPECT_EQ(nx_at24c02_write(&device, 0U, &data, 1U, 100U, &progress),
              NX_ERROR_TIMEOUT);
    EXPECT_TRUE(progress.uncertain);
    EXPECT_EQ(progress.confirmed_bytes, 0U);
    bus.ready();
    EXPECT_EQ(nx_at24c02_sync(&device, 200U), NX_SUCCESS);
}

TEST_F(At24C02Test, ConfirmedEarlierPageSurvivesDeadlineBeforeNextPage) {
    initialize();
    const std::array<uint8_t, 9> data{};
    bus.page(0U, std::vector<uint8_t>(8U));
    bus.ready(NX_SUCCESS, [&] {
        bus.now = 99U;
        bus.step = 1U;
    });
    nx_at24c02_progress_t progress{};
    EXPECT_EQ(nx_at24c02_write(&device, 0U, data.data(), data.size(), 100U,
                               &progress),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(progress.confirmed_bytes, 8U);
    EXPECT_FALSE(progress.uncertain);
    EXPECT_FALSE(device.write_pending);
}

TEST_F(At24C02Test, ReentryCannotStopReadOrRebindDuringTransport) {
    initialize();
    const uint8_t data = 0x24U;
    bus.page(0U, {data}, NX_SUCCESS, [&] {
        uint8_t read{};
        EXPECT_EQ(nx_at24c02_stop(&device), NX_ERROR_BUSY);
        EXPECT_EQ(nx_at24c02_sync(&device, 200U), NX_ERROR_BUSY);
        EXPECT_EQ(nx_at24c02_read(&device, 0U, &read, 1U, 200U), NX_ERROR_BUSY);
        EXPECT_EQ(nx_at24c02_init(&device, &bus.endpoint, bus.time(), 2U),
                  NX_ERROR_BUSY);
    });
    bus.ready();
    nx_at24c02_progress_t progress{};
    EXPECT_EQ(nx_at24c02_write(&device, 0U, &data, 1U, 100U, &progress),
              NX_SUCCESS);
    EXPECT_FALSE(device.in_call);
}

TEST_F(At24C02Test, ShortSuccessDuringReadyKeepsTheWriteUncertain) {
    initialize();
    const uint8_t data = 0x24U;
    bus.page(0U, {data});
    bus.ready(NX_SUCCESS, {}, true);
    nx_at24c02_progress_t progress{};
    EXPECT_EQ(nx_at24c02_write(&device, 0U, &data, 1U, 100U, &progress),
              NX_ERROR_IO);
    EXPECT_TRUE(progress.uncertain);
    EXPECT_TRUE(device.write_pending);
}

TEST_F(At24C02Test, ClockRegressionIsTerminalUntilIdleStopAndReinit) {
    initialize();
    bus.now = 50U;
    uint8_t data{};
    random_read(0U, 1U, NX_SUCCESS, [&] { bus.now = 49U; });
    EXPECT_EQ(nx_at24c02_read(&device, 0U, &data, 1U, 100U), NX_ERROR_IO);
    EXPECT_TRUE(device.clock_failed);
    EXPECT_EQ(nx_at24c02_read(&device, 0U, &data, 1U, 100U), NX_ERROR_IO);
    EXPECT_EQ(nx_at24c02_stop(&device), NX_SUCCESS);
}

TEST_F(At24C02Test, ExpiredAndExhaustedClocksCauseNoBusAccess) {
    initialize();
    uint8_t data{};
    bus.now = 100U;
    EXPECT_EQ(nx_at24c02_read(&device, 0U, &data, 1U, 100U), NX_ERROR_TIMEOUT);
    bus.now = NX_DEADLINE_NEVER;
    EXPECT_EQ(nx_at24c02_read(&device, 0U, &data, 1U, 200U), NX_ERROR_IO);
}

TEST_F(At24C02Test, StoppedInstanceHasNoHiddenTransactionFallback) {
    initialize();
    EXPECT_EQ(nx_at24c02_stop(&device), NX_SUCCESS);
    uint8_t data{};
    EXPECT_EQ(nx_at24c02_read(&device, 0U, &data, 1U, 100U), NX_ERROR_STATE);
    EXPECT_EQ(nx_at24c02_sync(&device, 100U), NX_ERROR_STATE);
}
} /* namespace */
