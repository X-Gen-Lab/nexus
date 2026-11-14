/**
 * \file            stm32_encoder_test.cpp
 * \brief           Raw hardware quadrature modes and retained cold ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f4_encoder.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
bool privileged;
volatile uint32_t* held;
uint32_t held_mask;
uint32_t held_value;
class Stm32Encoder : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    alignas(nx_encoder_sample_t) nx_stm32_encoder_state_t state{};
    alignas(nx_encoder_sample_t) nx_stm32_encoder_config_t config{
        TIM3,
        NX_ENCODER_MODE_X4,
        3U,
        7U,
        false,
        true,
        {{GPIOA, 0U, 6U}, {GPIOA, 0U, 7U}}};
    nx_encoder_t face{&nx_stm32_encoder_ops, &state};
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        privileged = true;
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        held = nullptr;
        held_mask = held_value = 0U;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
        RCC->APB1ENR = RCC_APB1ENR_TIM7EN;
        RCC->CFGR = RCC_CFGR_PPRE1_DIV4;
    }
    void TearDown() override {
        held = nullptr;
        privileged = true;
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_f4_model_nvic = {};
        if (mapping != MAP_FAILED) {
            if (state.cold_effects != 0U || state.initialized) {
                EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
            }
            EXPECT_EQ(0, munmap(mapping, 0x80000U));
        }
    }
};
} /* namespace */

extern "C" bool nx_encoder_model_privileged(void) {
    return privileged;
}
extern "C" bool nx_encoder_model_isr(void) {
    return g_f4_model_isr;
}
extern "C" bool nx_encoder_model_masked(void) {
    return g_f4_model_mask != 0U;
}
extern "C" void nx_encoder_model_dsb(void) {
    if (held != nullptr)
        *held = (*held & ~held_mask) | (held_value & held_mask);
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 9000U;
}

TEST_F(Stm32Encoder, AlternateNonAdjacentInputsUseOnlyTheirConfiguredPads) {
    config.registers = TIM1;
    config.pins[0] = {GPIOE, 4U, 9U};
    config.pins[1] = {GPIOE, 4U, 11U};
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOEEN;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&state, &config));
    for (unsigned pin : {9U, 11U}) {
        EXPECT_EQ(2U, (GPIOE->MODER >> (pin * 2U)) & 3U);
        EXPECT_EQ(1U, (GPIOE->AFR[pin / 8U] >> ((pin % 8U) * 4U)) & 15U);
    }
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, (GPIOE->MODER >> 20U) & 3U);
    ASSERT_EQ(NX_SUCCESS, nx_encoder_start(&face));
    TIM1->CNT = 123U;
    nx_encoder_sample_t sample{};
    ASSERT_EQ(NX_SUCCESS, nx_encoder_read(&face, &sample));
    EXPECT_EQ(123U, sample.count);
}

TEST_F(Stm32Encoder, InputsOnDifferentPortsHaveIndependentClockAndPinChecks) {
    config.pins[0] = {GPIOB, 1U, 4U};
    config.pins[1] = {GPIOC, 2U, 7U};
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_encoder_prepare(&state, &config));
    EXPECT_EQ(0U, state.cold_effects);
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&state, &config));
    EXPECT_EQ(2U, (GPIOB->MODER >> 8U) & 3U);
    EXPECT_EQ(2U, (GPIOC->MODER >> 14U) & 3U);
    EXPECT_EQ(0U, GPIOA->MODER);
    auto* result = reinterpret_cast<nx_encoder_sample_t*>(GPIOC);
    EXPECT_EQ(NX_ERROR_INVALID, nx_encoder_read(&face, result));
}

TEST_F(Stm32Encoder, InvalidOrDuplicatePadMetadataRejectsBeforeClockEffects) {
    const auto original = config;
    for (unsigned fault = 0U; fault < 4U; ++fault) {
        config = original;
        if (fault == 0U)
            config.pins[0].port_index = 1U;
        if (fault == 1U)
            config.pins[0].pin = 16U;
        if (fault == 2U)
            config.pins[0].registers = nullptr;
        if (fault == 3U)
            config.pins[1] = config.pins[0];
        EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_encoder_prepare(&state, &config));
        EXPECT_EQ(RCC_APB1ENR_TIM7EN, RCC->APB1ENR);
        EXPECT_EQ(nullptr, state.config);
        EXPECT_EQ(0U, state.cold_effects);
    }
    config = original;
}

