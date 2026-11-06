/**
 * \file            software_spi_test.cpp
 * \brief           GPIO wire-level SPI timing and failed-withdrawal contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/software_spi.h"
#include <array>
#include <functional>
#include <gtest/gtest.h>
#include <vector>

namespace {
/** \brief Observe only external GPIO pins, edges, clock and injected faults. */
class Wire {
  public:
    struct Pin {
        Wire* wire;
        unsigned id;
        bool high;
    };
    struct Event {
        unsigned pin;
        bool high;
        nx_time_us_t at;
    };
    std::array<Pin, 4> pins{{{this, 0U, false},
                             {this, 1U, false},
                             {this, 2U, false},
                             {this, 3U, true}}};
    std::array<nx_gpio_port_t, 4> ports{};
    std::vector<Event> events;
    std::vector<bool> incoming;
    std::vector<bool> transmitted;
    size_t input_position{};
    nx_time_us_t now{};
    unsigned clock_reads{};
    unsigned writes{};
    unsigned fail_write{};
    bool freeze{};
    bool fail_cs_high{};
    bool fail_cs_low{};
    std::function<void()> on_edge;
    std::function<void()> on_clock;
    Wire() {
        for (unsigned i = 0U; i < ports.size(); ++i) {
            ports[i] = {&ops, &pins[i]};
        }
    }
    static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
        auto& pin = *static_cast<Pin*>(context);
        Wire& wire = *pin.wire;
        ++wire.writes;
        EXPECT_EQ(set | reset, 1U);
        bool high = set != 0U;
        if (wire.writes == wire.fail_write ||
            (pin.id == 3U &&
             ((high && wire.fail_cs_high) || (!high && wire.fail_cs_low)))) {
            return NX_ERROR_IO;
        }
        pin.high = high;
        wire.events.push_back({pin.id, high, wire.now});
        if (pin.id == 0U && high && !wire.pins[3].high) {
            wire.transmitted.push_back(wire.pins[1].high);
            if (wire.on_edge) {
                wire.on_edge();
            }
        }
        return NX_SUCCESS;
    }
    static nx_result_t read(const void* context, uint32_t* out) {
        auto& pin = *static_cast<const Pin*>(context);
        Wire& wire = *pin.wire;
        EXPECT_EQ(pin.id, 2U);
        EXPECT_TRUE(wire.pins[0].high);
        EXPECT_FALSE(wire.pins[3].high);
        *out = wire.input_position < wire.incoming.size() &&
                       wire.incoming[wire.input_position]
                   ? 1U
                   : 0U;
        ++wire.input_position;
        return NX_SUCCESS;
    }
    static nx_time_us_t clock(void* context) {
        auto& wire = *static_cast<Wire*>(context);
        ++wire.clock_reads;
        if (!wire.freeze) {
            ++wire.now;
        }
        if (wire.on_clock) {
            wire.on_clock();
        }
        return wire.now;
    }
    static const nx_gpio_ops_t ops;
    nx_software_spi_config_t config() {
        return {{&ports[0], 1U},
                {&ports[1], 1U},
                {&ports[2], 1U},
                {&ports[3], 1U},
                {clock, this},
                2U,
                3U,
                4U,
                16U,
                16U};
    }
    void receive(std::initializer_list<uint8_t> bytes) {
        incoming.clear();
        input_position = 0U;
        for (uint8_t byte : bytes) {
            for (unsigned bit = 0U; bit < 8U; ++bit) {
                incoming.push_back((byte & (0x80U >> bit)) != 0U);
            }
        }
    }
    std::vector<uint8_t> sent() const {
        std::vector<uint8_t> bytes;
        for (size_t i = 0U; i + 7U < transmitted.size(); i += 8U) {
            uint8_t byte = 0U;
            for (size_t j = 0U; j < 8U; ++j) {
                byte = static_cast<uint8_t>((byte << 1U) |
                                            (transmitted[i + j] ? 1U : 0U));
            }
            bytes.push_back(byte);
        }
        return bytes;
    }
    void clear() {
        events.clear();
        transmitted.clear();
        writes = 0U;
        clock_reads = 0U;
    }
};
const nx_gpio_ops_t Wire::ops = {Wire::write, Wire::read, nullptr};

