/**
 * \file            ili9341_test.cpp
 * \brief           ILI9341 real protocol, finite time and settled-span tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/ili9341.h"
#include <array>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using testing::_;
using testing::Invoke;
using testing::StrictMock;

/** \brief Model only the external settled parallel transport and clock. */
class Bus {
  public:
    MOCK_METHOD(nx_result_t, Command, (uint16_t, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Write,
                (const uint16_t*, size_t, size_t*, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Read, (uint16_t*, size_t, size_t*, nx_time_us_t));
    testing::Sequence order;
    nx_time_us_t now{};
    std::function<void()> on_clock;
    static nx_result_t command(void* c, uint16_t v, nx_time_us_t d) {
        return static_cast<Bus*>(c)->Command(v, d);
    }
    static nx_result_t write(void* c, const uint16_t* v, size_t n, size_t* done,
                             nx_time_us_t d) {
        return static_cast<Bus*>(c)->Write(v, n, done, d);
    }
    static nx_result_t read(void* c, uint16_t* v, size_t n, size_t* done,
                            nx_time_us_t d) {
        return static_cast<Bus*>(c)->Read(v, n, done, d);
    }
    static nx_time_us_t clock(void* c) {
        auto* bus = static_cast<Bus*>(c);
        if (bus->on_clock)
            bus->on_clock();
        return bus->now;
    }
    static constexpr nx_parallel16_ops_t ops{command, write, read};
    nx_parallel16_endpoint_t face{&ops, this};
    nx_ili9341_config_t config{&face, {clock, this}, 0U};
    nx_parallel16_endpoint_t port() {
        return face;
    }
    void cmd(uint8_t c, nx_result_t result = NX_SUCCESS) {
        EXPECT_CALL(*this, Command(c, _))
            .InSequence(order)
            .WillOnce(testing::Return(result));
    }
    void words(std::initializer_list<uint16_t> expected) {
        const std::vector<uint16_t> copy(expected);
        EXPECT_CALL(*this, Write(_, copy.size(), _, _))
            .InSequence(order)
            .WillOnce(Invoke([copy](const uint16_t* v, size_t n, size_t* done,
                                    nx_time_us_t) {
                EXPECT_EQ(std::vector<uint16_t>(v, v + n), copy);
                *done = n;
                return NX_SUCCESS;
            }));
    }
    void id(uint16_t high = 0x93U, uint16_t low = 0x41U) {
        cmd(0xd3U);
        EXPECT_CALL(*this, Read(_, 4U, _, _))
            .InSequence(order)
            .WillOnce(
                Invoke([=](uint16_t* v, size_t n, size_t* done, nx_time_us_t) {
                    v[0] = 0U;
                    v[1] = 0U;
                    v[2] = high;
                    v[3] = low;
                    *done = n;
                    return NX_SUCCESS;
                }));
    }
    void ready(nx_ili9341_t& device, uint8_t madctl = 0U) {
        config.madctl = madctl;
        id();
        cmd(0x01U);
        ASSERT_EQ(nx_ili9341_initialize(&device, &config, 500000U), NX_SUCCESS);
        now = 120000U;
        cmd(0x11U);
        ASSERT_EQ(nx_ili9341_service(&device), NX_ERROR_BUSY);
        now = 240000U;
        cmd(0x3aU);
        words({0x55U});
        cmd(0x36U);
        words({madctl});
        cmd(0x29U);
        ASSERT_EQ(nx_ili9341_service(&device), NX_SUCCESS);
    }
    void window() {
        cmd(0x2aU);
        words({0U, 1U, 0U, 2U});
        cmd(0x2bU);
        words({0U, 3U, 0U, 4U});
        cmd(0x2cU);
    }
};

TEST(ILI9341, InvalidAndShortBudgetCauseNoBusAccess) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    const auto port = bus.port();
    nx_ili9341_config_t cfg{&port, {Bus::clock, &bus}, 0x10U};
    EXPECT_EQ(nx_ili9341_initialize(&device, &cfg, 500000U), NX_ERROR_INVALID);
    cfg.madctl = 0U;
    EXPECT_EQ(nx_ili9341_initialize(&device, &cfg, 240000U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(device.phase, NX_ILI9341_STOPPED);
}

TEST(ILI9341, TimedInitializationNeverSpinsAndUsesActualID) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    const auto port = bus.port();
    const nx_ili9341_config_t cfg{&port, {Bus::clock, &bus}, 0x28U};
    bus.id();
    bus.cmd(0x01U);
    ASSERT_EQ(nx_ili9341_initialize(&device, &cfg, 500000U), NX_SUCCESS);
    EXPECT_EQ(device.phase, NX_ILI9341_RESET_WAIT);
    EXPECT_EQ(nx_ili9341_service(&device), NX_ERROR_BUSY);
    bus.now = 119999U;
    EXPECT_EQ(nx_ili9341_service(&device), NX_ERROR_BUSY);
    bus.now = 120000U;
    bus.cmd(0x11U);
    EXPECT_EQ(nx_ili9341_service(&device), NX_ERROR_BUSY);
    bus.now = 239999U;
    EXPECT_EQ(nx_ili9341_service(&device), NX_ERROR_BUSY);
    bus.now = 240000U;
    bus.cmd(0x3aU);
    bus.words({0x55U});
    bus.cmd(0x36U);
    bus.words({0x28U});
    bus.cmd(0x29U);
    EXPECT_EQ(nx_ili9341_service(&device), NX_SUCCESS);
    EXPECT_EQ(device.width, 320U);
    EXPECT_EQ(device.height, 240U);
    EXPECT_EQ(nx_ili9341_service(&device), NX_SUCCESS);
}

TEST(ILI9341, IdentityMismatchRetainsUncertainBinding) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    const auto port = bus.port();
    const nx_ili9341_config_t cfg{&port, {Bus::clock, &bus}, 0U};
    bus.id(0x93U, 0x42U);
    EXPECT_EQ(nx_ili9341_initialize(&device, &cfg, 500000U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(device.phase, NX_ILI9341_UNCERTAIN);
    EXPECT_EQ(device.config->endpoint->context, &bus);
    bus.cmd(0x28U);
    EXPECT_EQ(nx_ili9341_stop(&device, 600000U), NX_SUCCESS);
    EXPECT_EQ(device.phase, NX_ILI9341_STOPPED);
}

TEST(ILI9341, WrongIdentityReadLengthIsNotSuccess) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    const auto port = bus.port();
    const nx_ili9341_config_t cfg{&port, {Bus::clock, &bus}, 0U};
    bus.cmd(0xd3U);
    EXPECT_CALL(bus, Read(_, 4U, _, _))
        .InSequence(bus.order)
        .WillOnce(Invoke([](uint16_t*, size_t, size_t* done, nx_time_us_t) {
            *done = 3U;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(nx_ili9341_initialize(&device, &cfg, 500000U), NX_ERROR_IO);
    EXPECT_EQ(device.phase, NX_ILI9341_UNCERTAIN);
}

TEST(ILI9341, ResetTimeoutDoesNotForgetLiveBinding) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    const auto port = bus.port();
    const nx_ili9341_config_t cfg{&port, {Bus::clock, &bus}, 0U};
    bus.id();
    bus.cmd(0x01U);
    ASSERT_EQ(nx_ili9341_initialize(&device, &cfg, 240001U), NX_SUCCESS);
    bus.now = 240001U;
    EXPECT_EQ(nx_ili9341_service(&device), NX_ERROR_TIMEOUT);
    EXPECT_EQ(device.phase, NX_ILI9341_UNCERTAIN);
    bus.cmd(0x28U);
    EXPECT_EQ(nx_ili9341_stop(&device, 500000U), NX_SUCCESS);
}

TEST(ILI9341, ClockRegressionIsReportedAndRetained) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    const auto port = bus.port();
    const nx_ili9341_config_t cfg{&port, {Bus::clock, &bus}, 0U};
    bus.now = 100U;
    bus.id();
    bus.cmd(0x01U);
    ASSERT_EQ(nx_ili9341_initialize(&device, &cfg, 500000U), NX_SUCCESS);
    bus.now = 99U;
    EXPECT_EQ(nx_ili9341_service(&device), NX_ERROR_STATE);
    EXPECT_EQ(device.phase, NX_ILI9341_UNCERTAIN);
}

TEST(ILI9341, RectangleUsesColumnPageAndExactRGB565Span) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    bus.ready(device);
    bus.window();
    bus.words({0xf800U, 0x07e0U, 0x001fU, 0xffffU});
    const std::array<uint16_t, 4> pixels{0xf800U, 0x07e0U, 0x001fU, 0xffffU};
    size_t done = 99U;
    EXPECT_EQ(nx_ili9341_write(&device, 1U, 3U, 2U, 2U, pixels.data(),
                               pixels.size(), &done, 500000U),
              NX_SUCCESS);
    EXPECT_EQ(done, pixels.size());
    EXPECT_EQ(device.phase, NX_ILI9341_READY);
}

TEST(ILI9341, InvalidRectangleLeavesOutputAndPanelUnchanged) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    bus.ready(device);
    std::array<uint16_t, 4> pixels{};
    size_t done = 99U;
    EXPECT_EQ(nx_ili9341_write(&device, 239U, 3U, 2U, 2U, pixels.data(), 4U,
                               &done, 500000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_ili9341_write(&device, 1U, 3U, 2U, 2U, pixels.data(), 3U,
                               &done, 500000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_ili9341_write(&device, 1U, 3U, 0U, 2U, pixels.data(), 0U,
                               &done, 500000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(done, 99U);
    EXPECT_EQ(device.phase, NX_ILI9341_READY);
}

TEST(ILI9341, PartialPixelFailureReportsPrefixAndReleasesSpan) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    bus.ready(device);
    bus.window();
    EXPECT_CALL(bus, Write(_, 4U, _, _))
        .InSequence(bus.order)
        .WillOnce(
            Invoke([](const uint16_t*, size_t, size_t* done, nx_time_us_t) {
                *done = 2U;
                return NX_ERROR_IO;
            }));
    const std::array<uint16_t, 4> pixels{};
    size_t done = 99U;
    EXPECT_EQ(nx_ili9341_write(&device, 1U, 3U, 2U, 2U, pixels.data(), 4U,
                               &done, 500000U),
              NX_ERROR_IO);
    EXPECT_EQ(done, 2U);
    EXPECT_EQ(device.phase, NX_ILI9341_UNCERTAIN);
    EXPECT_EQ(nx_ili9341_write(&device, 1U, 3U, 2U, 2U, pixels.data(), 4U,
                               &done, 500000U),
              NX_ERROR_STATE);
}

TEST(ILI9341, SuccessfulShortPixelTransferIsProtocolFailure) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    bus.ready(device);
    bus.window();
    EXPECT_CALL(bus, Write(_, 4U, _, _))
        .InSequence(bus.order)
        .WillOnce(
            Invoke([](const uint16_t*, size_t, size_t* done, nx_time_us_t) {
                *done = 3U;
                return NX_SUCCESS;
            }));
    const std::array<uint16_t, 4> pixels{};
    size_t done{};
    EXPECT_EQ(nx_ili9341_write(&device, 1U, 3U, 2U, 2U, pixels.data(), 4U,
                               &done, 500000U),
              NX_ERROR_IO);
    EXPECT_EQ(done, 3U);
    EXPECT_EQ(device.phase, NX_ILI9341_UNCERTAIN);
}

TEST(ILI9341, StopFailureRetainsBindingForRetry) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    bus.ready(device);
    bus.cmd(0x28U, NX_ERROR_IO);
    EXPECT_EQ(nx_ili9341_stop(&device, 500000U), NX_ERROR_IO);
    EXPECT_EQ(device.phase, NX_ILI9341_UNCERTAIN);
    EXPECT_EQ(device.config->endpoint->context, &bus);
    bus.cmd(0x28U);
    EXPECT_EQ(nx_ili9341_stop(&device, 600000U), NX_SUCCESS);
    EXPECT_EQ(nx_ili9341_stop(&device, 600000U), NX_SUCCESS);
}

TEST(ILI9341, FreshStopNeedsNoTransport) {
    nx_ili9341_t device{};
    EXPECT_EQ(nx_ili9341_stop(&device, 1U), NX_SUCCESS);
    EXPECT_EQ(nx_ili9341_service(&device), NX_ERROR_STATE);
}

TEST(ILI9341, OutputAliasAndWrappedSpanRejectBeforeBusAccess) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    bus.ready(device);
    alignas(size_t) std::array<uint16_t, 4> pixels{};
    auto* output = reinterpret_cast<size_t*>(pixels.data());
    EXPECT_EQ(nx_ili9341_write(&device, 1U, 3U, 2U, 2U, pixels.data(), 4U,
                               output, 500000U),
              NX_ERROR_INVALID);
    size_t done = 99U;
    const auto* wrapped = reinterpret_cast<const uint16_t*>(UINTPTR_MAX - 1U);
    EXPECT_EQ(
        nx_ili9341_write(&device, 1U, 3U, 2U, 2U, wrapped, 4U, &done, 500000U),
        NX_ERROR_INVALID);
    EXPECT_EQ(done, 99U);
}

TEST(ILI9341, ExpiredWriteRejectsWithoutLosingReadyState) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    bus.ready(device);
    std::array<uint16_t, 4> pixels{};
    size_t done = 99U;
    EXPECT_EQ(nx_ili9341_write(&device, 1U, 3U, 2U, 2U, pixels.data(), 4U,
                               &done, bus.now),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(done, 99U);
    EXPECT_EQ(device.phase, NX_ILI9341_READY);
}

TEST(ILI9341, LiveRebindRejectsAndInstancesAreIndependent) {
    StrictMock<Bus> first;
    StrictMock<Bus> second;
    nx_ili9341_t a{};
    nx_ili9341_t b{};
    first.ready(a);
    const auto port = second.port();
    const nx_ili9341_config_t cfg{&port, {Bus::clock, &second}, 0U};
    EXPECT_EQ(nx_ili9341_initialize(&a, &cfg, 500000U), NX_ERROR_BUSY);
    second.ready(b);
    EXPECT_EQ(a.config->endpoint->context, &first);
    EXPECT_EQ(b.config->endpoint->context, &second);
}

TEST(ILI9341, StopDoesNotIssueACommandDuringResetRecovery) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    const auto port = bus.port();
    const nx_ili9341_config_t cfg{&port, {Bus::clock, &bus}, 0U};
    bus.id();
    bus.cmd(0x01U);
    ASSERT_EQ(nx_ili9341_initialize(&device, &cfg, 500000U), NX_SUCCESS);
    EXPECT_EQ(nx_ili9341_stop(&device, 500000U), NX_ERROR_BUSY);
    EXPECT_EQ(device.phase, NX_ILI9341_RESET_WAIT);
    bus.now = 120000U;
    bus.cmd(0x28U);
    EXPECT_EQ(nx_ili9341_stop(&device, 500000U), NX_SUCCESS);
}

