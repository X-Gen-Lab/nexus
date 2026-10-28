/**
 * \file            g474_control_factory_test.cpp
 * \brief           Real selected TIM1 group factory and retained cold leases
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_control_factory_model.h"
#include "nexus/core/time.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
TIM_TypeDef g_g474_control_tim1;
TIM_TypeDef g_g474_control_tim15;
COMP_TypeDef g_g474_control_comp1;
}
namespace {
bool comparator_active;
bool refuse_master_withdrawal;
bool refuse_comparator_withdrawal;
bool bootstrap_trigger;
bool refuse_priority_restore;
bool comparator_enable_observed;
bool output_connection_observed;
uint64_t comparator_enabled_at;
uint64_t output_connected_at;
unsigned unclocked_comparator_reads;
uint32_t active_compare[4];
testing::MockFunction<void(unsigned)>* effect_observer;
constexpr unsigned master_withdrawn = 1U;
constexpr unsigned pins_withdrawn = 2U;
constexpr unsigned comparator_withdrawn = 3U;
constexpr uint32_t output_mask = (3U << 16U) | (3U << 18U) | (3U << 20U);
constexpr uint32_t comparator_mask = (3U << 2U) | (3U << 8U);

void observe(unsigned effect) {
    if (effect_observer != nullptr) {
        effect_observer->Call(effect);
    }
}
bool in_timer(volatile uint32_t* address) {
    const uintptr_t value = reinterpret_cast<uintptr_t>(address);
    const uintptr_t first = reinterpret_cast<uintptr_t>(TIM1);
    return value >= first && value < first + sizeof(*TIM1);
}
} /* namespace */

extern "C" uint32_t nx_g474_control_read(volatile uint32_t* address) {
    if (address == &COMP1->CSR) {
        ++g_g474_model.reads;
        if ((RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) == 0U) {
            ++unclocked_comparator_reads;
            return 0U;
        }
        return (*address & ~static_cast<uint32_t>(COMP_CSR_VALUE)) |
               (comparator_active ? COMP_CSR_VALUE : 0U);
    }
    return nx_g474_model_read(address);
}
extern "C" void nx_g474_control_write(volatile uint32_t* address,
                                      uint32_t value) {
    if (address == g_g474_model.ignore_write) {
        ++g_g474_model.writes;
        return;
    }
    if (address == &COMP1->CSR) {
        ++g_g474_model.writes;
        if (refuse_comparator_withdrawal && (*address & COMP_CSR_EN) != 0U &&
            (value & COMP_CSR_EN) == 0U) {
            return;
        }
        if ((*address & COMP_CSR_EN) != 0U && (value & COMP_CSR_EN) == 0U) {
            observe(comparator_withdrawn);
        }
        if ((*address & COMP_CSR_EN) == 0U && (value & COMP_CSR_EN) != 0U) {
            comparator_enable_observed = true;
            comparator_enabled_at = nx_time_now_us();
        }
        *address = value & ~static_cast<uint32_t>(COMP_CSR_VALUE);
        return;
    }
    if (in_timer(address)) {
        ++g_g474_model.writes;
        if (address == &TIM1->SR) {
            *address &= value;
            return;
        }
        if (address == &TIM1->BDTR) {
            if (refuse_master_withdrawal && (*address & TIM_BDTR_MOE) != 0U &&
                (value & TIM_BDTR_MOE) == 0U) {
                return;
            }
            if ((*address & TIM_BDTR_MOE) != 0U &&
                (value & TIM_BDTR_MOE) == 0U) {
                observe(master_withdrawn);
            }
            if (comparator_active) {
                value &= ~static_cast<uint32_t>(TIM_BDTR_MOE);
            }
        }
        if (address == &TIM1->EGR && (value & TIM_EGR_UG) != 0U) {
            if ((TIM1->CR2 & TIM_CR2_MMS2) == 0U) {
                bootstrap_trigger = true;
            }
            if ((TIM1->CR1 & TIM_CR1_UDIS) == 0U) {
                TIM1->CNT = 0U;
                TIM1->SR |= TIM_SR_UIF;
                active_compare[0] = TIM1->CCR1;
                active_compare[1] = TIM1->CCR2;
                active_compare[2] = TIM1->CCR3;
                active_compare[3] = TIM1->CCR4;
            }
        }
        *address = value;
        return;
    }
    if (address == &GPIOA->MODER && !output_connection_observed &&
        (value & output_mask) != 0U) {
        output_connection_observed = true;
        output_connected_at = nx_time_now_us();
    }
    if (address == &GPIOA->MODER && (*address & output_mask) != 0U &&
        (value & output_mask) == 0U) {
        observe(pins_withdrawn);
    }
    nx_g474_model_write(address, value);
}
extern "C" void nx_g474_control_poll(void* system) {
    nx_g474_model_system_poll(system);
    if ((TIM2->CR1 & TIM_CR1_CEN) != 0U) {
        ++TIM2->CNT;
    }
}
extern "C" void nx_g474_control_priority_write(int irq, uint32_t priority) {
    if (!refuse_priority_restore && irq >= 0 && irq < 102 && priority < 16U) {
        g_g474_model.priority[irq] = static_cast<uint8_t>(priority);
    }
}

