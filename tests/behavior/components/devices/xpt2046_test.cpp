/**
 * \file            xpt2046_test.cpp
 * \brief           Actual XPT2046 command, power-down and raw sample contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/xpt2046.h"
#include <array>
#include <cstring>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using testing::_;
using testing::Invoke;
using testing::StrictMock;

/** \brief Mock only external completed 24-clock SPI and pen/clock observations.
 */
class TouchBus {
  public:
    MOCK_METHOD(nx_result_t, Transfer,
                (const uint8_t*, uint8_t*, size_t, nx_time_us_t, size_t*));
    MOCK_METHOD(nx_result_t, Pen, (uint32_t*));
    MOCK_METHOD(nx_result_t, Level, (bool*));
    testing::Sequence sequence;
    nx_time_us_t now{10U};
    unsigned clock_reads{};
    std::function<void()> on_clock;
    nx_spi_endpoint_t endpoint{&spi_ops, this};
    nx_gpio_port_t pen{&gpio_ops, this};
    nx_exti_port_t exti{&exti_ops, this};
    static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                                size_t length, nx_time_us_t deadline,
                                size_t* transferred) {
        return static_cast<TouchBus*>(context)->Transfer(tx, rx, length,
                                                         deadline, transferred);
    }
    static nx_result_t read(const void* context, uint32_t* value) {
        return const_cast<TouchBus*>(static_cast<const TouchBus*>(context))
            ->Pen(value);
    }
    static nx_result_t level(const void* context, bool* high) {
        return const_cast<TouchBus*>(static_cast<const TouchBus*>(context))
            ->Level(high);
    }
    static nx_time_us_t clock(void* context) {
        auto* bus = static_cast<TouchBus*>(context);
        ++bus->clock_reads;
        if (bus->on_clock) {
            bus->on_clock();
        }
        return bus->now;
    }
    static const nx_spi_endpoint_ops_t spi_ops;
    static const nx_gpio_ops_t gpio_ops;
    static const nx_exti_ops_t exti_ops;
    nx_xpt2046_config_t config(bool pressure = false) {
        return {&endpoint, &pen, 1U, {clock, this}, 3U, 7U, pressure, nullptr};
    }
    void convert(uint8_t command, uint16_t value = 0U,
                 nx_result_t result = NX_SUCCESS, size_t count = 3U,
                 std::function<void()> callback = {},
                 uint16_t format_bits = 0U) {
        EXPECT_CALL(*this, Transfer(_, _, 3U, _, _))
            .InSequence(sequence)
            .WillOnce(Invoke([=](const uint8_t* tx, uint8_t* rx, size_t,
                                 nx_time_us_t, size_t* transferred) {
                EXPECT_EQ(tx[0], command);
                EXPECT_EQ(tx[1], 0U);
                EXPECT_EQ(tx[2], 0U);
                const uint16_t word =
                    static_cast<uint16_t>((value << 3U) | format_bits);
                rx[0] = 0xffU;
                rx[1] = static_cast<uint8_t>(word >> 8U);
                rx[2] = static_cast<uint8_t>(word);
                *transferred = count;
                if (callback) {
                    callback();
                }
                return result;
            }));
    }
    void level_contact(bool down, nx_result_t result = NX_SUCCESS) {
        EXPECT_CALL(*this, Level(_))
            .InSequence(sequence)
            .WillOnce(Invoke([=](bool* out) {
                *out = !down;
                return result;
            }));
    }
    void contact(bool down, nx_result_t result = NX_SUCCESS) {
        EXPECT_CALL(*this, Pen(_))
            .InSequence(sequence)
            .WillOnce(Invoke([=](uint32_t* out) {
                *out = down ? 0U : 1U;
                return result;
            }));
    }
};
const nx_spi_endpoint_ops_t TouchBus::spi_ops = {TouchBus::transfer, nullptr,
                                                 nullptr, nullptr, nullptr};
const nx_gpio_ops_t TouchBus::gpio_ops = {nullptr, TouchBus::read, nullptr};
const nx_exti_ops_t TouchBus::exti_ops = {nullptr, nullptr, nullptr,
                                          TouchBus::level};