TEST(ILI9341, FailedResetAttemptStillEnforcesQuietTimeBeforeRecovery) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    const auto port = bus.port();
    const nx_ili9341_config_t cfg{&port, {Bus::clock, &bus}, 0U};
    bus.id();
    bus.cmd(0x01U, NX_ERROR_IO);
    EXPECT_EQ(nx_ili9341_initialize(&device, &cfg, 500000U), NX_ERROR_IO);
    EXPECT_EQ(device.phase, NX_ILI9341_UNCERTAIN);
    EXPECT_EQ(nx_ili9341_initialize(&device, &cfg, 500000U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_ili9341_stop(&device, 500000U), NX_ERROR_BUSY);
    bus.now = 120000U;
    bus.cmd(0x28U);
    EXPECT_EQ(nx_ili9341_stop(&device, 500000U), NX_SUCCESS);
}
TEST(ILI9341, ClockCallbackCannotReenterInitialization) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    const auto port = bus.port();
    const nx_ili9341_config_t cfg{&port, {Bus::clock, &bus}, 0U};
    bool first = true;
    nx_result_t reentry = NX_SUCCESS;
    bus.on_clock = [&] {
        if (first) {
            first = false;
            reentry = nx_ili9341_stop(&device, 500000U);
        }
    };
    bus.id();
    bus.cmd(0x01U);
    ASSERT_EQ(nx_ili9341_initialize(&device, &cfg, 500000U), NX_SUCCESS);
    EXPECT_EQ(reentry, NX_ERROR_BUSY);
}