class G474ControlFactory : public testing::Test {
  protected:
    void SetUp() override {
        nx_g474_model_reset();
        std::memset(TIM1, 0, sizeof(*TIM1));
        std::memset(TIM15, 0, sizeof(*TIM15));
        std::memset(COMP1, 0, sizeof(*COMP1));
        comparator_active = false;
        refuse_master_withdrawal = false;
        refuse_comparator_withdrawal = false;
        bootstrap_trigger = false;
        refuse_priority_restore = false;
        comparator_enable_observed = false;
        output_connection_observed = false;
        comparator_enabled_at = 0U;
        output_connected_at = 0U;
        unclocked_comparator_reads = 0U;
        std::memset(active_compare, 0, sizeof(active_compare));
        effect_observer = nullptr;
    }
    void TearDown() override {
        effect_observer = nullptr;
        refuse_priority_restore = false;
        g_g474_model.ignore_write = nullptr;
        g_g474_model.hold_irq_off = false;
        g_g474_model.irq_active[TIM1_BRK_TIM15_IRQn] = false;
        g_g474_model.irq_pending[TIM1_BRK_TIM15_IRQn] = false;
        refuse_master_withdrawal = false;
        refuse_comparator_withdrawal = false;
        EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    }
    static const nx_pwm_group_t* wave() {
        return nx_factory_pwm_group(NX_PWM_GROUP_ID_WAVE);
    }
};

TEST_F(G474ControlFactory, LookupReturnsOneStaticIdentityWithoutColdEffects) {
    const auto* first = wave();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, wave());
    EXPECT_NE(first->ops, nullptr);
    EXPECT_NE(first->context, nullptr);
    EXPECT_EQ(g_g474_model.reads, 0U);
    EXPECT_EQ(g_g474_model.writes, 0U);
}

TEST_F(G474ControlFactory, StartupPreparesExactRoutesBeforePublishingReady) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ((GPIOA->AFR[1] >> 0U) & 15U, 6U);
    EXPECT_EQ((GPIOA->AFR[1] >> 4U) & 15U, 6U);
    EXPECT_EQ((GPIOA->AFR[1] >> 8U) & 15U, 6U);
    EXPECT_EQ(GPIOA->MODER & comparator_mask, comparator_mask);
    EXPECT_EQ(GPIOA->PUPDR & comparator_mask, 0U);
    EXPECT_EQ(COMP1->CSR, COMP_CSR_EN | COMP_CSR_INMSEL_2 | COMP_CSR_INMSEL_1);
    EXPECT_TRUE(g_g474_model.irq_enabled[TIM1_BRK_TIM15_IRQn]);
    EXPECT_EQ(TIM1->BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(TIM1->CCER, 0U);
    EXPECT_EQ(TIM1->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_FALSE(bootstrap_trigger);
}

TEST_F(G474ControlFactory,
       ComparatorSettlesBeforeOutputConnectionAndPlatformReadiness) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_TRUE(comparator_enable_observed);
    ASSERT_TRUE(output_connection_observed);
    ASSERT_GE(output_connected_at, comparator_enabled_at);
    const auto ready_at = nx_time_now_us();
    ASSERT_GE(ready_at, comparator_enabled_at);
    EXPECT_GE(output_connected_at - comparator_enabled_at, 6U);
    EXPECT_GE(ready_at - comparator_enabled_at, 6U);
    EXPECT_TRUE(g_g474_model.irq_enabled[TIM1_BRK_TIM15_IRQn]);
}

TEST_F(G474ControlFactory, ExistingGroupFaceStagesAndStartsTheActualTimer) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    const uint32_t duty[3] = {100U, 500U, 900U};
    const nx_pwm_group_frame_t frame = {duty, 3U, 750U};
    ASSERT_EQ(nx_pwm_group_commit(wave(), &frame), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_group_start(wave()), NX_SUCCESS);
    EXPECT_EQ(active_compare[0], 100U);
    EXPECT_EQ(active_compare[1], 500U);
    EXPECT_EQ(active_compare[2], 900U);
    EXPECT_EQ(active_compare[3], 750U);
    EXPECT_NE(TIM1->BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_NE(TIM1->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_FALSE(bootstrap_trigger);
}

TEST_F(G474ControlFactory, WholeTimerForeignGateRejectsBeforeClockMutation) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_TIM1EN, 0U);
}

TEST_F(G474ControlFactory, ForeignComparatorRejectsWithoutAcquiringResources) {
    /* The early CSR observation requires the actual APB control clock. */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    COMP1->CSR = COMP_CSR_EN;
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_EQ(COMP1->CSR, COMP_CSR_EN);
}

