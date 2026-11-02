/**
 * \file            entropy_test.cpp
 * \brief           Finite entropy collection and official F407 RNG health tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/io/entropy.h"
#include "stm32f407_entropy.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
RNG_TypeDef nx_entropy_model_rng{};
RCC_TypeDef nx_entropy_model_rcc{};
}

namespace {
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

struct Clock {
    nx_time_us_t now = 1;
    nx_time_us_t step = 1;
    size_t reads = 0;
    static nx_time_us_t read(void* context) {
        auto& self = *static_cast<Clock*>(context);
        ++self.reads;
        nx_time_us_t value = self.now;
        self.now += self.step;
        return value;
    }
    nx_clock_t view() {
        return {read, this};
    }
};

class EntropySource {
  public:
    MOCK_METHOD(nx_result_t, read, (uint32_t*));
    static nx_result_t read_callback(void* context, uint32_t* word) {
        return static_cast<EntropySource*>(context)->read(word);
    }
};

const nx_entropy_ops_t mock_ops{EntropySource::read_callback};

TEST(Entropy, WordDispatchHasNoFallbackOrHiddenWait) {
    StrictMock<EntropySource> source;
    const nx_entropy_port_t port{&mock_ops, &source};
    uint32_t word = 99;
    EXPECT_CALL(source, read(_)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_entropy_port_try_read(&port, &word), NX_ERROR_BUSY);
    EXPECT_EQ(word, 99U);
}

TEST(Entropy, FillUsesCompleteWordsAndDiscardsUnusedFinalBytes) {
    StrictMock<EntropySource> source;
    const nx_entropy_port_t port{&mock_ops, &source};
    Clock clock;
    const auto time = clock.view();
    std::array<uint8_t, 7> output{};
    testing::InSequence order;
    EXPECT_CALL(source, read(_))
        .WillOnce(DoAll(SetArgPointee<0>(0x04030201U), Return(NX_SUCCESS)));
    EXPECT_CALL(source, read(_))
        .WillOnce(DoAll(SetArgPointee<0>(0x08070605U), Return(NX_SUCCESS)));
    EXPECT_EQ(nx_entropy_fill(&port, output.data(), output.size(), &time, 20),
              NX_SUCCESS);
    EXPECT_EQ(output, (std::array<uint8_t, 7>{1, 2, 3, 4, 5, 6, 7}));
}

TEST(Entropy, BusyIsPolledOnlyWithinTheExplicitDeadline) {
    StrictMock<EntropySource> source;
    const nx_entropy_port_t port{&mock_ops, &source};
    Clock clock;
    const auto time = clock.view();
    std::array<uint8_t, 5> output{9, 9, 9, 9, 9};
    EXPECT_CALL(source, read(_)).Times(3).WillRepeatedly(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_entropy_fill(&port, output.data(), output.size(), &time, 4),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(output, (std::array<uint8_t, 5>{}));
}

TEST(Entropy, PartialSuccessFollowedByHealthFailureErasesEntireOutput) {
    StrictMock<EntropySource> source;
    const nx_entropy_port_t port{&mock_ops, &source};
    Clock clock;
    const auto time = clock.view();
    std::array<uint8_t, 9> output{};
    output.fill(0xa5);
    testing::InSequence order;
    EXPECT_CALL(source, read(_))
        .WillOnce(DoAll(SetArgPointee<0>(0x12345678U), Return(NX_SUCCESS)));
    EXPECT_CALL(source, read(_)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_entropy_fill(&port, output.data(), output.size(), &time, 20),
              NX_ERROR_IO);
    EXPECT_EQ(output, (std::array<uint8_t, 9>{}));
}

TEST(Entropy, ExpiredDeadlineNeverConsumesReadyEntropy) {
    StrictMock<EntropySource> source;
    const nx_entropy_port_t port{&mock_ops, &source};
    Clock clock;
    const auto time = clock.view();
    std::array<uint8_t, 4> output{1, 1, 1, 1};
    EXPECT_EQ(nx_entropy_fill(&port, output.data(), output.size(), &time, 1),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(output, (std::array<uint8_t, 4>{}));
}

TEST(Entropy, ExpiryDuringReadRejectsAndErasesTheReturnedWord) {
    StrictMock<EntropySource> source;
    const nx_entropy_port_t port{&mock_ops, &source};
    Clock clock;
    const auto time = clock.view();
    std::array<uint8_t, 4> output{1, 1, 1, 1};
    EXPECT_CALL(source, read(_))
        .WillOnce(DoAll(SetArgPointee<0>(0x12345678U), Return(NX_SUCCESS)));
    EXPECT_EQ(nx_entropy_fill(&port, output.data(), output.size(), &time, 2),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(output, (std::array<uint8_t, 4>{}));
}

TEST(Entropy, ZeroLengthHasNoClockOrProviderAccess) {
    StrictMock<EntropySource> source;
    const nx_entropy_port_t port{&mock_ops, &source};
    Clock clock;
    const auto time = clock.view();
    EXPECT_EQ(nx_entropy_fill(&port, nullptr, 0, &time, 1), NX_SUCCESS);
    EXPECT_EQ(clock.reads, 0U);
}

TEST(Entropy, InvalidAndAliasedSpansRejectBeforeSourceOrClockAccess) {
    StrictMock<EntropySource> source;
    nx_entropy_port_t port{&mock_ops, &source};
    Clock clock;
    auto time = clock.view();
    std::array<uint8_t, 8> output{};
    output.fill(0xa5);
    EXPECT_EQ(nx_entropy_fill(&port, output.data(), SIZE_MAX, &time, 20),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_entropy_fill(&port, nullptr, 1, &time, 20), NX_ERROR_INVALID);
    EXPECT_EQ(nx_entropy_fill(&port, output.data(), output.size(), &time,
                              NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_entropy_fill(&port, &port, sizeof(port), &time, 20),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_entropy_fill(&port, &time, sizeof(time), &time, 20),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_entropy_fill(&port, &clock, 1, &time, 20), NX_ERROR_INVALID);
    EXPECT_EQ(nx_entropy_fill(&port, &source, 1, &time, 20), NX_ERROR_INVALID);
    EXPECT_EQ(clock.reads, 0U);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(),
                            [](uint8_t byte) { return byte == 0xa5; }));
}

TEST(Entropy, MissingProviderAndClockNeverBecomeDeterministicEntropy) {
    uint32_t output = 99;
    Clock clock;
    auto time = clock.view();
    const nx_entropy_ops_t absent{};
    const nx_entropy_port_t port{&absent, &clock};
    EXPECT_EQ(nx_entropy_port_try_read(nullptr, &output), NX_ERROR_INVALID);
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_entropy_fill(&port, &output, sizeof(output), &time, 20),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(output, 99U);
    EXPECT_EQ(clock.reads, 0U);
}

std::deque<uint32_t> pending_words;
uint32_t after_read_error = 0;
bool after_read_clock_loss = false;
bool block_gate_enable = false;
bool block_rng_disable = false;
size_t data_reads = 0;

class F407Entropy : public testing::Test {
  protected:
    nx_stm32f407_entropy_state_t state{};
    const nx_stm32f407_entropy_config_t config{8000000, 168000000};
    const nx_entropy_port_t port{&nx_stm32f407_entropy_ops, &state};

    void SetUp() override {
        std::memset(&nx_entropy_model_rng, 0, sizeof(nx_entropy_model_rng));
        std::memset(&nx_entropy_model_rcc, 0, sizeof(nx_entropy_model_rcc));
        nx_entropy_model_rcc.CR = RCC_CR_HSERDY | RCC_CR_PLLRDY;
        nx_entropy_model_rcc.PLLCFGR = 8U | (336U << RCC_PLLCFGR_PLLN_Pos) |
                                       RCC_PLLCFGR_PLLSRC |
                                       (7U << RCC_PLLCFGR_PLLQ_Pos);
        nx_entropy_model_rcc.CFGR = RCC_CFGR_SWS_PLL;
        pending_words.clear();
        after_read_error = 0;
        after_read_clock_loss = block_gate_enable = block_rng_disable = false;
        data_reads = 0;
    }

    void words(std::initializer_list<uint32_t> input) {
        pending_words = input;
        if (!pending_words.empty()) {
            nx_entropy_model_rng.DR = pending_words.front();
            nx_entropy_model_rng.SR |= RNG_SR_DRDY;
        }
    }

    void start() {
        ASSERT_EQ(nx_stm32f407_entropy_start(&state, &config), NX_SUCCESS);
    }
};

TEST_F(F407Entropy, StartAndStopTouchOnlyOwnedRngGateAndPeripheral) {
    nx_entropy_model_rcc.AHB2ENR = RCC_AHB2ENR_OTGFSEN;
    const auto pll = nx_entropy_model_rcc.PLLCFGR;
    const auto cfgr = nx_entropy_model_rcc.CFGR;
    start();
    EXPECT_TRUE(state.running);
    EXPECT_TRUE(state.gate_owned);
    EXPECT_NE(nx_entropy_model_rng.CR & RNG_CR_RNGEN, 0U);
    EXPECT_EQ(nx_entropy_model_rng.CR & RNG_CR_IE, 0U);
    EXPECT_EQ(nx_stm32f407_entropy_stop(&state), NX_SUCCESS);
    EXPECT_EQ(nx_entropy_model_rcc.AHB2ENR, RCC_AHB2ENR_OTGFSEN);
    EXPECT_EQ(nx_entropy_model_rcc.PLLCFGR, pll);
    EXPECT_EQ(nx_entropy_model_rcc.CFGR, cfgr);
    EXPECT_FALSE(state.running);
    EXPECT_EQ(nx_stm32f407_entropy_stop(&state), NX_SUCCESS);
}

TEST_F(F407Entropy, InitialWordIsDiscardedAndDistinctSubsequentWordIsCopied) {
    start();
    words({0x11223344U, 0x55667788U});
    uint32_t output = 99;
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_BUSY);
    EXPECT_EQ(output, 99U);
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_SUCCESS);
    EXPECT_EQ(output, 0x55667788U);
    EXPECT_EQ(data_reads, 2U);
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_BUSY);
    EXPECT_EQ(data_reads, 2U);
}

TEST_F(F407Entropy, ConsecutiveDuplicateLatchesFaultUntilFullRestart) {
    start();
    words({7, 7, 8});
    uint32_t output = 99;
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_BUSY);
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_IO);
    EXPECT_TRUE(state.faulted);
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_IO);
    EXPECT_EQ(data_reads, 2U);
    EXPECT_EQ(output, 99U);
    ASSERT_EQ(nx_stm32f407_entropy_stop(&state), NX_SUCCESS);
    start();
    words({10, 11});
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_BUSY);
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_SUCCESS);
    EXPECT_EQ(output, 11U);
}

TEST_F(F407Entropy, EveryCurrentOrStickyHealthErrorRejectsReadyData) {
    const std::array<uint32_t, 4> flags{RNG_SR_CECS, RNG_SR_CEIS, RNG_SR_SECS,
                                        RNG_SR_SEIS};
    for (uint32_t flag : flags) {
        SCOPED_TRACE(flag);
        start();
        words({1, 2});
        nx_entropy_model_rng.SR |= flag;
        uint32_t output = 99;
        EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_IO);
        EXPECT_EQ(output, 99U);
        EXPECT_TRUE(state.faulted);
        nx_entropy_model_rng.SR = 0;
        EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_IO);
        EXPECT_EQ(nx_stm32f407_entropy_stop(&state), NX_SUCCESS);
    }
    EXPECT_EQ(data_reads, 0U);
}

TEST_F(F407Entropy, SeedFailureAppearingAfterReadDiscardsThatWord) {
    start();
    words({1, 2});
    uint32_t output = 99;
    ASSERT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_BUSY);
    after_read_error = RNG_SR_SEIS;
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_IO);
    EXPECT_EQ(output, 99U);
    EXPECT_TRUE(state.faulted);
}

TEST_F(F407Entropy, ClockLossAfterReadRejectsSampleAndNeverChangesPll) {
    start();
    words({1, 2});
    uint32_t output = 99;
    ASSERT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_BUSY);
    const auto pll = nx_entropy_model_rcc.PLLCFGR;
    after_read_clock_loss = true;
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_IO);
    EXPECT_EQ(output, 99U);
    EXPECT_EQ(nx_entropy_model_rcc.PLLCFGR, pll);
}

TEST_F(F407Entropy, ExistingRngOrInterruptIsNotStolen) {
    nx_entropy_model_rcc.AHB2ENR = RCC_AHB2ENR_RNGEN;
    nx_entropy_model_rng.CR = RNG_CR_RNGEN;
    EXPECT_EQ(nx_stm32f407_entropy_start(&state, &config), NX_ERROR_BUSY);
    EXPECT_FALSE(state.gate_owned);
    nx_entropy_model_rng.CR = RNG_CR_IE;
    EXPECT_EQ(nx_stm32f407_entropy_start(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(nx_entropy_model_rng.CR, RNG_CR_IE);
}

TEST_F(F407Entropy, ExistingGateRemainsEnabledAfterStop) {
    nx_entropy_model_rcc.AHB2ENR = RCC_AHB2ENR_RNGEN;
    start();
    EXPECT_FALSE(state.gate_owned);
    EXPECT_EQ(nx_stm32f407_entropy_stop(&state), NX_SUCCESS);
    EXPECT_NE(nx_entropy_model_rcc.AHB2ENR & RCC_AHB2ENR_RNGEN, 0U);
}

TEST_F(F407Entropy, ClockProfileMismatchRejectsBeforeAcquiringGate) {
    auto wrong = config;
    wrong.hclk_hz = 84000000;
    EXPECT_EQ(nx_stm32f407_entropy_start(&state, &wrong), NX_ERROR_UNSUPPORTED);
    nx_entropy_model_rcc.PLLCFGR ^= 1U << RCC_PLLCFGR_PLLQ_Pos;
    EXPECT_EQ(nx_stm32f407_entropy_start(&state, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_entropy_model_rcc.AHB2ENR, 0U);
    EXPECT_EQ(nx_entropy_model_rng.CR, 0U);
    nx_entropy_model_rcc.CR = 0;
    EXPECT_EQ(nx_stm32f407_entropy_start(&state, &config), NX_ERROR_STATE);
}

TEST_F(F407Entropy, ValidTwoMegahertzPllInputUsesOfficialMultiplicationRange) {
    nx_entropy_model_rcc.PLLCFGR = 4U | (168U << RCC_PLLCFGR_PLLN_Pos) |
                                   RCC_PLLCFGR_PLLSRC |
                                   (7U << RCC_PLLCFGR_PLLQ_Pos);
    EXPECT_EQ(nx_stm32f407_entropy_start(&state, &config), NX_SUCCESS);
    EXPECT_EQ(nx_stm32f407_entropy_stop(&state), NX_SUCCESS);
}

TEST_F(F407Entropy, FailedDisableRetainsOwnershipAndClosesAdmission) {
    start();
    block_rng_disable = true;
    EXPECT_EQ(nx_stm32f407_entropy_stop(&state), NX_ERROR_IO);
    EXPECT_TRUE(state.rng_owned);
    EXPECT_TRUE(state.gate_owned);
    EXPECT_FALSE(state.running);
    uint32_t output = 99;
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_STATE);
    EXPECT_EQ(nx_stm32f407_entropy_start(&state, &config), NX_ERROR_BUSY);
    block_rng_disable = false;
    EXPECT_EQ(nx_stm32f407_entropy_stop(&state), NX_SUCCESS);
    EXPECT_FALSE(state.rng_owned);
    EXPECT_FALSE(state.gate_owned);
}

TEST_F(F407Entropy, GateEnableFailureCanBeExplicitlyWithdrawn) {
    block_gate_enable = true;
    EXPECT_EQ(nx_stm32f407_entropy_start(&state, &config), NX_ERROR_IO);
    EXPECT_TRUE(state.gate_owned);
    EXPECT_FALSE(state.running);
    EXPECT_EQ(nx_entropy_model_rng.CR, 0U);
    EXPECT_EQ(nx_stm32f407_entropy_stop(&state), NX_SUCCESS);
    EXPECT_FALSE(state.gate_owned);
}

TEST_F(F407Entropy, InvalidOutputsCannotOverwriteProviderOrHardware) {
    start();
    words({1, 2});
    EXPECT_EQ(nx_stm32f407_entropy_ops.try_read(&state, &state.previous_word),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f407_entropy_ops.try_read(
                  &state, const_cast<uint32_t*>(&nx_entropy_model_rng.CR)),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f407_entropy_ops.try_read(
                  &state, const_cast<uint32_t*>(&nx_entropy_model_rcc.AHB2ENR)),
              NX_ERROR_INVALID);
    EXPECT_EQ(data_reads, 0U);
}

TEST_F(F407Entropy, FiniteFillPrimesHardwareAndProducesOnlyAdmittedWords) {
    start();
    words({1, 0x04030201U, 0x08070605U});
    Clock clock;
    const auto time = clock.view();
    std::array<uint8_t, 6> output{};
    EXPECT_EQ(nx_entropy_fill(&port, output.data(), output.size(), &time, 20),
              NX_SUCCESS);
    EXPECT_EQ(output, (std::array<uint8_t, 6>{1, 2, 3, 4, 5, 6}));
    EXPECT_EQ(data_reads, 3U);
}

class F407EntropyInstance : public F407Entropy {
  protected:
    void TearDown() override {
        block_rng_disable = false;
        nx_entropy_model_rcc.AHB2ENR |= RCC_AHB2ENR_RNGEN;
        EXPECT_EQ(nx_stm32f407_entropy_instance_stop(), NX_SUCCESS);
    }
};

TEST_F(F407EntropyInstance, GetterIsPureStableAndExposesOnlyTheDirectFace) {
    const auto gates = nx_entropy_model_rcc.AHB2ENR;
    const auto* face = nx_stm32f407_entropy_instance();
    ASSERT_NE(face, nullptr);
    EXPECT_EQ(nx_stm32f407_entropy_instance(), face);
    EXPECT_EQ(face->ops, &nx_stm32f407_entropy_ops);
    EXPECT_NE(face->context, nullptr);
    EXPECT_EQ(nx_entropy_model_rcc.AHB2ENR, gates);
    EXPECT_EQ(nx_entropy_model_rng.CR, 0U);
    uint32_t output = 99;
    EXPECT_EQ(nx_entropy_port_try_read(face, &output), NX_ERROR_STATE);
    EXPECT_EQ(output, 99U);
    EXPECT_EQ(data_reads, 0U);
}

TEST_F(F407EntropyInstance, ExplicitStartRejectsDuplicateAndPreservesFace) {
    const auto* face = nx_stm32f407_entropy_instance();
    ASSERT_EQ(nx_stm32f407_entropy_instance_start(&config), NX_SUCCESS);
    EXPECT_EQ(nx_stm32f407_entropy_instance_start(&config), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32f407_entropy_instance(), face);
    words({1, 2});
    uint32_t output = 99;
    EXPECT_EQ(nx_entropy_port_try_read(face, &output), NX_ERROR_BUSY);
    EXPECT_EQ(nx_entropy_port_try_read(face, &output), NX_SUCCESS);
    EXPECT_EQ(output, 2U);
    EXPECT_EQ(nx_stm32f407_entropy_instance_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_entropy_port_try_read(face, &output), NX_ERROR_STATE);
    EXPECT_EQ(nx_stm32f407_entropy_instance(), face);
    EXPECT_EQ(config.hse_hz, 8000000U);
    EXPECT_EQ(config.hclk_hz, 168000000U);
}

TEST_F(F407EntropyInstance, FailedStopRetainsTheUniqueOwnerUntilRetry) {
    nx_entropy_model_rcc.CR = 0;
    EXPECT_EQ(nx_stm32f407_entropy_instance_start(&config), NX_ERROR_STATE);
    EXPECT_EQ(nx_entropy_model_rcc.AHB2ENR, 0U);
    nx_entropy_model_rcc.CR = RCC_CR_HSERDY | RCC_CR_PLLRDY;
    ASSERT_EQ(nx_stm32f407_entropy_instance_start(&config), NX_SUCCESS);
    block_rng_disable = true;
    EXPECT_EQ(nx_stm32f407_entropy_instance_stop(), NX_ERROR_IO);
    EXPECT_EQ(nx_stm32f407_entropy_instance_start(&config), NX_ERROR_BUSY);
    uint32_t output = 99;
    EXPECT_EQ(
        nx_entropy_port_try_read(nx_stm32f407_entropy_instance(), &output),
        NX_ERROR_STATE);
    block_rng_disable = false;
    EXPECT_EQ(nx_stm32f407_entropy_instance_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_stm32f407_entropy_instance_start(&config), NX_SUCCESS);
}
} /* namespace */