class SoftwareSPI : public testing::Test {
  protected:
    Wire wire;
    nx_software_spi_config_t config = wire.config();
    nx_software_spi_t owner{};
    nx_spi_endpoint_t endpoint = nx_software_spi_endpoint(&owner);
    nx_spi_port_t port = nx_software_spi_port(&owner);
    void initialize() {
        ASSERT_EQ(nx_software_spi_init(&owner, &config), NX_SUCCESS);
        wire.clear();
    }
    nx_result_t exchange(const uint8_t* tx, uint8_t* rx, size_t length,
                         size_t* count, nx_time_us_t deadline = 10000U) {
        return nx_spi_endpoint_transfer(&endpoint, tx, rx, length, deadline,
                                        count);
    }
};

TEST_F(SoftwareSPI, InitEstablishesInactiveSelectBeforeClockAndSharesMethods) {
    ASSERT_EQ(nx_software_spi_init(&owner, &config), NX_SUCCESS);
    ASSERT_EQ(wire.events.size(), 3U);
    EXPECT_EQ(wire.events[0].pin, 3U);
    EXPECT_TRUE(wire.events[0].high);
    EXPECT_EQ(wire.events[1].pin, 0U);
    EXPECT_FALSE(wire.events[1].high);
    EXPECT_FALSE(wire.events[2].high);
    nx_software_spi_t other{};
    EXPECT_EQ(nx_software_spi_endpoint(&other).ops, endpoint.ops);
    EXPECT_EQ(nx_software_spi_port(&other).ops, port.ops);
    EXPECT_FALSE(nx_software_spi_faulted(&owner));
}

TEST_F(SoftwareSPI, InvalidConfigurationNeverWritesPins) {
    auto bad = config;
    bad.cs.mask = 3U;
    EXPECT_EQ(nx_software_spi_init(&owner, &bad), NX_ERROR_INVALID);
    bad = config;
    bad.mosi = bad.sck;
    EXPECT_EQ(nx_software_spi_init(&owner, &bad), NX_ERROR_INVALID);
    bad = config;
    bad.max_bytes = 257U;
    EXPECT_EQ(nx_software_spi_init(&owner, &bad), NX_ERROR_INVALID);
    bad = config;
    bad.poll_limit = 0U;
    EXPECT_EQ(nx_software_spi_init(&owner, &bad), NX_ERROR_INVALID);
    bad = config;
    bad.half_period_us = 0U;
    EXPECT_EQ(nx_software_spi_init(&owner, &bad), NX_ERROR_INVALID);
    EXPECT_EQ(wire.writes, 0U);
}

TEST_F(SoftwareSPI, FullDuplexUsesOneSelectAndModeZeroMsbFirstEdges) {
    initialize();
    wire.receive({0x96U, 0x3cU});
    const uint8_t tx[] = {0xa5U, 0x19U};
    uint8_t rx[2]{};
    size_t count = 99U;
    ASSERT_EQ(exchange(tx, rx, 2U, &count), NX_SUCCESS);
    EXPECT_EQ(count, 2U);
    EXPECT_EQ(rx[0], 0x96U);
    EXPECT_EQ(rx[1], 0x3cU);
    EXPECT_EQ(wire.sent(), (std::vector<uint8_t>{0xa5U, 0x19U}));
    ASSERT_FALSE(wire.events.empty());
    EXPECT_EQ(wire.events.front().pin, 3U);
    EXPECT_FALSE(wire.events.front().high);
    unsigned selections = 0U;
    nx_time_us_t previous_edge = 0U;
    bool first = true;
    for (const auto& event : wire.events) {
        if (event.pin == 3U && !event.high) {
            ++selections;
        }
        if (event.pin == 0U) {
            if (!first) {
                EXPECT_GE(event.at - previous_edge, config.half_period_us);
            }
            first = false;
            previous_edge = event.at;
        }
    }
    EXPECT_EQ(selections, 1U);
    EXPECT_TRUE(wire.pins[3].high);
    EXPECT_FALSE(wire.pins[0].high);
}