class XPT2046 : public testing::Test {
  protected:
    StrictMock<TouchBus> bus;
    nx_xpt2046_config_t config = bus.config();
    nx_xpt2046_t device{};
    nx_xpt2046_sample_t output{};
    void initialize() {
        bus.convert(0x90U);
        ASSERT_EQ(nx_xpt2046_init(&device, &config, 1000U), NX_SUCCESS);
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

TEST_F(XPT2046, InitUsesActualDifferential12BitPowerDownCommand) {
    initialize();
    EXPECT_TRUE(device.initialized);
    EXPECT_FALSE(device.uncertain);
    EXPECT_EQ(device.config, &config);
}

TEST_F(XPT2046, CompleteRawXySequencePublishesOnlyAfterFinalPenObservation) {
    initialize();
    bus.contact(true);
    bus.convert(0xd0U, 0xabcU);
    bus.convert(0x90U, 0x123U);
    bus.contact(true);
    ASSERT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_SUCCESS);
    EXPECT_EQ(output.x, 0xabcU);
    EXPECT_EQ(output.y, 0x123U);
    EXPECT_FALSE(output.pressure_valid);
    EXPECT_EQ(output.z1, 0U);
    EXPECT_EQ(output.z2, 0U);
    EXPECT_EQ(output.observed.domain, 3U);
    EXPECT_EQ(output.observed.epoch, 7U);
    EXPECT_EQ(output.observed.ticks, bus.now);
    EXPECT_EQ(output.observed.source, NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    EXPECT_FALSE(output.observed.uncertainty_known);
}

TEST_F(XPT2046, PressureUsesExactAdditionalChannelsAndKeepsRawValues) {
    config.pressure = true;
    initialize();
    bus.contact(true);
    bus.convert(0xd0U, 1U);
    bus.convert(0x90U, 4095U);
    bus.convert(0xb0U, 0U);
    bus.convert(0xc0U, 2048U);
    bus.contact(true);
    ASSERT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_SUCCESS);
    EXPECT_EQ(output.x, 1U);
    EXPECT_EQ(output.y, 4095U);
    EXPECT_EQ(output.z1, 0U);
    EXPECT_EQ(output.z2, 2048U);
    EXPECT_TRUE(output.pressure_valid);
}

TEST_F(XPT2046, PenUpBeforeSamplingDoesNotIssueConversions) {
    initialize();
    auto before = poison();
    bus.contact(false);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_ERROR_EMPTY);
    unchanged(before);
    EXPECT_TRUE(device.initialized);
}

TEST_F(XPT2046, PenReleaseDuringSequenceKeepsOutputUnchanged) {
    initialize();
    auto before = poison();
    bus.contact(true);
    bus.convert(0xd0U, 123U);
    bus.convert(0x90U, 456U);
    bus.contact(false);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_ERROR_EMPTY);
    unchanged(before);
    EXPECT_TRUE(device.initialized);
}

TEST_F(XPT2046, TransportErrorClosesSamplingUntilSameBindingReinitializes) {
    initialize();
    auto before = poison();
    bus.contact(true);
    bus.convert(0xd0U, 123U, NX_ERROR_IO);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_ERROR_IO);
    unchanged(before);
    EXPECT_TRUE(device.uncertain);
    EXPECT_FALSE(device.initialized);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_ERROR_STATE);
    auto changed = config;
    EXPECT_EQ(nx_xpt2046_init(&device, &changed, 1000U), NX_ERROR_BUSY);
    bus.convert(0x90U);
    EXPECT_EQ(nx_xpt2046_init(&device, &config, 1000U), NX_SUCCESS);
    EXPECT_FALSE(device.uncertain);
}

TEST_F(XPT2046, ShortSuccessfulTransferIsAnUncertainIoFailure) {
    initialize();
    auto before = poison();
    bus.contact(true);
    bus.convert(0xd0U, 123U, NX_SUCCESS, 2U);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_ERROR_IO);
    unchanged(before);
    EXPECT_TRUE(device.uncertain);
}

TEST_F(XPT2046, ExpiredDeadlineAndClockRegressionCauseNoExchange) {
    initialize();
    auto before = poison();
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, bus.now), NX_ERROR_TIMEOUT);
    unchanged(before);
    bus.now = 9U;
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_ERROR_IO);
    unchanged(before);
}

TEST_F(XPT2046, DeadlineExpiringAfterFirstExchangeNeverPublishesPartialSample) {
    initialize();
    auto before = poison();
    bus.contact(true);
    bus.convert(0xd0U, 123U, NX_SUCCESS, 3U, [&] { bus.now = 1000U; });
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_ERROR_TIMEOUT);
    unchanged(before);
    EXPECT_TRUE(device.uncertain);
}