TEST_F(Stm32Encoder, ThreeWidthsAndCountingModesKeepRawPositionAcrossStop) {
    const struct {
        TIM_TypeDef* timer;
        GPIO_TypeDef* gpio;
        uint32_t clock;
        uint8_t first_pin;
    } timers[]{{TIM3, GPIOA, RCC_APB1ENR_TIM3EN, 6U},
               {TIM4, GPIOB, RCC_APB1ENR_TIM4EN, 6U},
               {TIM5, GPIOA, RCC_APB1ENR_TIM5EN, 0U}};
    for (const auto& selected : timers) {
        for (const auto mode :
             {NX_ENCODER_MODE_X2_A, NX_ENCODER_MODE_X2_B, NX_ENCODER_MODE_X4}) {
            config.registers = selected.timer;
            const auto bank = static_cast<uint8_t>(
                (reinterpret_cast<uintptr_t>(selected.gpio) - GPIOA_BASE) /
                0x400U);
            config.pins[0] = {selected.gpio, bank, selected.first_pin};
            config.pins[1] = {selected.gpio, bank,
                              static_cast<uint8_t>(selected.first_pin + 1U)};
            config.mode = mode;
            config.invert_a = mode == NX_ENCODER_MODE_X2_A;
            ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&state, &config));
            EXPECT_EQ(RCC_APB1ENR_TIM7EN | selected.clock, RCC->APB1ENR);
            EXPECT_EQ(0U, selected.timer->DIER);
            EXPECT_EQ(0U, selected.timer->CR1 & TIM_CR1_CEN);
            EXPECT_EQ(static_cast<uint32_t>(mode),
                      selected.timer->SMCR & TIM_SMCR_SMS);
            EXPECT_EQ((3U << 4U) | (7U << 12U) | 0x101U, selected.timer->CCMR1);
            EXPECT_EQ(config.invert_a ? TIM_CCER_CC1P : 0U,
                      selected.timer->CCER & (TIM_CCER_CC1P | TIM_CCER_CC1NP));
            EXPECT_EQ(TIM_CCER_CC2P,
                      selected.timer->CCER & (TIM_CCER_CC2P | TIM_CCER_CC2NP));
            for (unsigned pin = selected.first_pin;
                 pin < selected.first_pin + 2U; ++pin) {
                EXPECT_EQ(2U, (selected.gpio->MODER >> (pin * 2U)) & 3U);
                EXPECT_EQ(2U,
                          (selected.gpio->AFR[pin / 8U] >> ((pin % 8U) * 4U)) &
                              15U);
            }
            ASSERT_EQ(NX_SUCCESS, nx_encoder_start(&face));
            selected.timer->CNT = 0x1234ffffU;
            ASSERT_EQ(NX_SUCCESS, nx_encoder_start(&face));
            nx_encoder_sample_t sample{};
            ASSERT_EQ(NX_SUCCESS, nx_encoder_read(&face, &sample));
            EXPECT_EQ(selected.timer == TIM5 ? 0x1234ffffU : 0xffffU,
                      sample.count);
            EXPECT_EQ(9000U, sample.observed_at);
            nx_encoder_state_t info{};
            ASSERT_EQ(NX_SUCCESS, nx_encoder_state(&face, &info));
            EXPECT_EQ(selected.timer == TIM5 ? 32U : 16U, info.counter_bits);
            EXPECT_EQ(84000000U, info.filter_clock_hz);
            EXPECT_EQ(mode, info.mode);
            EXPECT_EQ(1U, info.filter_a.clock_divisor);
            EXPECT_EQ(8U, info.filter_a.consecutive_samples);
            EXPECT_EQ(4U, info.filter_b.clock_divisor);
            EXPECT_EQ(8U, info.filter_b.consecutive_samples);
            EXPECT_TRUE(info.running);
            ASSERT_EQ(NX_SUCCESS, nx_encoder_stop(&face));
            EXPECT_EQ(0U, selected.timer->CR1 & TIM_CR1_CEN);
            EXPECT_EQ(0U, selected.timer->CCER & 0x1111U);
            ASSERT_EQ(NX_SUCCESS, nx_encoder_read(&face, &sample));
            EXPECT_EQ(selected.timer == TIM5 ? 0x1234ffffU : 0xffffU,
                      sample.count);
            ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
            EXPECT_EQ(0U, state.cold_effects);
            EXPECT_EQ(RCC_APB1ENR_TIM7EN, RCC->APB1ENR);
        }
    }
}

