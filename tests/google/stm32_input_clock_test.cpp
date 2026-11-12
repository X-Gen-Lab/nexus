/**
 * \file            stm32_input_clock_test.cpp
 * \brief           Static input-clock startup with explicit PLL48 ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/core/fault.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
nx_stm32_system_t g_nx_stm32_system{};
uint32_t nx_boot_cpacr;
uint32_t nx_boot_mpu;
uint32_t nx_boot_vtor;
uint32_t nx_system_rng_control;
}
#include <algorithm>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

namespace {
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* hardware;
std::vector<uint32_t> oscillator_writes;
uint32_t mask;
bool hold_pll;
bool hold_hse;
bool irq_enabled;

class InputClock : public testing::Test {
  protected:
    RCC_TypeDef rcc{};
    FLASH_TypeDef flash{};
    PWR_TypeDef power{};
    TIM_TypeDef timer{};
    testing::NiceMock<testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        writes;

    void SetUp() override {
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &rcc;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.power = &power;
        g_nx_stm32_system.timer = &timer;
        rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        rcc.PLLCFGR = 0x24003010U;
        mask = 0U;
        nx_boot_cpacr = 0x00f00000U;
        nx_boot_mpu = 0U;
        nx_boot_vtor = 0x08000000U;
        nx_system_rng_control = 0U;
        hold_pll = hold_hse = irq_enabled = false;
        oscillator_writes.clear();
        hardware = &writes;
        ON_CALL(writes, Call(testing::_, testing::_))
            .WillByDefault([this](volatile uint32_t* reg, uint32_t value) {
                if (reg == &rcc.CR)
                    oscillator_writes.push_back(value);
                if (reg == &timer.SR)
                    *reg &= value;
                else
                    *reg = value;
                if (reg == &timer.EGR && (value & TIM_EGR_UG) != 0U) {
                    timer.SR |= TIM_SR_UIF;
                    timer.CNT = 0U;
                }
            });
    }
    void TearDown() override {
        hold_pll = hold_hse = false;
        mask = 0U;
        EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
        hardware = nullptr;
    }
    nx_stm32_start_result_t Start() {
        return nx_stm32_system_start(&g_nx_stm32_system, 8U);
    }
};
} /* namespace */