TEST(ILI9341, ClockCallbackCannotReenterRectangleOrStop) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    bus.ready(device);
    nx_result_t reentry = NX_SUCCESS;
    bool first = true;
    bus.on_clock = [&] {
        if (first) {
            first = false;
            reentry = nx_ili9341_service(&device);
        }
    };
    bus.window();
    bus.words({0U, 0U, 0U, 0U});
    const std::array<uint16_t, 4> pixels{};
    size_t done{};
    EXPECT_EQ(nx_ili9341_write(&device, 1U, 3U, 2U, 2U, pixels.data(), 4U,
                               &done, 500000U),
              NX_SUCCESS);
    EXPECT_EQ(reentry, NX_ERROR_BUSY);
    first = true;
    reentry = NX_SUCCESS;
    bus.cmd(0x28U);
    EXPECT_EQ(nx_ili9341_stop(&device, 500000U), NX_SUCCESS);
    EXPECT_EQ(reentry, NX_ERROR_BUSY);
}

TEST(ILI9341, CallerPixelsCannotAliasReadonlyConfigurationOrFace) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    bus.ready(device);
    size_t done = 91U;
    EXPECT_EQ(nx_ili9341_write(&device, 0U, 0U, 2U, 2U,
                               reinterpret_cast<const uint16_t*>(&bus.config),
                               4U, &done, 500000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(done, 91U);
    EXPECT_EQ(nx_ili9341_write(&device, 0U, 0U, 2U, 2U,
                               reinterpret_cast<const uint16_t*>(&bus.face), 4U,
                               &done, 500000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(done, 91U);
    EXPECT_EQ(device.phase, NX_ILI9341_READY);
}

TEST(ILI9341, TransportAnchorInsideDeviceRejectsBeforeFirstClock) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    const nx_parallel16_endpoint_t endpoint{&Bus::ops, &device};
    const nx_ili9341_config_t config{&endpoint, {Bus::clock, &bus}, 0U};
    unsigned reads = 0U;
    bus.on_clock = [&] { ++reads; };
    EXPECT_EQ(nx_ili9341_initialize(&device, &config, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(reads, 0U);
    EXPECT_EQ(device.phase, NX_ILI9341_STOPPED);
}

TEST(ILI9341, PixelsCannotCrossKnownTransportContextAnchor) {
    StrictMock<Bus> bus;
    nx_ili9341_t device{};
    bus.ready(device);
    size_t done = 91U;
    EXPECT_EQ(nx_ili9341_write(&device, 0U, 0U, 2U, 2U,
                               reinterpret_cast<const uint16_t*>(&bus), 4U,
                               &done, 500000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(done, 91U);
    EXPECT_EQ(device.phase, NX_ILI9341_READY);
}

TEST(ILI9341, ProgressCannotOverwriteKnownClockContextAnchor) {
    StrictMock<Bus> bus;
    bus.config.clock = {[](void* c) { return *static_cast<nx_time_us_t*>(c); },
                        &bus.now};
    nx_ili9341_t device{};
    bus.ready(device);
    uint16_t pixels[4]{};
    auto* progress = reinterpret_cast<size_t*>(&bus.now);
    EXPECT_EQ(nx_ili9341_write(&device, 0U, 0U, 2U, 2U, pixels, 4U, progress,
                               500000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(bus.now, 240000U);
    EXPECT_EQ(device.phase, NX_ILI9341_READY);
}
} /* namespace */