TEST_F(XPT2046, SlowConversionCannotPublishDespiteWideCallerDeadline) {
    initialize();
    const auto before = poison();
    bus.contact(true);
    bus.convert(0xd0U, 0xabcU, NX_SUCCESS, 3U, [&] { bus.now += 1601U; });
    /* The old implementation continued and published this valid Y frame.
     * Optional successors expose that behavior without requiring the fix to
     * perform an exchange after the first timed-out conversion. */
    EXPECT_CALL(bus, Transfer(_, _, 3U, _, _))
        .Times(testing::AtMost(1))
        .InSequence(bus.sequence)
        .WillOnce(Invoke([](const uint8_t*, uint8_t* rx, size_t, nx_time_us_t,
                            size_t* done) {
            rx[0] = 0U;
            rx[1] = 0U;
            rx[2] = 8U;
            *done = 3U;
            return NX_SUCCESS;
        }));
    EXPECT_CALL(bus, Pen(_))
        .Times(testing::AtMost(1))
        .InSequence(bus.sequence)
        .WillOnce(Invoke([](uint32_t* level) {
            *level = 0U;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 100000U), NX_ERROR_TIMEOUT);
    unchanged(before);
    EXPECT_FALSE(device.initialized);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(device.config, &config);
}

TEST_F(XPT2046, InitRejectsAtTheConversionBoundaryAndRetainsPowerDownDuty) {
    bus.convert(0x90U, 0U, NX_SUCCESS, 3U, [&] { bus.now += 1600U; });
    EXPECT_EQ(nx_xpt2046_init(&device, &config, 100000U), NX_ERROR_TIMEOUT);
    EXPECT_FALSE(device.initialized);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(device.config, &config);
}

TEST_F(XPT2046, StopRejectsLatePowerDownAndKeepsTheSameCleanupBinding) {
    initialize();
    bus.convert(0x90U, 0U, NX_SUCCESS, 3U, [&] { bus.now += 1600U; });
    EXPECT_EQ(nx_xpt2046_stop(&device, 100000U), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(device.config, &config);
}

TEST_F(XPT2046, FiniteConversionBudgetCannotOverflowIntoAnInfiniteDeadline) {
    bus.now = UINT64_MAX - 100U;
    EXPECT_EQ(nx_xpt2046_init(&device, &config, UINT64_MAX - 1U),
              NX_ERROR_EXHAUSTED);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(device.config, &config);
}

TEST_F(XPT2046, EndpointReceivesTheEarlierOfConversionAndCallerDeadlines) {
    EXPECT_CALL(bus, Transfer(_, _, 3U, 1610U, _))
        .WillOnce(Invoke([](const uint8_t*, uint8_t* rx, size_t, nx_time_us_t,
                            size_t* done) {
            std::memset(rx, 0, 3U);
            *done = 3U;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(nx_xpt2046_init(&device, &config, 100000U), NX_SUCCESS);
    EXPECT_CALL(bus, Transfer(_, _, 3U, 1000U, _))
        .WillOnce(Invoke([](const uint8_t*, uint8_t* rx, size_t, nx_time_us_t,
                            size_t* done) {
            std::memset(rx, 0, 3U);
            *done = 3U;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(nx_xpt2046_stop(&device, 1000U), NX_SUCCESS);
}

TEST_F(XPT2046, SeparateConversionsMayEachFinishBeforeTheirOwnBoundary) {
    initialize();
    bus.contact(true);
    bus.convert(0xd0U, 0xabcU, NX_SUCCESS, 3U, [&] { bus.now += 1599U; });
    bus.convert(0x90U, 0x123U, NX_SUCCESS, 3U, [&] { bus.now += 1599U; });
    bus.contact(true);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 100000U), NX_SUCCESS);
    EXPECT_EQ(output.x, 0xabcU);
    EXPECT_EQ(output.y, 0x123U);
}

TEST_F(XPT2046, TransportFailureRemainsPrimaryOverLateCompletion) {
    bus.convert(0x90U, 0U, NX_ERROR_PERMISSION, 0U, [&] { bus.now += 1600U; });
    EXPECT_EQ(nx_xpt2046_init(&device, &config, 100000U), NX_ERROR_PERMISSION);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(device.config, &config);
}

TEST_F(XPT2046, InvalidConfigAndOutputAliasesHaveNoIo) {
    auto invalid = config;
    invalid.pen_mask = 3U;
    EXPECT_EQ(nx_xpt2046_init(&device, &invalid, 1000U), NX_ERROR_INVALID);
    invalid = config;
    invalid.clock_domain = 0U;
    EXPECT_EQ(nx_xpt2046_init(&device, &invalid, 1000U), NX_ERROR_INVALID);
    initialize();
    EXPECT_EQ(nx_xpt2046_sample(&device,
                                reinterpret_cast<nx_xpt2046_sample_t*>(&device),
                                1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
}

TEST_F(XPT2046, CleanupFailureRetainsBindingAndRetryActuallySendsPowerDown) {
    initialize();
    bus.convert(0x90U, 0U, NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_xpt2046_stop(&device, 1000U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(device.config, &config);
    EXPECT_TRUE(device.uncertain);
    bus.convert(0x90U);
    EXPECT_EQ(nx_xpt2046_stop(&device, 2000U), NX_SUCCESS);
    EXPECT_EQ(device.config, nullptr);
    EXPECT_FALSE(device.initialized);
    EXPECT_FALSE(device.uncertain);
}

TEST_F(XPT2046, CallbackReentryCannotSampleOrReleaseTheDevice) {
    initialize();
    bus.contact(true);
    bus.convert(0xd0U, 123U, NX_SUCCESS, 3U, [&] {
        nx_xpt2046_sample_t nested{};
        EXPECT_EQ(nx_xpt2046_sample(&device, &nested, 1000U), NX_ERROR_BUSY);
        EXPECT_EQ(nx_xpt2046_stop(&device, 1000U), NX_ERROR_BUSY);
    });
    bus.convert(0x90U, 456U);
    bus.contact(true);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_SUCCESS);
}

TEST_F(XPT2046, InitialClockCallbackCannotIssueASecondInitialization) {
    bool called = false;
    bus.on_clock = [&] {
        if (!called) {
            called = true;
            EXPECT_EQ(nx_xpt2046_init(&device, &config, 1000U), NX_ERROR_BUSY);
            EXPECT_EQ(nx_xpt2046_stop(&device, 1000U), NX_ERROR_BUSY);
        }
    };
    bus.convert(0x90U);
    EXPECT_EQ(nx_xpt2046_init(&device, &config, 1000U), NX_SUCCESS);
    EXPECT_TRUE(called);
}

TEST_F(XPT2046, NonzeroNullBitRejectsBeforeAnotherConversion) {
    initialize();
    auto before = poison();
    bus.contact(true);
    bus.convert(0xd0U, 123U, NX_SUCCESS, 3U, {}, 0x8000U);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_ERROR_IO);
    unchanged(before);
    EXPECT_TRUE(device.uncertain);
}

TEST_F(XPT2046, NonzeroTrailingBitRejectsBeforeAnotherConversion) {
    initialize();
    auto before = poison();
    bus.contact(true);
    bus.convert(0xd0U, 123U, NX_SUCCESS, 3U, {}, 1U);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_ERROR_IO);
    unchanged(before);
    EXPECT_TRUE(device.uncertain);
}

TEST_F(XPT2046, ExtiOwnerSuppliesLivePenWithoutASecondGpioOwner) {
    config.pen = nullptr;
    config.pen_mask = 0U;
    config.pen_exti = &bus.exti;
    initialize();
    bus.level_contact(true);
    bus.convert(0xd0U, 123U);
    bus.convert(0x90U, 456U);
    bus.level_contact(true);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_SUCCESS);
    EXPECT_EQ(output.x, 123U);
    EXPECT_EQ(output.y, 456U);
}

TEST_F(XPT2046, MissingOrConflictingPenSourcesRejectBeforeSpi) {
    config.pen_exti = &bus.exti;
    EXPECT_EQ(nx_xpt2046_init(&device, &config, 1000U), NX_ERROR_INVALID);
    config.pen = nullptr;
    EXPECT_EQ(nx_xpt2046_init(&device, &config, 1000U), NX_ERROR_INVALID);
    config.pen_exti = nullptr;
    config.pen_mask = 0U;
    EXPECT_EQ(nx_xpt2046_init(&device, &config, 1000U), NX_ERROR_INVALID);
}

TEST_F(XPT2046, ExtiWithoutLiveLevelIsUnsupportedAndDoesNotFallBackToEdges) {
    static const nx_exti_ops_t unsupported_ops = {};
    nx_exti_port_t unsupported{&unsupported_ops, &bus};
    config.pen = nullptr;
    config.pen_mask = 0U;
    config.pen_exti = &unsupported;
    EXPECT_EQ(nx_xpt2046_init(&device, &config, 1000U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(device.config, nullptr);
}

TEST_F(XPT2046, ExtiLevelFailureClosesSamplingWithoutGpioOrEventFallback) {
    config.pen = nullptr;
    config.pen_mask = 0U;
    config.pen_exti = &bus.exti;
    initialize();
    auto before = poison();
    bus.level_contact(false, NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_xpt2046_sample(&device, &output, 1000U), NX_ERROR_UNSUPPORTED);
    unchanged(before);
    EXPECT_TRUE(device.uncertain);
}

TEST_F(XPT2046, ExtiFaceCannotAliasCopiedOutputOrConsumeThePenPredicate) {
    struct alignas(nx_xpt2046_sample_t) FaceStorage {
        nx_exti_port_t face;
        std::array<uint8_t, sizeof(nx_xpt2046_sample_t)> extra;
    } isolated{{&TouchBus::exti_ops, &bus}, {}};
    config.pen = nullptr;
    config.pen_mask = 0U;
    config.pen_exti = &isolated.face;
    initialize();
    EXPECT_EQ(nx_xpt2046_sample(
                  &device,
                  reinterpret_cast<nx_xpt2046_sample_t*>(&isolated.face),
                  1000U),
              NX_ERROR_INVALID);
}
} /* namespace */