/** \brief Consume only RNG_DR; ordinary register reads retain their semantics.
 */
extern "C" uint32_t nx_entropy_model_read(const volatile uint32_t* address) {
    uint32_t value = *address;
    if (address == &nx_entropy_model_rng.DR) {
        ++data_reads;
        if (!pending_words.empty())
            pending_words.pop_front();
        if (pending_words.empty()) {
            nx_entropy_model_rng.SR &= ~static_cast<uint32_t>(RNG_SR_DRDY);
        } else {
            nx_entropy_model_rng.DR = pending_words.front();
        }
        nx_entropy_model_rng.SR |= after_read_error;
        if (after_read_clock_loss)
            nx_entropy_model_rcc.CR &= ~static_cast<uint32_t>(RCC_CR_PLLRDY);
    }
    return value;
}

/** \brief Simulate only W0C health flags and selected stuck-write failures. */
extern "C" void nx_entropy_model_write(volatile uint32_t* address,
                                       uint32_t value) {
    if (address == &nx_entropy_model_rng.SR) {
        nx_entropy_model_rng.SR &=
            value | ~static_cast<uint32_t>(RNG_SR_CEIS | RNG_SR_SEIS);
    } else if (address == &nx_entropy_model_rcc.AHB2ENR && block_gate_enable &&
               (value & RCC_AHB2ENR_RNGEN) != 0U) {
        *address = value & ~static_cast<uint32_t>(RCC_AHB2ENR_RNGEN);
    } else if (address == &nx_entropy_model_rng.CR && block_rng_disable &&
               (value & RNG_CR_RNGEN) == 0U) {
        *address = value | RNG_CR_RNGEN;
    } else {
        *address = value;
    }
}