TEST_F(Stm32Encoder, FilterFactsDescribeSamplingInsteadOfRegisterCodes) {
    const uint8_t divisors[] = {0U, 1U, 1U,  1U,  2U,  2U,  4U,  4U,
                                8U, 8U, 16U, 16U, 16U, 32U, 32U, 32U};
    const uint8_t samples[] = {0U, 2U, 4U, 8U, 6U, 8U, 6U, 8U,
                               6U, 8U, 5U, 6U, 8U, 5U, 6U, 8U};
    for (uint8_t code = 0U; code < 16U; ++code) {
        SCOPED_TRACE(code);
        config.filter_a = code;
        config.filter_b = 15U - code;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&state, &config));
        nx_encoder_state_t info{};
        ASSERT_EQ(NX_SUCCESS, nx_encoder_state(&face, &info));
        EXPECT_EQ(divisors[code], info.filter_a.clock_divisor);
        EXPECT_EQ(samples[code], info.filter_a.consecutive_samples);
        EXPECT_EQ(divisors[15U - code], info.filter_b.clock_divisor);
        EXPECT_EQ(samples[15U - code], info.filter_b.consecutive_samples);
        EXPECT_FALSE(info.running);
        ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
    }
}

TEST_F(Stm32Encoder, AdvancedTimersKeepRawPositionAndUseTheirApb2FilterClock) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    for (TIM_TypeDef* timer : {TIM1, TIM8}) {
        for (nx_encoder_mode_t mode :
             {NX_ENCODER_MODE_X2_A, NX_ENCODER_MODE_X2_B, NX_ENCODER_MODE_X4}) {
            SCOPED_TRACE(timer);
            SCOPED_TRACE(mode);
            config = {timer, mode, 3U, 7U, false, true, {}};
            config.pins[0] = timer == TIM1
                                 ? nx_stm32_encoder_pin_t{GPIOA, 0U, 8U}
                                 : nx_stm32_encoder_pin_t{GPIOC, 2U, 6U};
            config.pins[1] = timer == TIM1
                                 ? nx_stm32_encoder_pin_t{GPIOA, 0U, 9U}
                                 : nx_stm32_encoder_pin_t{GPIOC, 2U, 7U};
            const nx_result_t prepared =
                nx_stm32_encoder_prepare(&state, &config);
            EXPECT_EQ(NX_SUCCESS, prepared);
            if (prepared != NX_SUCCESS)
                continue;
            EXPECT_EQ(RCC_APB1ENR_TIM7EN, RCC->APB1ENR);
            EXPECT_EQ(timer == TIM1 ? RCC_APB2ENR_TIM1EN : RCC_APB2ENR_TIM8EN,
                      RCC->APB2ENR);
            ASSERT_EQ(NX_SUCCESS, nx_encoder_start(&face));
            timer->CNT = 0x1234abcdU;
            nx_encoder_sample_t sample{};
            ASSERT_EQ(NX_SUCCESS, nx_encoder_read(&face, &sample));
            EXPECT_EQ(0xabcdU, sample.count);
            nx_encoder_state_t info{};
            ASSERT_EQ(NX_SUCCESS, nx_encoder_state(&face, &info));
            EXPECT_EQ(168000000U, info.filter_clock_hz);
            EXPECT_EQ(16U, info.counter_bits);
            EXPECT_EQ(mode, info.mode);
            ASSERT_EQ(NX_SUCCESS, nx_encoder_stop(&face));
            EXPECT_EQ(0x1234abcdU, timer->CNT);
            EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
            EXPECT_EQ(0U, RCC->APB2ENR);
        }
    }
}