TEST_F(InputClock, SourceMuxAndEveryNominalConsumerMatchTheActualRunState) {
    ASSERT_EQ(0, Start().primary);
    EXPECT_EQ(NEXUS_CORE_HZ, g_nx_stm32_system.core_hz);
    EXPECT_EQ(NEXUS_CLOCK_SW, rcc.CFGR & RCC_CFGR_SW);
    EXPECT_EQ(NEXUS_CLOCK_SW << 2U, rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(NEXUS_APB1_TIMER_HZ / 1000000U - 1U, timer.PSC);
    EXPECT_EQ(NEXUS_FLASH_WAIT_STATES, flash.ACR & FLASH_ACR_LATENCY);
    EXPECT_TRUE(nx_stm32_clock_profile_matches(&rcc, &flash));
    timer.CNT = 29U;
    EXPECT_EQ(29U, nx_stm32_system_now(&g_nx_stm32_system));
}

TEST_F(InputClock, UnselectedPllIsNeverEnabledEvenTransiently) {
    ASSERT_EQ(0, Start().primary);
    bool any_enabled = std::any_of(
        oscillator_writes.begin(), oscillator_writes.end(),
        [](uint32_t value) { return (value & RCC_CR_PLLON) != 0U; });
    EXPECT_EQ(NEXUS_INPUT_PLL48 != 0, any_enabled);
    EXPECT_EQ(NEXUS_INPUT_PLL48 ? RCC_CR_PLLON | RCC_CR_PLLRDY : 0U,
              rcc.CR & (RCC_CR_PLLON | RCC_CR_PLLRDY));
    EXPECT_EQ(NEXUS_INPUT_PLL48 ? 48000000U : 0U, NEXUS_PLL48_HZ);
}

TEST_F(InputClock, SharedPllInputFieldsAreStaticEvenWithTheMainPllOff) {
    uint32_t original = rcc.PLLCFGR;
    uint32_t owned_fields = RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLSRC;
#if NEXUS_CLOCK_PLL_ENABLED
    owned_fields |= RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLQ;
#endif
    /* RM0090 7.2.3/7.3.2: unselected outputs and reserved bits stay parked. */
    EXPECT_CALL(writes, Call(testing::Ne(&rcc.PLLCFGR), testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(writes,
                Call(&rcc.PLLCFGR,
                     testing::Truly([original, owned_fields](uint32_t value) {
                         return (value & ~owned_fields) ==
                                (original & ~owned_fields);
                     })))
        .Times(testing::AnyNumber());
    ASSERT_EQ(0, Start().primary);
    EXPECT_EQ(NEXUS_CLOCK_PLLCFGR & owned_fields, rcc.PLLCFGR & owned_fields);
    EXPECT_EQ(original & ~owned_fields, rcc.PLLCFGR & ~owned_fields);
    EXPECT_EQ(NEXUS_CLOCK_PLLM, rcc.PLLCFGR & RCC_PLLCFGR_PLLM);
    EXPECT_EQ(NEXUS_INPUT_HSE != 0,
              (rcc.PLLCFGR & RCC_PLLCFGR_PLLSRC_HSE) != 0U);
}

TEST_F(InputClock, MissingPllReadyMattersOnlyForAnExplicitPllConsumer) {
    hold_pll = true;
    auto result = Start();
    if (NEXUS_INPUT_PLL48) {
        EXPECT_NE(0, result.primary);
        EXPECT_EQ(0, result.cleanup);
        EXPECT_FALSE(g_nx_stm32_system.started);
    } else {
        EXPECT_EQ(0, result.primary);
        EXPECT_TRUE(g_nx_stm32_system.started);
    }
}

TEST_F(InputClock, ClockProofRejectsAnUnexpectedPllOrLostInput) {
    ASSERT_EQ(0, Start().primary);
    if (NEXUS_INPUT_PLL48)
        rcc.CR &= ~(uint32_t)RCC_CR_PLLRDY;
    else
        rcc.CR |= RCC_CR_PLLON | RCC_CR_PLLRDY;
    EXPECT_FALSE(nx_stm32_clock_profile_matches(&rcc, &flash));
    if (NEXUS_INPUT_PLL48)
        rcc.CR |= RCC_CR_PLLRDY;
    else
        rcc.CR &= ~(uint32_t)(RCC_CR_PLLON | RCC_CR_PLLRDY);
    rcc.CR &= ~(uint32_t)(NEXUS_INPUT_HSE ? RCC_CR_HSERDY : RCC_CR_HSIRDY);
    EXPECT_FALSE(nx_stm32_clock_profile_matches(&rcc, &flash));
}

TEST_F(InputClock, WithdrawalConfirmsHsiAndRestoresTheOriginalPllAndPower) {
    uint32_t original_pll = rcc.PLLCFGR;
    ASSERT_EQ(0, Start().primary);
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(16000000U, g_nx_stm32_system.core_hz);
    EXPECT_EQ(0U, rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(0U, rcc.CR & (RCC_CR_PLLON | RCC_CR_HSEON));
    EXPECT_EQ(original_pll, rcc.PLLCFGR);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_FALSE(g_nx_stm32_system.started);
}

/** \brief Advance external ready/source status, not the startup algorithm. */
extern "C" void nx_stm32_system_model_poll(void* raw) {
    auto* system = static_cast<nx_stm32_system_t*>(raw);
    auto* rcc = system->rcc;
    if ((rcc->CR & RCC_CR_HSION) != 0U)
        rcc->CR |= RCC_CR_HSIRDY;
    if ((rcc->CR & RCC_CR_HSEON) != 0U && !hold_hse)
        rcc->CR |= RCC_CR_HSERDY;
    else
        rcc->CR &= ~(uint32_t)RCC_CR_HSERDY;
    if ((rcc->CR & RCC_CR_PLLON) != 0U && !hold_pll)
        rcc->CR |= RCC_CR_PLLRDY;
    else
        rcc->CR &= ~(uint32_t)RCC_CR_PLLRDY;
    rcc->CFGR = (rcc->CFGR & ~(uint32_t)RCC_CFGR_SWS) |
                ((rcc->CFGR & (uint32_t)RCC_CFGR_SW) << 2U);
}
/** \brief Observe or refuse actual external register writes with GoogleMock.
 */
extern "C" void nx_stm32_system_model_write(volatile uint32_t* reg,
                                            uint32_t value) {
    hardware->Call(reg, value);
}
/** \brief The selected timer vector has no foreign owner in this fixture. */
extern "C" bool nx_stm32_system_model_irq_available(int) {
    return !irq_enabled;
}
/** \brief Track actual vector admission and withdrawal at the NVIC boundary.
 */
extern "C" int nx_stm32_system_model_irq_control(int, bool enabled) {
    irq_enabled = enabled;
    return 0;
}
/** \brief Save the incoming modeled configurable-interrupt mask. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t saved = {mask};
    mask = 1U;
    return saved;
}
/** \brief Restore that exact modeled incoming mask. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    mask = saved.value;
}
/** \brief Inspect all masks without changing the modeled CPU context. */
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {mask, 0U, 0U};
}
/** \brief This cold-start program runs in privileged Thread context. */
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
/** \brief No startup operation executes in a modeled exception. */
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
/** \brief Inspect the incoming modeled interrupt mask. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return mask != 0U;
}
/** \brief Synchronous host MMIO needs no physical data pipeline barrier. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief This host fixture has no Cortex-M instruction pipeline. */
extern "C" void nx_arch_isb(void) {
}
/** \brief Intercept unexpected fail-stop without returning into production.
 */
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    throw std::runtime_error("unexpected input-clock terminal fault");
}