TEST_F(SoftwareSPI, ReceiveOnlySendsDummyOnesAndExactInPlaceIsSafe) {
    initialize();
    wire.receive({0x12U});
    uint8_t byte = 0U;
    size_t count = 0U;
    ASSERT_EQ(exchange(nullptr, &byte, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(byte, 0x12U);
    EXPECT_EQ(wire.sent(), (std::vector<uint8_t>{0xffU}));
    wire.clear();
    wire.receive({0xe1U});
    byte = 0x35U;
    ASSERT_EQ(exchange(&byte, &byte, 1U, &count), NX_SUCCESS);
    EXPECT_EQ(byte, 0xe1U);
    EXPECT_EQ(wire.sent(), (std::vector<uint8_t>{0x35U}));
}

TEST_F(SoftwareSPI, PartialOverlapAndControlAliasesRejectWithoutPinWrites) {
    initialize();
    uint8_t bytes[3]{};
    size_t count = 0U;
    EXPECT_EQ(exchange(bytes, bytes + 1U, 2U, &count), NX_ERROR_INVALID);
    EXPECT_EQ(exchange(reinterpret_cast<uint8_t*>(&owner), nullptr, 1U, &count),
              NX_ERROR_INVALID);
    EXPECT_EQ(exchange(bytes, nullptr, 1U, reinterpret_cast<size_t*>(&owner)),
              NX_ERROR_INVALID);
    EXPECT_TRUE(wire.events.empty());
}

TEST_F(SoftwareSPI, InvalidLengthAndExpiredDeadlineNeverSelect) {
    initialize();
    const uint8_t byte = 0x80U;
    size_t count = 99U;
    EXPECT_EQ(exchange(&byte, nullptr, 17U, &count), NX_ERROR_INVALID);
    EXPECT_EQ(exchange(&byte, nullptr, 1U, &count, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(exchange(&byte, nullptr, 1U, &count, wire.now), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(wire.events.empty());
    EXPECT_FALSE(owner.faulted);
}

TEST_F(SoftwareSPI, StalledClockHasFinitePollBoundAndClosesReuse) {
    initialize();
    wire.freeze = true;
    const uint8_t tx = 0U;
    size_t count = 1U;
    EXPECT_EQ(exchange(&tx, nullptr, 1U, &count), NX_ERROR_IO);
    EXPECT_EQ(count, 0U);
    EXPECT_LE(wire.clock_reads, config.poll_limit + 3U);
    EXPECT_TRUE(nx_software_spi_faulted(&owner));
    EXPECT_TRUE(wire.pins[3].high);
    const auto writes = wire.writes;
    EXPECT_EQ(exchange(&tx, nullptr, 1U, &count), NX_ERROR_STATE);
    EXPECT_EQ(wire.writes, writes);
}

TEST_F(SoftwareSPI, SelectAssertionFailureRetainsOwnerWithoutClockingAByte) {
    initialize();
    wire.fail_cs_low = true;
    const uint8_t tx = 0U;
    size_t count = 1U;
    EXPECT_EQ(exchange(&tx, nullptr, 1U, &count), NX_ERROR_IO);
    EXPECT_EQ(count, 0U);
    EXPECT_TRUE(owner.faulted);
    EXPECT_TRUE(wire.transmitted.empty());
    EXPECT_EQ(nx_software_spi_init(&owner, &config), NX_ERROR_BUSY);
    wire.fail_cs_low = false;
    EXPECT_EQ(nx_spi_port_recover(&port), NX_SUCCESS);
    EXPECT_FALSE(owner.faulted);
}

TEST_F(SoftwareSPI, FailedSelectWithdrawalPreservesFaultUntilExplicitRecovery) {
    initialize();
    wire.fail_cs_high = true;
    const uint8_t tx = 0x55U;
    size_t count = 0U;
    EXPECT_EQ(exchange(&tx, nullptr, 1U, &count), NX_ERROR_IO);
    EXPECT_EQ(count, 1U);
    EXPECT_TRUE(owner.faulted);
    EXPECT_FALSE(wire.pins[3].high);
    const auto bits = wire.transmitted.size();
    EXPECT_EQ(nx_spi_port_recover(&port), NX_ERROR_IO);
    EXPECT_TRUE(owner.faulted);
    EXPECT_EQ(wire.transmitted.size(), bits);
    EXPECT_EQ(nx_spi_port_stop(&port), NX_ERROR_IO);
    EXPECT_NE(owner.config, nullptr);
    wire.fail_cs_high = false;
    EXPECT_EQ(nx_spi_port_recover(&port), NX_SUCCESS);
    EXPECT_TRUE(wire.pins[3].high);
    EXPECT_FALSE(wire.pins[0].high);
    EXPECT_FALSE(owner.faulted);
    EXPECT_EQ(wire.transmitted.size(), bits);
    EXPECT_EQ(nx_spi_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(owner.config, nullptr);
    EXPECT_FALSE(owner.initialized);
}

TEST_F(SoftwareSPI, PartialByteErrorNeverCountsOrPublishesTheIncompleteByte) {
    initialize();
    wire.receive({0xaaU});
    wire.fail_write = 8U;
    const uint8_t tx = 0x55U;
    uint8_t rx = 0x5aU;
    size_t count = 1U;
    EXPECT_EQ(exchange(&tx, &rx, 1U, &count), NX_ERROR_IO);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(rx, 0x5aU);
    EXPECT_TRUE(owner.faulted);
}

TEST_F(SoftwareSPI, CallbackReentryCannotWriteAnotherPinOrStopTheOwner) {
    initialize();
    const uint8_t tx = 0x55U;
    size_t count = 0U;
    bool called = false;
    wire.on_edge = [&] {
        if (!called) {
            called = true;
            size_t nested = 0U;
            const auto writes = wire.writes;
            EXPECT_EQ(exchange(&tx, nullptr, 1U, &nested), NX_ERROR_BUSY);
            EXPECT_EQ(nx_spi_port_recover(&port), NX_ERROR_BUSY);
            EXPECT_EQ(nx_spi_port_stop(&port), NX_ERROR_BUSY);
            EXPECT_EQ(wire.writes, writes);
        }
    };
    EXPECT_EQ(exchange(&tx, nullptr, 1U, &count), NX_SUCCESS);
    EXPECT_TRUE(called);
    EXPECT_EQ(count, 1U);
}

TEST_F(SoftwareSPI, IndependentOwnerFaultDoesNotInvalidateOtherInstance) {
    initialize();
    Wire second;
    auto second_config = second.config();
    nx_software_spi_t second_owner{};
    ASSERT_EQ(nx_software_spi_init(&second_owner, &second_config), NX_SUCCESS);
    auto second_endpoint = nx_software_spi_endpoint(&second_owner);
    wire.fail_cs_high = true;
    const uint8_t tx = 0U;
    size_t count = 0U;
    EXPECT_EQ(exchange(&tx, nullptr, 1U, &count), NX_ERROR_IO);
    EXPECT_EQ(nx_spi_endpoint_transfer(&second_endpoint, &tx, nullptr, 1U,
                                       10000U, &count),
              NX_SUCCESS);
    EXPECT_FALSE(second_owner.faulted);
}

TEST_F(SoftwareSPI, AsyncSubmitIsExplicitlyUnsupported) {
    initialize();
    nx_spi_request_t request{};
    EXPECT_EQ(nx_spi_endpoint_submit(&endpoint, &request),
              NX_ERROR_UNSUPPORTED);
}

TEST_F(SoftwareSPI, InitialClockCallbackCannotReenterOrReleaseUnboundOwner) {
    bool called = false;
    wire.on_clock = [&] {
        if (!called) {
            called = true;
            EXPECT_EQ(nx_software_spi_init(&owner, &config), NX_ERROR_BUSY);
            EXPECT_EQ(nx_spi_port_stop(&port), NX_ERROR_BUSY);
        }
    };
    EXPECT_EQ(nx_software_spi_init(&owner, &config), NX_SUCCESS);
    EXPECT_TRUE(called);
    EXPECT_EQ(wire.events.size(), 3U);
}
} /* namespace */