TEST_F(Stm32Encoder,
       AdvancedForeignBreakStateAndFailedGateReleaseRetainOwners) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    for (TIM_TypeDef* timer : {TIM1, TIM8}) {
        SCOPED_TRACE(timer);
        config = {timer, NX_ENCODER_MODE_X4, 0U, 0U, false, false, {}};
        config.pins[0] = timer == TIM1 ? nx_stm32_encoder_pin_t{GPIOA, 0U, 8U}
                                       : nx_stm32_encoder_pin_t{GPIOC, 2U, 6U};
        config.pins[1] = timer == TIM1 ? nx_stm32_encoder_pin_t{GPIOA, 0U, 9U}
                                       : nx_stm32_encoder_pin_t{GPIOC, 2U, 7U};
        const uint32_t gate =
            timer == TIM1 ? RCC_APB2ENR_TIM1EN : RCC_APB2ENR_TIM8EN;
        const unsigned peer = timer == TIM1 ? 24U : 43U;
        NVIC->ISER[peer / 32U] |= 1U << (peer % 32U);
        timer->BDTR = TIM_BDTR_MOE | TIM_BDTR_LOCK_0;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_encoder_prepare(&state, &config));
        EXPECT_FALSE(state.initialized);
        EXPECT_NE(0U, state.cold_effects);
        EXPECT_EQ(TIM_BDTR_MOE | TIM_BDTR_LOCK_0, timer->BDTR);
        ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
        timer->BDTR = 0U;
        timer->CCER = TIM_CCER_CC1NE;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_encoder_prepare(&state, &config));
        EXPECT_EQ(TIM_CCER_CC1NE, timer->CCER);
        ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
        timer->CCER = 0U;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&state, &config));
        held = &RCC->APB2ENR;
        held_mask = held_value = gate;
        EXPECT_EQ(NX_ERROR_IO, nx_stm32_encoder_close(&state));
        EXPECT_NE(0U, state.cold_effects);
        EXPECT_EQ(&config, state.config);
        held = nullptr;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
        EXPECT_NE(0U, NVIC->ISER[peer / 32U] & (1U << (peer % 32U)));
    }
}

TEST_F(Stm32Encoder, RefusedSourceWithdrawalKeepsSessionAndClockForRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&state, &config));
    ASSERT_EQ(NX_SUCCESS, nx_encoder_start(&face));
    held = &TIM3->CR1;
    held_mask = held_value = TIM_CR1_CEN;
    EXPECT_EQ(NX_ERROR_IO, nx_encoder_stop(&face));
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_encoder_close(&state));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_TIM3EN);
    EXPECT_NE(0U, state.cold_effects);
    held = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
    EXPECT_EQ(RCC_APB1ENR_TIM7EN, RCC->APB1ENR);
}

TEST_F(Stm32Encoder, LiveCountersAndShutdownKeepOtherInstancesIndependent) {
    nx_stm32_encoder_state_t peers[2]{};
    const nx_stm32_encoder_config_t configs[]{
        {TIM4,
         NX_ENCODER_MODE_X2_A,
         0U,
         0U,
         false,
         false,
         {{GPIOB, 1U, 6U}, {GPIOB, 1U, 7U}}},
        {TIM5,
         NX_ENCODER_MODE_X2_B,
         1U,
         2U,
         true,
         false,
         {{GPIOA, 0U, 0U}, {GPIOA, 0U, 1U}}}};
    const nx_encoder_t faces[]{face,
                               {&nx_stm32_encoder_ops, &peers[0]},
                               {&nx_stm32_encoder_ops, &peers[1]}};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&state, &config));
    for (unsigned i = 0U; i < 2U; ++i) {
        EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&peers[i], &configs[i]));
    }
    for (const auto& port : faces)
        EXPECT_EQ(NX_SUCCESS, nx_encoder_start(&port));
    TIM3->CNT = 41U;
    TIM4->CNT = 42U;
    TIM5->CNT = 43U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
    for (unsigned i = 0U; i < 2U; ++i) {
        nx_encoder_sample_t sample{};
        EXPECT_EQ(NX_SUCCESS, nx_encoder_read(&faces[i + 1U], &sample));
        EXPECT_EQ(42U + i, sample.count);
        EXPECT_NE(0U, configs[i].registers->CR1 & TIM_CR1_CEN);
        EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&peers[i]));
    }
    EXPECT_EQ(RCC_APB1ENR_TIM7EN, RCC->APB1ENR);
}

TEST_F(Stm32Encoder, RefusedPinPreparationRetainsPartialControllerCleanup) {
    held = &GPIOA->MODER;
    held_mask = 3U << 12U;
    held_value = 0U;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_encoder_prepare(&state, &config));
    EXPECT_FALSE(state.initialized);
    EXPECT_NE(0U, state.cold_effects);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_TIM3EN);
    nx_encoder_sample_t sample{123U, 456U};
    EXPECT_EQ(NX_ERROR_STATE, nx_encoder_read(&face, &sample));
    EXPECT_EQ(123U, sample.observed_at);
    EXPECT_EQ(456U, sample.count);
    held = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
    EXPECT_EQ(RCC_APB1ENR_TIM7EN, RCC->APB1ENR);
}