TEST_F(G474ControlFactory, SharedTim15VectorOwnerRejectsBeforeEffects) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
    TIM15->DIER = TIM_DIER_UIE;
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_EQ(TIM15->DIER, TIM_DIER_UIE);
}

TEST_F(G474ControlFactory, FailedConstructorClosesRetainedPartialState) {
    g_g474_model.ignore_write = &TIM1->CCMR1;
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(TIM1->CCER, 0U);
    EXPECT_EQ(COMP1->CSR, 0U);
    EXPECT_EQ(GPIOA->MODER, 0U);
    EXPECT_EQ(RCC->APB2ENR & (RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SYSCFGEN), 0U);
}

TEST_F(G474ControlFactory, RefusedMasterWithdrawalRetainsAllOuterLeases) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_pwm_group_start(wave()), NX_SUCCESS);
    refuse_master_withdrawal = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(COMP1->CSR & COMP_CSR_EN, 0U);
    EXPECT_NE(GPIOA->MODER & output_mask, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_TIM1EN, 0U);
    refuse_master_withdrawal = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}

TEST_F(G474ControlFactory, RefusedVectorDisablePreservesComparatorAndPins) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    g_g474_model.hold_irq_off = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(COMP1->CSR & COMP_CSR_EN, 0U);
    EXPECT_NE(GPIOA->MODER & output_mask, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_TIM1EN, 0U);
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}

TEST_F(G474ControlFactory, ActivePublisherKeepsTheColdOwnerForRetry) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    g_g474_model.irq_active[TIM1_BRK_TIM15_IRQn] = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(COMP1->CSR & COMP_CSR_EN, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN, 0U);
    g_g474_model.irq_active[TIM1_BRK_TIM15_IRQn] = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}

TEST_F(G474ControlFactory, RefusedComparatorRestoreRetainsItsControlClock) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    refuse_comparator_withdrawal = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(COMP1->CSR & COMP_CSR_EN, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN, 0U);
    refuse_comparator_withdrawal = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}

TEST_F(G474ControlFactory, OutputSourceWithdrawsBeforeFinalPinRelease) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_pwm_group_start(wave()), NX_SUCCESS);
    testing::StrictMock<testing::MockFunction<void(unsigned)>> effects;
    const testing::Expectation source =
        EXPECT_CALL(effects, Call(master_withdrawn));
    EXPECT_CALL(effects, Call(comparator_withdrawn)).After(source);
    EXPECT_CALL(effects, Call(pins_withdrawn)).After(source);
    effect_observer = &effects;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    effect_observer = nullptr;
    EXPECT_EQ(GPIOA->MODER, 0U);
    EXPECT_EQ(COMP1->CSR, 0U);
    EXPECT_EQ(RCC->APB2ENR & (RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SYSCFGEN), 0U);
}

TEST_F(G474ControlFactory, DisabledComparatorValueIsNotAConfigurationOwner) {
    comparator_active = true;
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    nx_pwm_group_state_t status{};
    ASSERT_EQ(nx_pwm_group_state(wave(), &status), NX_SUCCESS);
    EXPECT_TRUE(status.fault);
    EXPECT_EQ(TIM1->BDTR & TIM_BDTR_MOE, 0U);
}

TEST_F(G474ControlFactory, EndedVectorRestoresItsOriginalLogicalPriority) {
    g_g474_model.priority[TIM1_BRK_TIM15_IRQn] = 3U;
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(g_g474_model.priority[TIM1_BRK_TIM15_IRQn], 5U);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(g_g474_model.priority[TIM1_BRK_TIM15_IRQn], 3U);
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM1_BRK_TIM15_IRQn]);
}

TEST_F(G474ControlFactory, RefusedPriorityRestoreKeepsTheColdLease) {
    g_g474_model.priority[TIM1_BRK_TIM15_IRQn] = 3U;
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    refuse_priority_restore = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(g_g474_model.priority[TIM1_BRK_TIM15_IRQn], 5U);
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM1_BRK_TIM15_IRQn]);
    EXPECT_NE(COMP1->CSR & COMP_CSR_EN, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN, 0U);
    refuse_priority_restore = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(g_g474_model.priority[TIM1_BRK_TIM15_IRQn], 3U);
}

TEST_F(G474ControlFactory, ComparatorStatusIsReadOnlyAfterClockAcquisition) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(unclocked_comparator_reads, 0U);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(unclocked_comparator_reads, 0U);
}

TEST_F(G474ControlFactory, UnclockedForeignComparatorIsRecheckedAndPreserved) {
    const uint32_t foreign = COMP_CSR_EN | COMP_CSR_INMSEL_2;
    COMP1->CSR = foreign;
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(COMP1->CSR, foreign);
    EXPECT_EQ(unclocked_comparator_reads, 0U);
    EXPECT_EQ(RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN, 0U);
    EXPECT_EQ(RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN, 0U);
}
