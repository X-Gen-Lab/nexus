/**
 * \file            onewire_test.cpp
 * \brief           One-wire GPIO timing, jitter and physical ownership tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/ds18b20.h"
#include "nexus/components/onewire.h"
#include <algorithm>
#include <array>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::StrictMock;
class Pin {
  public:
    MOCK_METHOD(nx_result_t, Write, (uint32_t, uint32_t));
    MOCK_METHOD(nx_result_t, Read, (uint32_t*));
    MOCK_METHOD(nx_time_us_t, Clock, ());
    static nx_result_t write(void* p, uint32_t set, uint32_t reset) {
        return static_cast<Pin*>(p)->Write(set, reset);
    }
    static nx_result_t read(const void* p, uint32_t* out) {
        return const_cast<Pin*>(static_cast<const Pin*>(p))->Read(out);
    }
    static nx_time_us_t clock(void* p) {
        return static_cast<Pin*>(p)->Clock();
    }
    static const nx_gpio_ops_t ops;
    nx_gpio_port_t port{&ops, this};
};
const nx_gpio_ops_t Pin::ops{Pin::write, Pin::read, nullptr};
class OneWire : public testing::Test {
  protected:
    StrictMock<Pin> pin;
    nx_onewire_t owner{};
    nx_onewire_config_t config{&pin.port, 1U, {Pin::clock, &pin}, 2000U};
    nx_time_us_t time = 100U;
    nx_time_us_t low_at = 0U;
    nx_time_us_t reset_release = 0U;
    nx_time_us_t slave_low_until = 0U;
    nx_time_us_t previous_slot = 0U;
    bool master_released = true;
    bool presence = true;
    bool stuck = false;
    bool freeze_clock = false;
    bool refuse_release = false;
    bool fail_low = false;
    unsigned write_latency = 0U;
    unsigned reset_pre_latency = 0U;
    unsigned data_pre_latency = 0U;
    unsigned read_pre_latency = 0U;
    unsigned read_latency = 0U;
    unsigned writes = 0U;
    unsigned reads = 0U;
    unsigned clocks = 0U;
    size_t tx_bits = 0U;
    size_t rx_bits = 0U;
    std::vector<uint8_t> expected_tx{0xa5U};
    std::vector<uint8_t> receive{0x96U, 0x3cU};
    struct Transaction {
        std::vector<uint8_t> command;
        std::vector<uint8_t> response;
    };
    std::vector<Transaction> script;
    size_t script_index = 0U;
    std::vector<bool> sent;
    std::vector<uint64_t> low_widths;
    std::vector<uint64_t> slot_gaps;
    std::function<void()> write_hook;
    void SetUp() override {
        EXPECT_CALL(pin, Clock())
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([this] {
                ++clocks;
                auto value = time;
                if (!freeze_clock)
                    ++time;
                return value;
            }));
        EXPECT_CALL(pin, Write(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([this](uint32_t set, uint32_t reset) {
                ++writes;
                EXPECT_EQ(0U, set & reset);
                EXPECT_EQ(1U, set | reset);
                if (write_hook)
                    write_hook();
                const bool high = set != 0U;
                if (high && refuse_release)
                    return NX_ERROR_IO;
                if (!high && master_released) {
                    time += reset_release == 0U ? reset_pre_latency
                            : tx_bits < expected_tx.size() * 8U
                                ? data_pre_latency
                                : read_pre_latency;
                    low_at = time;
                    master_released = false;
                } else if (high && !master_released) {
                    auto low = time - low_at;
                    low_widths.push_back(low);
                    master_released = true;
                    if (low >= 480U) {
                        reset_release = time;
                        tx_bits = rx_bits = 0U;
                        previous_slot = 0U;
                        if (!script.empty()) {
                            EXPECT_LT(script_index, script.size());
                            if (script_index < script.size()) {
                                expected_tx = script[script_index].command;
                                receive = script[script_index].response;
                                ++script_index;
                            }
                        }
                    } else if (reset_release && time >= reset_release + 480U) {
                        if (previous_slot)
                            slot_gaps.push_back(low_at - previous_slot);
                        previous_slot = low_at;
                        if (tx_bits < expected_tx.size() * 8U) {
                            if (!script.empty()) {
                                EXPECT_EQ(bool((expected_tx[tx_bits / 8U] >>
                                                (tx_bits % 8U)) &
                                               1U),
                                          low < 15U);
                            }
                            sent.push_back(low < 15U);
                            ++tx_bits;
                        } else {
                            const bool bit =
                                rx_bits < receive.size() * 8U &&
                                ((receive[rx_bits / 8U] >> (rx_bits % 8U)) &
                                 1U);
                            if (!bit)
                                slave_low_until = low_at + 50U;
                            ++rx_bits;
                        }
                    }
                }
                if (!high)
                    time += write_latency;
                return !high && fail_low ? NX_ERROR_IO : NX_SUCCESS;
            }));
        EXPECT_CALL(pin, Read(_))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([this](uint32_t* out) {
                ++reads;
                const bool presence_low = presence && reset_release &&
                                          time >= reset_release + 20U &&
                                          time < reset_release + 140U;
                *out = master_released && !stuck && !presence_low &&
                               time >= slave_low_until
                           ? 1U
                           : 0U;
                if (rx_bits)
                    time += read_latency;
                return NX_SUCCESS;
            }));
    }
    void open() {
        ASSERT_EQ(NX_SUCCESS, nx_onewire_init(&owner, &config, 100000U));
    }
    nx_result_t exchange(uint8_t* out, size_t bits = 16U) {
        return nx_onewire_exchange(&owner, expected_tx.data(),
                                   expected_tx.size(), out, bits, 100000U);
    }
};
TEST_F(OneWire, InitOnlyReleasesAndReadsTheDeclaredOpenDrainBit) {
    open();
    EXPECT_TRUE(master_released);
    EXPECT_GT(reads, 0U);
    EXPECT_TRUE(owner.initialized);
    EXPECT_FALSE(owner.faulted);
}
TEST_F(OneWire, ExactResetPresenceSlotsAndLsbDataUseRealGpioPorts) {
    open();
    std::array<uint8_t, 2> out{};
    ASSERT_EQ(NX_SUCCESS, exchange(out.data()));
    EXPECT_EQ(receive[0], out[0]);
    EXPECT_EQ(receive[1], out[1]);
    ASSERT_EQ(8U, sent.size());
    for (size_t i = 0; i < 8U; ++i)
        EXPECT_EQ(bool((0xa5U >> i) & 1U), sent[i]);
    ASSERT_GE(low_widths.size(), 25U);
    EXPECT_GE(low_widths[0], 480U);
    EXPECT_LE(low_widths[0], 960U);
    for (auto gap : slot_gaps) {
        EXPECT_GE(gap, 60U);
        EXPECT_LE(gap, 120U);
    }
    EXPECT_TRUE(master_released);
    EXPECT_FALSE(owner.in_call);
}
TEST_F(OneWire, PartialReceiveByteClearsUnusedHighBits) {
    open();
    uint8_t out = 0xffU;
    ASSERT_EQ(NX_SUCCESS, exchange(&out, 3U));
    EXPECT_EQ(6U, out);
}
TEST_F(OneWire, ExplicitFiniteDeadlineRejectsBeforePinsOrBuffers) {
    open();
    const auto count = writes;
    uint8_t out = 0x5aU;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_onewire_exchange(&owner, expected_tx.data(), 1U, &out, 8U,
                                  NX_DEADLINE_NEVER));
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_onewire_exchange(&owner, expected_tx.data(),
                                                    1U, &out, 8U, 0U));
    EXPECT_EQ(count, writes);
    EXPECT_EQ(0x5aU, out);
}
TEST_F(OneWire, InvalidWiringAndBoundsRejectWithoutAnyPinEffect) {
    config.mask = 3U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_onewire_init(&owner, &config, 1000U));
    EXPECT_EQ(0U, writes);
    config.mask = 1U;
    open();
    auto before = writes;
    uint8_t out{};
    EXPECT_EQ(NX_ERROR_INVALID, nx_onewire_exchange(&owner, expected_tx.data(),
                                                    0U, &out, 8U, 100000U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_onewire_exchange(&owner, expected_tx.data(),
                                                    1U, &out, 129U, 100000U));
    EXPECT_EQ(before, writes);
}
TEST_F(OneWire, PayloadAndTimeAliasesCannotCorruptTheWireOwner) {
    open();
    auto before = writes;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_onewire_exchange(&owner,
                                  reinterpret_cast<const uint8_t*>(&owner), 1U,
                                  nullptr, 0U, 100000U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_onewire_now(&owner, reinterpret_cast<nx_time_us_t*>(&owner)));
    EXPECT_EQ(before, writes);
}
TEST_F(OneWire, MissingPresenceIsAnErrorAndNeverSendsACommand) {
    open();
    presence = false;
    uint8_t out{};
    EXPECT_EQ(NX_ERROR_NACK, exchange(&out, 8U));
    EXPECT_TRUE(sent.empty());
    EXPECT_TRUE(master_released);
    EXPECT_TRUE(owner.faulted);
}
TEST_F(OneWire, StalledClockStillExitsAtTheExplicitPollBudget) {
    open();
    freeze_clock = true;
    config.poll_limit = 4U;
    uint8_t out{};
    EXPECT_EQ(NX_ERROR_IO, exchange(&out, 8U));
    EXPECT_FALSE(owner.in_call);
    EXPECT_LT(clocks, 40U);
    EXPECT_TRUE(master_released);
}
TEST_F(OneWire, BackwardAndNeverClockFaultsCannotBeRetimedSilently) {
    open();
    time = 0U;
    nx_time_us_t out = 77U;
    EXPECT_EQ(NX_ERROR_IO, nx_onewire_now(&owner, &out));
    EXPECT_EQ(77U, out);
    time = 10000U;
    EXPECT_EQ(NX_ERROR_IO, nx_onewire_now(&owner, &out));
    EXPECT_EQ(77U, out);
}
TEST_F(OneWire, NonfiniteClockRejectsBeforeInitialPinWrite) {
    time = NX_DEADLINE_NEVER;
    EXPECT_EQ(NX_ERROR_IO, nx_onewire_init(&owner, &config, 10000U));
    EXPECT_EQ(0U, writes);
}
TEST_F(OneWire, ExcessiveWriteSlotJitterReturnsErrorInsteadOfTimingClaim) {
    open();
    write_latency = 20U;
    uint8_t out{};
    EXPECT_EQ(NX_ERROR_IO, exchange(&out, 8U));
    EXPECT_TRUE(owner.faulted);
    EXPECT_TRUE(master_released);
}
TEST_F(OneWire, ExcessiveReadSampleLatencyFailsBeforeSuccessfulPublication) {
    open();
    read_latency = 20U;
    uint8_t out{};
    EXPECT_EQ(NX_ERROR_IO, exchange(&out, 8U));
    EXPECT_TRUE(owner.faulted);
}
TEST_F(OneWire, FailedReleaseRetainsBindingButNoCpuPayloadOrExecutorLoan) {
    open();
    refuse_release = true;
    uint8_t out{};
    EXPECT_NE(NX_SUCCESS, exchange(&out, 8U));
    EXPECT_EQ(&config, owner.config);
    EXPECT_TRUE(owner.faulted);
    EXPECT_FALSE(owner.in_call);
    EXPECT_NE(NX_SUCCESS, nx_onewire_stop(&owner, 100000U));
    EXPECT_EQ(&config, owner.config);
    refuse_release = false;
    presence = false;
    EXPECT_EQ(NX_SUCCESS, nx_onewire_stop(&owner, 100000U));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_FALSE(owner.initialized);
}
TEST_F(OneWire, StopIsIdempotentAndDoesNotEmitAResetPulse) {
    open();
    ASSERT_EQ(NX_SUCCESS, nx_onewire_stop(&owner, 100000U));
    EXPECT_TRUE(low_widths.empty());
    auto before = writes;
    EXPECT_EQ(NX_SUCCESS, nx_onewire_stop(&owner, 100000U));
    EXPECT_EQ(before, writes);
}
TEST_F(OneWire, GpioCallbackReentryCannotStartAnotherTransfer) {
    open();
    nx_result_t nested = NX_SUCCESS;
    bool called = false;
    write_hook = [&] {
        if (!called) {
            called = true;
            nested = nx_onewire_exchange(&owner, expected_tx.data(), 1U,
                                         nullptr, 0U, 100000U);
        }
    };
    uint8_t out{};
    EXPECT_EQ(NX_SUCCESS, exchange(&out, 8U));
    EXPECT_EQ(NX_ERROR_BUSY, nested);
}
TEST_F(OneWire, AnIndependentOwnerClockOrGpioCannotBeBorrowedImplicitly) {
    nx_onewire_t other{};
    uint8_t out{};
    EXPECT_EQ(NX_ERROR_STATE, nx_onewire_exchange(&other, expected_tx.data(),
                                                  1U, &out, 8U, 100000U));
    EXPECT_EQ(0U, writes);
    open();
    EXPECT_EQ(NX_ERROR_BUSY, nx_onewire_init(&owner, &config, 100000U));
}
TEST_F(OneWire, DelayedResetFallingEdgeStillHasItsFullMinimumLowTime) {
    open();
    reset_pre_latency = 30U;
    uint8_t out{};
    EXPECT_EQ(NX_SUCCESS, exchange(&out, 8U));
    ASSERT_FALSE(low_widths.empty());
    EXPECT_GE(low_widths.front(), 480U);
}
TEST_F(OneWire, DelayedZeroFallingEdgeCannotShortenItsPhysicalLowPulse) {
    expected_tx = {0U};
    open();
    data_pre_latency = 10U;
    uint8_t out{};
    EXPECT_EQ(NX_SUCCESS, exchange(&out, 8U));
    ASSERT_GE(low_widths.size(), 9U);
    for (size_t i = 1U; i <= 8U; ++i)
        EXPECT_GE(low_widths[i], 60U);
}
TEST_F(OneWire, DelayedReadFallingEdgeCannotPublishAZeroWidthInitiation) {
    open();
    read_pre_latency = 2U;
    uint8_t out{};
    EXPECT_EQ(NX_ERROR_IO, exchange(&out, 8U));
    ASSERT_GE(low_widths.size(), 10U);
    EXPECT_GE(low_widths[9], 2U);
}
TEST_F(OneWire, ExpiredStopBudgetRetainsTheBindingWithoutPinChanges) {
    open();
    const auto before = writes;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_onewire_stop(&owner, 0U));
    EXPECT_EQ(before, writes);
    EXPECT_EQ(&config, owner.config);
}
TEST_F(OneWire, LateStopReadbackCannotClaimDeadlineSuccess) {
    open();
    uint8_t out{};
    ASSERT_EQ(NX_SUCCESS, exchange(&out, 8U));
    read_latency = 20U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_onewire_stop(&owner, time + 5U));
    EXPECT_EQ(&config, owner.config);
}
TEST_F(OneWire, FinalReleaseReadbackMustStillObserveTheTransferDeadline) {
    open();
    write_hook = [&] {
        if (master_released && rx_bits >= 8U)
            time = 100000U;
    };
    uint8_t out{};
    EXPECT_EQ(NX_ERROR_TIMEOUT, exchange(&out, 8U));
    EXPECT_FALSE(owner.in_call);
}

TEST_F(OneWire, RealDeviceCompositionUsesGpioSlotsCrcAndCooperativeWait) {
    const std::array<uint8_t, 8> rom{0x28U, 1U, 2U, 3U, 4U, 5U, 6U, 0x9eU};
    auto command = [&](uint8_t function) {
        std::vector<uint8_t> bytes{0x55U};
        bytes.insert(bytes.end(), rom.begin(), rom.end());
        bytes.push_back(function);
        return bytes;
    };
    auto scratch = [](uint16_t raw) {
        std::vector<uint8_t> bytes{static_cast<uint8_t>(raw),
                                   static_cast<uint8_t>(raw >> 8U),
                                   0x4bU,
                                   0x46U,
                                   0x1fU,
                                   0xffU,
                                   0x0cU,
                                   0x10U,
                                   0U};
        bytes[8] = nx_onewire_crc8(bytes.data(), 8U);
        return bytes;
    };
    script = {{command(0xb4U), {1U}},
              {command(0xbeU), scratch(0x0550U)},
              {command(0xbeU), scratch(0x0550U)},
              {command(0x44U), {}},
              {command(0xbeU), scratch(0xff90U)}};
    open();
    nx_ds18b20_t device{};
    nx_ds18b20_config_t sensor{};
    sensor.bus = &owner;
    sensor.external_vdd = true;
    std::copy(rom.begin(), rom.end(), sensor.rom);
    ASSERT_EQ(NX_SUCCESS, nx_ds18b20_init(&device, &sensor, 2000000U));
    ASSERT_EQ(NX_SUCCESS, nx_ds18b20_start(&device, 2000000U));
    EXPECT_EQ(9U, device.resolution_bits);
    const auto before = writes;
    int16_t result = 777;
    time = device.ready_at - 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_ds18b20_service(&device, &result));
    EXPECT_EQ(777, result);
    EXPECT_EQ(before, writes);
    time = device.ready_at;
    EXPECT_EQ(NX_SUCCESS, nx_ds18b20_service(&device, &result));
    EXPECT_EQ(-112, result);
    EXPECT_EQ(script.size(), script_index);
    EXPECT_EQ(NX_SUCCESS, nx_ds18b20_stop(&device));
    EXPECT_EQ(NX_SUCCESS, nx_onewire_stop(&owner, time + 10000U));
}

TEST_F(OneWire, FaultedStopKeepsTheWireThroughPendingDelayedPresence) {
    open();
    uint8_t out{};
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_onewire_exchange(&owner, expected_tx.data(),
                                                    1U, &out, 8U, time + 515U));
    ASSERT_NE(0U, reset_release);
    ASSERT_LT(time, reset_release + 20U);
    ASSERT_TRUE(owner.faulted);
    EXPECT_EQ(NX_SUCCESS, nx_onewire_stop(&owner, time + 2000U));
    EXPECT_GE(time, reset_release + 500U);
    EXPECT_EQ(nullptr, owner.config);
}
TEST_F(OneWire, FaultedStopShortBudgetRetainsTheWireBeforePresenceSettles) {
    open();
    uint8_t out{};
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_onewire_exchange(&owner, expected_tx.data(),
                                                    1U, &out, 8U, time + 515U));
    ASSERT_TRUE(owner.faulted);
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_onewire_stop(&owner, time + 100U));
    EXPECT_EQ(&config, owner.config);
    EXPECT_FALSE(owner.in_call);
}