TEST_F(Stm32Encoder, FailedClockAcquisitionAndReleaseRetainExactEffects) {
    held = &RCC->APB1ENR;
    held_mask = RCC_APB1ENR_TIM3EN;
    held_value = 0U;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_encoder_prepare(&state, &config));
    EXPECT_NE(0U, state.cold_effects);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
    held = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&state, &config));
    held = &RCC->APB1ENR;
    held_value = RCC_APB1ENR_TIM3EN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_encoder_close(&state));
    EXPECT_NE(0U, state.cold_effects);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_encoder_prepare(&state, &config));
    held = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
    EXPECT_EQ(RCC_APB1ENR_TIM7EN, RCC->APB1ENR);
}

TEST_F(Stm32Encoder, ForeignClockControllerAndVectorAreNeverAdopted) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_encoder_prepare(&state, &config));
    EXPECT_EQ(0U, state.cold_effects);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_TIM3EN);
    RCC->APB1ENR &= ~static_cast<uint32_t>(RCC_APB1ENR_TIM3EN);
    const unsigned irq = static_cast<unsigned>(TIM3_IRQn);
    NVIC->IABR[irq / 32U] = 1U << (irq % 32U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_encoder_prepare(&state, &config));
    EXPECT_EQ(0U, state.cold_effects);
    NVIC->IABR[irq / 32U] = 0U;
    TIM3->CR1 = TIM_CR1_CEN;
    TIM3->DIER = TIM_DIER_UDE;
    TIM3->CCER = TIM_CCER_CC1E;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_encoder_prepare(&state, &config));
    EXPECT_NE(0U, state.cold_effects);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_encoder_close(&state));
    EXPECT_EQ(TIM_CR1_CEN, TIM3->CR1);
    EXPECT_EQ(TIM_DIER_UDE, TIM3->DIER);
    EXPECT_EQ(TIM_CCER_CC1E, TIM3->CCER);
}

TEST_F(Stm32Encoder, UnsupportedInputsRejectBeforeAnyHardwareWrite) {
    for (unsigned invalid = 0U; invalid < 4U; ++invalid) {
        config = {TIM3,
                  NX_ENCODER_MODE_X4,
                  0U,
                  0U,
                  false,
                  false,
                  {{GPIOA, 0U, 6U}, {GPIOA, 0U, 7U}}};
        if (invalid == 0U)
            config.registers = TIM9;
        if (invalid == 1U)
            config.mode = NX_ENCODER_MODE_UNKNOWN;
        if (invalid == 2U)
            config.filter_a = 16U;
        if (invalid == 3U)
            config.filter_b = 16U;
        const auto* bytes = static_cast<const uint8_t*>(mapping);
        const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
        EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_encoder_prepare(&state, &config));
        EXPECT_EQ(0, std::memcmp(bytes, before.data(), before.size()));
        EXPECT_EQ(0U, state.cold_effects);
    }
}

TEST_F(Stm32Encoder, ContextAndPrivateAliasesKeepRejectedOutputsUnchanged) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_encoder_prepare(&state, &config));
    nx_encoder_sample_t sample{123U, 456U};
    for (unsigned context = 0U; context < 3U; ++context) {
        privileged = context != 0U;
        g_f4_model_isr = context == 1U;
        g_f4_model_mask = context == 2U ? 1U : 0U;
        EXPECT_EQ(NX_ERROR_CONTEXT, nx_encoder_start(&face));
        EXPECT_EQ(NX_ERROR_CONTEXT, nx_encoder_read(&face, &sample));
        EXPECT_EQ(123U, sample.observed_at);
        EXPECT_EQ(456U, sample.count);
    }
    privileged = true;
    g_f4_model_isr = false;
    g_f4_model_mask = 0U;
    for (void* pointer :
         {static_cast<void*>(&state), static_cast<void*>(&config),
          static_cast<void*>(TIM3), static_cast<void*>(RCC)}) {
        EXPECT_EQ(
            NX_ERROR_INVALID,
            nx_encoder_read(&face, static_cast<nx_encoder_sample_t*>(pointer)));
    }
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(TIM3, config.registers);
}
