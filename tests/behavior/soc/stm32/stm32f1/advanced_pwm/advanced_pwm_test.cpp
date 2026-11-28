/**
 * \file            advanced_pwm_test.cpp
 * \brief           F103 TIM1/TIM8 complementary output and break contracts.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/advanced_pwm/instance.h"
#include "nexus/arch/arch.h"
#include "system/platform.h"
#include <gtest/gtest.h>

#if defined(TEST_F1_CYCLE)
#include "drivers/pwm_adc/instance.h"
#endif

TIM_TypeDef f1_apwm_tim{};
TIM_TypeDef f1_apwm_peer_tim{};
GPIO_TypeDef f1_apwm_gpioa{};
GPIO_TypeDef f1_apwm_gpiob{};
GPIO_TypeDef f1_apwm_gpioc{};
RCC_TypeDef f1_apwm_rcc{};
AFIO_TypeDef f1_apwm_afio{};

#if defined(TEST_F1_CYCLE)
ADC_TypeDef f1_apwm_adc{};
uintptr_t f1_cycle_ram_begin;
uintptr_t f1_cycle_ram_end;
static bool cycle_active[43], cycle_pending[43], cycle_enabled[43];
static uint32_t cycle_priority[43];
static nx_time_us_t cycle_time;

uint32_t f1_cycle_GetEnableIRQ(IRQn_Type irq) {
    return cycle_enabled[irq];
}
uint32_t f1_cycle_GetPendingIRQ(IRQn_Type irq) {
    return cycle_pending[irq];
}
uint32_t f1_cycle_GetActive(IRQn_Type irq) {
    return cycle_active[irq];
}
uint32_t f1_cycle_GetPriority(IRQn_Type irq) {
    return cycle_priority[irq];
}
void f1_cycle_SetPriority(IRQn_Type irq, uint32_t priority) {
    cycle_priority[irq] = priority;
}
void f1_cycle_EnableIRQ(IRQn_Type irq) {
    cycle_enabled[irq] = true;
}
void f1_cycle_DisableIRQ(IRQn_Type irq) {
    cycle_enabled[irq] = false;
}
void f1_cycle_ClearPendingIRQ(IRQn_Type irq) {
    cycle_pending[irq] = false;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++cycle_time;
}
#endif

constexpr uint32_t pair = TIM_CCER_CC1E | TIM_CCER_CC1NE;
#if defined(STM32F103xE)
#define MODELED_TIMER       TIM8
#define MODELED_MAIN_MODES  GPIOC->CRL
#define MODELED_OTHER_MODES GPIOA->CRL
#define MODELED_OTHER       GPIOA
constexpr unsigned main_shift = 24U;
constexpr unsigned comp_shift = 28U;
constexpr unsigned break_shift = 24U;
constexpr uint32_t break_pin = 1U << 6U;
constexpr uint32_t clock_gate = RCC_APB2ENR_TIM8EN;
constexpr uint32_t timer_reset = RCC_APB2RSTR_TIM8RST;
#else
#define MODELED_TIMER       TIM1
#define MODELED_MAIN_MODES  GPIOA->CRH
#define MODELED_OTHER_MODES GPIOB->CRH
#define MODELED_OTHER       GPIOB
constexpr unsigned main_shift = 0U;
constexpr unsigned comp_shift = 20U;
constexpr unsigned break_shift = 16U;
constexpr uint32_t break_pin = 1U << 12U;
constexpr uint32_t clock_gate = RCC_APB2ENR_TIM1EN;
constexpr uint32_t timer_reset = RCC_APB2RSTR_TIM1RST;
#endif
constexpr uint32_t protection =
    TIM_BDTR_BKE | TIM_BDTR_OSSI | TIM_BDTR_OSSR | 8U;
static bool enabled, pending, active, mask, inject_break, refuse_detach;
static uint32_t irq_priority;
static unsigned writes;

static void hardware_break() {
    MODELED_OTHER->IDR &= ~break_pin;
    if ((MODELED_TIMER->BDTR & TIM_BDTR_BKE) != 0U) {
        MODELED_TIMER->BDTR &= ~static_cast<uint32_t>(TIM_BDTR_MOE);
        MODELED_TIMER->SR |= TIM_SR_BIF;
        pending = enabled && (MODELED_TIMER->DIER & TIM_DIER_BIE) != 0U;
    }
}

extern "C" void f1_apwm_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
#if defined(TEST_F1_CYCLE)
    if (reg == &RCC->APB2RSTR && (value & RCC_APB2RSTR_ADC1RST) != 0U)
        f1_apwm_adc = {};
    if (reg == &ADC1->CR2)
        value &= ~static_cast<uint32_t>(ADC_CR2_CAL | ADC_CR2_RSTCAL);
#endif
    if (reg == &RCC->APB2RSTR && (value & timer_reset) != 0U) {
        f1_apwm_tim = {};
    }
    for (auto* target : {GPIOA, GPIOB, GPIOC}) {
        if (reg == &target->BSRR) {
            target->ODR = (target->ODR | (value & 0xffffU)) & ~(value >> 16U);
            return;
        }
    }
    if (reg == &MODELED_TIMER->EGR) {
        EXPECT_EQ(static_cast<uint32_t>(TIM_EGR_UG), value);
        MODELED_TIMER->CNT = 0U;
        MODELED_TIMER->SR |= TIM_SR_UIF;
        return;
    }
    if (reg == &MODELED_TIMER->SR) {
        MODELED_TIMER->SR &= value;
        if ((MODELED_OTHER->IDR & break_pin) == 0U &&
            (MODELED_TIMER->BDTR & TIM_BDTR_BKE) != 0U) {
            MODELED_TIMER->SR |= TIM_SR_BIF;
        }
        return;
    }
    if (reg == &MODELED_TIMER->CCER && value == 0U && refuse_detach) {
        return;
    }
    *reg = value;
    if (reg == &MODELED_TIMER->BDTR) {
        EXPECT_EQ(0U, value & (TIM_BDTR_AOE | TIM_BDTR_LOCK));
        if ((value & TIM_BDTR_MOE) != 0U) {
            EXPECT_EQ(pair, MODELED_TIMER->CCER & pair);
            EXPECT_EQ(0xaU, (MODELED_MAIN_MODES >> main_shift) & 0xfU);
            EXPECT_EQ(0xaU, (MODELED_OTHER_MODES >> comp_shift) & 0xfU);
            if (inject_break) {
                hardware_break();
                MODELED_OTHER->IDR |= break_pin;
            }
        } else if ((MODELED_OTHER->IDR & break_pin) == 0U) {
            hardware_break();
        }
    }
}

extern "C" void f1_apwm_check_vector(IRQn_Type irq) {
#if defined(STM32F103xE)
    EXPECT_EQ(TIM8_BRK_IRQn, irq);
#else
    EXPECT_EQ(TIM1_BRK_IRQn, irq);
#endif
}

extern "C" bool f1_apwm_irq_enabled(void) {
    return enabled;
}
extern "C" bool f1_apwm_irq_pending(void) {
    return pending;
}
extern "C" bool f1_apwm_irq_active(void) {
    return active;
}
extern "C" uint32_t f1_apwm_irq_priority(void) {
    return irq_priority;
}
extern "C" void f1_apwm_set_priority(uint32_t value) {
    irq_priority = value;
}
extern "C" void f1_apwm_enable_irq(void) {
    enabled = true;
}
extern "C" void f1_apwm_disable_irq(void) {
    enabled = false;
}
extern "C" void f1_apwm_clear_irq(void) {
    pending = false;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return mask;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_isb(void) {
}

extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous{mask ? 1U : 0U};
    mask = true;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t state) {
    mask = state.value != 0U;
}

class F1AdvancedPwm : public testing::Test {
  protected:
    nx_stm32f1_advanced_pwm_t owner{};
    nx_stm32f1_advanced_pwm_config_t config{800U,  8000000U, 8U,   5U,
                                            false, false,    false};
    const nx_advanced_pwm_t port{&nx_stm32f1_advanced_pwm_ops, &owner};

    void SetUp() override {
        f1_apwm_tim = {};
        f1_apwm_gpioa = {};
        f1_apwm_gpiob = {};
        f1_apwm_rcc = {};
        f1_apwm_afio = {};
        f1_apwm_gpioc = {};
        f1_apwm_peer_tim = {};
        GPIOA->CRH = GPIOB->CRH = 0x44444444U;
        MODELED_MAIN_MODES = MODELED_OTHER_MODES = 0x44444444U;
        MODELED_OTHER->IDR = break_pin;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        irq_priority = 2U;
        writes = 0;
        enabled = pending = active = mask = inject_break = refuse_detach =
            false;
    }

    void expect_parked() {
        EXPECT_EQ(0U, MODELED_TIMER->BDTR & TIM_BDTR_MOE);
        EXPECT_EQ(0U, MODELED_TIMER->CCER & pair);
        EXPECT_EQ(0U, MODELED_TIMER->CR1 & TIM_CR1_CEN);
        EXPECT_EQ(2U, (MODELED_MAIN_MODES >> main_shift) & 0xfU);
        EXPECT_EQ(2U, (MODELED_OTHER_MODES >> comp_shift) & 0xfU);
    }
};

TEST_F(F1AdvancedPwm, RejectsUnsafeOptionsAndAClaimedVector) {
    config.active_high = true;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    config.active_high = false;
    config.deadtime_ticks = 0U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    config.deadtime_ticks = 8U;
    pending = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    EXPECT_EQ(0U, writes);
}

TEST_F(F1AdvancedPwm, OpenBreakNeverEnablesAnOutput) {
    MODELED_OTHER->IDR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    EXPECT_EQ(protection, MODELED_TIMER->BDTR);
    EXPECT_EQ(8U, (MODELED_OTHER_MODES >> break_shift) & 0xfU);
    EXPECT_EQ(0U, MODELED_OTHER->ODR & break_pin);
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&port));
    EXPECT_EQ(NX_ERROR_BUSY, nx_advanced_pwm_clear_fault(&port));
    expect_parked();
}

TEST_F(F1AdvancedPwm, UsesPreloadedDutyAndRestoresClockPadsVector) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_set_duty(&port, 400U));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&port));
    EXPECT_EQ(400U, MODELED_TIMER->CCR1);
    EXPECT_EQ(799U, MODELED_TIMER->ARR);
    EXPECT_EQ(0U, MODELED_TIMER->PSC);
    EXPECT_TRUE(enabled);
    EXPECT_EQ(5U, irq_priority);
    nx_advanced_pwm_state_t observed{};
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_state(&port, &observed));
    EXPECT_TRUE(observed.running);
    EXPECT_EQ(8U, observed.deadtime_ticks);
    EXPECT_EQ(8000000U, observed.deadtime_hz);
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_stop(&port));
    expect_parked();
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_close(&owner));
    EXPECT_EQ(0x44444444U, MODELED_MAIN_MODES);
    EXPECT_EQ(0x44444444U, MODELED_OTHER_MODES);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(2U, irq_priority);
}

TEST_F(F1AdvancedPwm, ATransientBreakAtEnableRequiresExplicitRecovery) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    inject_break = true;
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&port));
    expect_parked();
    inject_break = false;
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&port));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_clear_fault(&port));
    expect_parked();
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&port));
    hardware_break();
    nx_stm32f1_advanced_pwm_irq(&owner);
    EXPECT_EQ(0U, MODELED_TIMER->DIER & TIM_DIER_BIE);
    EXPECT_EQ(0U, MODELED_TIMER->BDTR & TIM_BDTR_MOE);
    EXPECT_NE(0U, MODELED_TIMER->SR & TIM_SR_BIF);
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&port));
}

TEST_F(F1AdvancedPwm, ShutdownFailureParksPinsAndRetainsTheExactOwner) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&port));
    refuse_detach = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_advanced_pwm_close(&owner));
    EXPECT_EQ(0U, MODELED_TIMER->BDTR & TIM_BDTR_MOE);
    EXPECT_EQ(2U, (MODELED_MAIN_MODES >> main_shift) & 0xfU);
    EXPECT_EQ(2U, (MODELED_OTHER_MODES >> comp_shift) & 0xfU);
    EXPECT_NE(0U, RCC->APB2ENR & clock_gate);
    refuse_detach = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_close(&owner));
    EXPECT_EQ(0U, RCC->APB2ENR);
}

#if defined(STM32F103xE)
TEST_F(F1AdvancedPwm, Tim8OwnsOnlyItsGateAndUsesNoAfioRemap) {
    RCC->APB2ENR = RCC_APB2ENR_TIM1EN;
    TIM1->CCR1 = 29U;
    AFIO->MAPR = AFIO_MAPR_TIM1_REMAP;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_prepare(&owner, &config));
    EXPECT_EQ(RCC_APB2ENR_TIM1EN | RCC_APB2ENR_TIM8EN,
              RCC->APB2ENR & (RCC_APB2ENR_TIM1EN | RCC_APB2ENR_TIM8EN));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&port));
    EXPECT_EQ(29U, TIM1->CCR1);
    EXPECT_EQ(AFIO_MAPR_TIM1_REMAP, AFIO->MAPR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_advanced_pwm_close(&owner));
    EXPECT_EQ(RCC_APB2ENR_TIM1EN, RCC->APB2ENR);
    EXPECT_EQ(29U, TIM1->CCR1);
}
#endif

#if defined(TEST_F1_CYCLE)
namespace {
class F103Cycle : public testing::Test {
  protected:
    nx_stm32f1_cycle_config_t config{
        {2048U, 8000000U, 4U, 5U, false, false, false}, 512U, 3U};
    nx_stm32f1_cycle_t owner{};
    const nx_pwm_adc_t port{&nx_stm32f1_cycle_ops, &owner};
    nx_pwm_adc_request_t request{};
    uint16_t value = 0xffffU;
    uint32_t duty = 256U;
    nx_pwm_group_frame_t frame{&duty, 1U, 512U};

    void SetUp() override {
        f1_apwm_tim = {};
        f1_apwm_gpioa = {};
        f1_apwm_gpiob = {};
        f1_apwm_rcc = {};
        f1_apwm_afio = {};
        f1_apwm_adc = {};
        for (unsigned i = 0U; i < 43U; ++i) {
            cycle_active[i] = cycle_pending[i] = cycle_enabled[i] = false;
            cycle_priority[i] = 0U;
        }
        mask = active = enabled = pending = inject_break = refuse_detach =
            false;
        f1_cycle_ram_begin = reinterpret_cast<uintptr_t>(this);
        f1_cycle_ram_end = f1_cycle_ram_begin + sizeof(*this);
        cycle_time = 0U;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        GPIOB->IDR = break_pin;
        nx_request_initialize(&request.base);
    }

    void TearDown() override {
        refuse_detach = false;
        EXPECT_EQ(NX_SUCCESS, nx_stm32f1_cycle_close(&owner));
    }

    void submit() {
        ASSERT_EQ(NX_SUCCESS, nx_pwm_adc_prepare(&request, &value, 1U, &frame,
                                                 cycle_time + 1000U));
        ASSERT_EQ(NX_SUCCESS, nx_pwm_adc_submit(&port, &request));
        ASSERT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
        ASSERT_NE(0U, TIM1->CR1 & TIM_CR1_OPM);
        ASSERT_NE(0U, TIM1->BDTR & TIM_BDTR_MOE);
        EXPECT_EQ(512U, TIM1->CCR4);
        EXPECT_EQ(0U, TIM1->CCER & TIM_CCER_CC4E);
        EXPECT_EQ(ADC_CR2_JEXTSEL_0 | ADC_CR2_JEXTTRIG | ADC_CR2_ADON,
                  ADC1->CR2);
        EXPECT_EQ(1U << ADC_JSQR_JSQ4_Pos, ADC1->JSQR);
    }

    void cycle(bool compare, bool converted, uint16_t raw) {
        if (compare)
            TIM1->SR |= TIM_SR_CC4IF;
        if (converted) {
            ADC1->SR = ADC_SR_JSTRT | ADC_SR_JEOC;
            ADC1->JDR1 = raw;
        }
        TIM1->CR1 &= ~static_cast<uint32_t>(TIM_CR1_CEN);
        TIM1->SR |= TIM_SR_UIF;
        nx_stm32f1_cycle_update_irq(&owner);
        EXPECT_EQ(0U, TIM1->BDTR & TIM_BDTR_MOE);
    }

    void settled(nx_result_t expected, size_t count) {
        nx_result_t result;
        size_t samples;
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_result(&request.base, &result, &samples));
        EXPECT_EQ(expected, result);
        EXPECT_EQ(count, samples);
        EXPECT_EQ(0U, TIM1->CCER);
        EXPECT_EQ(0U, TIM1->DIER);
        EXPECT_EQ(0U, ADC1->CR2 & ADC_CR2_JEXTTRIG);
    }
};

TEST_F(F103Cycle, PublishesEachNewInteriorCompareAndConversionAfterDrain) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_cycle_prepare(&owner, &config));
    for (uint16_t raw :
         {static_cast<uint16_t>(13U), static_cast<uint16_t>(42U)}) {
        submit();
        EXPECT_EQ(NX_ERROR_BUSY, nx_pwm_adc_service(&port));
        cycle(true, true, raw);
        ASSERT_EQ(NX_SUCCESS, nx_pwm_adc_service(&port));
        settled(NX_SUCCESS, 1U);
        EXPECT_EQ(raw, value);
    }
}

TEST_F(F103Cycle, StaleAdcFlagAndMissingCompareCannotPublish) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_cycle_prepare(&owner, &config));
    ADC1->SR = ADC_SR_JSTRT | ADC_SR_JEOC;
    submit();
    EXPECT_EQ(0U, ADC1->SR);
    cycle(false, true, 22U);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_adc_service(&port));
    settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(0xffffU, value);
}

TEST_F(F103Cycle, ConversionOverrunAtFinalUpdateIsAFault) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_cycle_prepare(&owner, &config));
    submit();
    cycle(true, false, 0U);
    ADC1->SR = ADC_SR_JSTRT | ADC_SR_JEOC;
    ADC1->JDR1 = 19U;
    ASSERT_EQ(NX_SUCCESS, nx_pwm_adc_service(&port));
    settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(0xffffU, value);
}

TEST_F(F103Cycle, CompleteCycleObservedAtTheDeadlineCannotPublish) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_cycle_prepare(&owner, &config));
    submit();
    cycle(true, true, 99U);
    cycle_time = request.base.deadline;
    ASSERT_EQ(NX_SUCCESS, nx_pwm_adc_service(&port));
    settled(NX_ERROR_TIMEOUT, 0U);
    EXPECT_EQ(0xffffU, value);
}

TEST_F(F103Cycle, FailedOutputWithdrawalQuarantinesExactLoan) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_cycle_prepare(&owner, &config));
    submit();
    cycle(true, true, 8U);
    refuse_detach = true;
    EXPECT_EQ(NX_ERROR_IO, nx_pwm_adc_service(&port));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(0xffffU, value);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_cycle_close(&owner));
    refuse_detach = false;
    EXPECT_EQ(NX_SUCCESS, nx_pwm_adc_service(&port));
    settled(NX_ERROR_IO, 0U);
}

TEST_F(F103Cycle, HardwareBreakNeverPublishesAnInjectedSample) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_cycle_prepare(&owner, &config));
    submit();
    hardware_break();
    nx_stm32f1_cycle_break_irq(&owner);
    EXPECT_EQ(0U, TIM1->BDTR & TIM_BDTR_MOE);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_adc_service(&port));
    settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(0xffffU, value);
}

TEST_F(F103Cycle, OutOfRegionSampleIsRejectedBeforeArming) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_cycle_prepare(&owner, &config));
    uint16_t outside = 9U;
    ASSERT_EQ(NX_SUCCESS, nx_pwm_adc_prepare(&request, &outside, 1U, &frame,
                                             cycle_time + 1000U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_pwm_adc_submit(&port, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(0U, TIM1->BDTR & TIM_BDTR_MOE);
    EXPECT_FALSE(cycle_enabled[TIM1_UP_IRQn]);
    EXPECT_EQ(9U, outside);
}

TEST_F(F103Cycle, ForeignAdcOwnerRemainsUntouched) {
    RCC->APB2ENR |= RCC_APB2ENR_ADC2EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_cycle_prepare(&owner, &config));
    EXPECT_EQ(RCC_APB2ENR_ADC2EN, RCC->APB2ENR);
    EXPECT_FALSE(cycle_enabled[TIM1_UP_IRQn]);
}
} /* namespace */
#endif
