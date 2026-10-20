/**
 * \file            g474_pwm_shared_factory_test.cpp
 * \brief           Real normal Platform construction of one TIM3 and two faces
 * \author          Nexus Team
 * \version         0.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pwm_shared_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32g474_pwm_shared.h"
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
constexpr uint32_t pads = (1U << 6U) | (1U << 7U);
constexpr uint32_t mode_fields = (3U << 12U) | (3U << 14U);
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* writes;
void observed_write(volatile uint32_t* reg, uint32_t value) {
    if (writes != nullptr) {
        writes->Call(reg, value);
    }
}

class G474SharedPwmFactory : public testing::Test {
  protected:
    GPIO_TypeDef snapshot{};
    nx_stm32g474_pwm_shared_state_t* owner{};

    static const nx_pwm_port_t* Left() {
        return nx_factory_pwm(NX_PWM_ID_LEFT);
    }
    static const nx_pwm_port_t* Right() {
        return nx_factory_pwm(NX_PWM_ID_RIGHT);
    }
    static const nx_stm32g474_pwm_shared_channel_t*
    Reference(const nx_pwm_port_t* face) {
        return face == nullptr
                   ? nullptr
                   : static_cast<const nx_stm32g474_pwm_shared_channel_t*>(
                         face->context);
    }
    static uint32_t PinMode(unsigned pin) {
        return (GPIOA->MODER >> (pin * 2U)) & 3U;
    }
    void SetUp() override {
        nx_g474_shared_reset();
        g_shared_factory_faults = {};
        writes = nullptr;
        /* Pre-existing fields are deliberately distinct from both safe GPIO
         * and AF PWM. Only the selected pads may be changed or restored. */
        GPIOA->MODER = mode_fields | (2U << 8U);
        GPIOA->OTYPER = 1U << 7U;
        GPIOA->OSPEEDR = (2U << 12U) | (1U << 14U);
        GPIOA->PUPDR = (2U << 12U) | (1U << 14U);
        GPIOA->AFR[0] = (1U << 24U) | (3U << 28U) | (4U << 16U);
        GPIOA->ODR = 1U << 4U;
        snapshot = *GPIOA;
        g_shared_factory_faults.snapshot_mode = snapshot.MODER;
        ASSERT_NE(Left(), nullptr);
        ASSERT_NE(Right(), nullptr);
        ASSERT_NE(Reference(Left()), nullptr);
        ASSERT_NE(Reference(Right()), nullptr);
        owner = Reference(Left())->owner;
        ASSERT_NE(owner, nullptr);
    }
    void TearDown() override {
        writes = nullptr;
        g_shared_factory_faults = {};
        g_shared_pwm.write_observer = nullptr;
        g_shared_pwm.refuse_register = nullptr;
        g_shared_pwm.hold_ccer_set = 0U;
        g_shared_pwm.refuse_update = false;
        g_shared_pwm.refuse_ack = false;
        g_shared_pwm.refuse_reset_assert = false;
        g_shared_pwm.refuse_reset_release = false;
        g_g474_model.ignore_write = nullptr;
        const nx_result_t stopped = nx_platform_stop();
        EXPECT_EQ(stopped, NX_SUCCESS);
        if (stopped != NX_SUCCESS ||
            (owner != nullptr && (owner->retained || owner->idle.retained ||
                                  owner->running_mask != 0U)) ||
            g_nx_stm32g474_system.started) {
            /* Never reset the register model over an unclosed static owner. */
            std::abort();
        }
    }
    nx_result_t StartBoth() {
        nx_result_t status = nx_platform_start().primary;
        if (status == NX_SUCCESS) {
            status = nx_pwm_port_start(Left());
        }
        if (status == NX_SUCCESS) {
            status = nx_pwm_port_start(Right());
        }
        return status;
    }
    void ExpectOuterLeases() {
        EXPECT_NE(RCC->APB1ENR1 & RCC_APB1ENR1_TIM3EN, 0U);
        EXPECT_NE(RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN, 0U);
        EXPECT_TRUE(g_nx_stm32g474_system.started);
    }
    void ExpectOriginalPins() {
        EXPECT_EQ(GPIOA->MODER, snapshot.MODER);
        EXPECT_EQ(GPIOA->OTYPER, snapshot.OTYPER);
        EXPECT_EQ(GPIOA->OSPEEDR, snapshot.OSPEEDR);
        EXPECT_EQ(GPIOA->PUPDR, snapshot.PUPDR);
        EXPECT_EQ(GPIOA->AFR[0], snapshot.AFR[0]);
        EXPECT_EQ(GPIOA->ODR, snapshot.ODR);
    }
    void ExpectReleased() {
        EXPECT_FALSE(owner->retained);
        EXPECT_FALSE(owner->idle.retained);
        EXPECT_EQ(owner->running_mask, 0U);
        EXPECT_EQ(RCC->APB1ENR1 & RCC_APB1ENR1_TIM3EN, 0U);
        EXPECT_EQ(RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN, 0U);
        EXPECT_FALSE(g_nx_stm32g474_system.started);
        ExpectOriginalPins();
    }
};

TEST_F(G474SharedPwmFactory, ColdFactoryKeepsTwoOriginalFacesAndOneOwner) {
    EXPECT_EQ(Left(), nx_factory_pwm(NX_PWM_ID_LEFT));
    EXPECT_EQ(Right(), nx_factory_pwm(NX_PWM_ID_RIGHT));
    EXPECT_NE(Left(), Right());
    EXPECT_NE(Left()->context, Right()->context);
    EXPECT_EQ(Left()->ops, &nx_stm32g474_pwm_shared_ops);
    EXPECT_EQ(Right()->ops, Left()->ops);
    EXPECT_EQ(Reference(Right())->owner, owner);
    EXPECT_EQ(Reference(Left())->channel, 1U);
    EXPECT_EQ(Reference(Right())->channel, 2U);
    EXPECT_EQ(g_g474_model.reads, 0U);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_EQ(g_shared_pwm.timer_reads, 0U);
    EXPECT_EQ(g_shared_pwm.timer_writes, 0U);
}

TEST_F(G474SharedPwmFactory, RealNormalColdStartLoadsOneBaseAndBothSafePads) {
    const auto result = nx_platform_start();
    ASSERT_EQ(result.primary, NX_SUCCESS);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(Reference(Right())->owner, owner);
    ASSERT_NE(owner->config, nullptr);
    EXPECT_TRUE(owner->retained);
    EXPECT_TRUE(owner->initialized);
    EXPECT_EQ(owner->config->inactive, 1U << 7U);
    EXPECT_TRUE(g_nx_stm32g474_system.started);
    EXPECT_EQ(g_nx_stm32g474_system.core_hz, 170000000U);
    EXPECT_TRUE(g_g474_model.boost_delayed);
    EXPECT_FALSE(g_g474_model.unsafe_order);
    EXPECT_EQ(RCC->CFGR & RCC_CFGR_SWS, RCC_CFGR_SWS_PLL);
    EXPECT_EQ(TIM3->PSC, 169U);
    EXPECT_EQ(TIM3->ARR, 999U);
    EXPECT_EQ(g_shared_pwm.active_psc, 169U);
    EXPECT_EQ(g_shared_pwm.active_arr, 999U);
    EXPECT_EQ(g_shared_pwm.active_ccr[0], 250U);
    EXPECT_EQ(g_shared_pwm.active_ccr[1], 750U);
    EXPECT_EQ(g_shared_pwm.software_updates, 1U);
    EXPECT_EQ(g_shared_pwm.trigger_edges, 0U);
    EXPECT_EQ(TIM3->CCER, 0U);
    EXPECT_EQ(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(PinMode(6U), 1U);
    EXPECT_EQ(PinMode(7U), 1U);
    EXPECT_EQ(GPIOA->ODR & pads, 1U << 7U);
    EXPECT_EQ(GPIOA->MODER & (3U << 8U), snapshot.MODER & (3U << 8U));
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM3_IRQn]);
    EXPECT_FALSE(g_g474_model.irq_pending[TIM3_IRQn]);
    ExpectOuterLeases();
}

TEST_F(G474SharedPwmFactory, SecondFaceStartsWithoutRestartingTheRunningPeer) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_start(Left()), NX_SUCCESS);
    nx_g474_shared_advance(317U);
    const uint32_t phase = TIM3->CNT;
    const unsigned updates = g_shared_pwm.software_updates;
    const unsigned resets = g_shared_pwm.reset_asserts;
    ASSERT_EQ(nx_pwm_port_start(Right()), NX_SUCCESS);
    EXPECT_EQ(TIM3->CNT, phase);
    EXPECT_EQ(g_shared_pwm.software_updates, updates);
    EXPECT_EQ(g_shared_pwm.reset_asserts, resets);
    EXPECT_EQ(TIM3->CCER, TIM_CCER_CC1E | TIM_CCER_CC2E);
    EXPECT_EQ(owner->running_mask, 3U);
    EXPECT_EQ(PinMode(6U), 2U);
    EXPECT_EQ(PinMode(7U), 2U);
    EXPECT_EQ((GPIOA->AFR[0] >> 24U) & 255U, 0x22U);
}

TEST_F(G474SharedPwmFactory, PublicSetAndOneLaneStopPreserveThePeerBase) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    testing::StrictMock<
        testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        calls;
    EXPECT_CALL(calls, Call(&TIM3->CCR2, 640U)).Times(1);
    writes = &calls;
    g_shared_pwm.write_observer = observed_write;
    ASSERT_EQ(nx_pwm_port_set(Right(), 1000U, 640U), NX_SUCCESS);
    g_shared_pwm.write_observer = nullptr;
    writes = nullptr;
    nx_g474_shared_update();
    nx_g474_shared_advance(215U);
    const uint32_t phase = TIM3->CNT;
    const unsigned updates = g_shared_pwm.software_updates;
    const unsigned resets = g_shared_pwm.reset_asserts;
    ASSERT_EQ(nx_pwm_port_stop(Left()), NX_SUCCESS);
    EXPECT_EQ(TIM3->CNT, phase);
    EXPECT_EQ(TIM3->CCR2, 640U);
    EXPECT_EQ(g_shared_pwm.active_ccr[1], 640U);
    EXPECT_EQ(TIM3->CCER, TIM_CCER_CC2E);
    EXPECT_NE(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(g_shared_pwm.software_updates, updates);
    EXPECT_EQ(g_shared_pwm.reset_asserts, resets);
    EXPECT_EQ(PinMode(6U), 1U);
    EXPECT_EQ(PinMode(7U), 2U);
    EXPECT_EQ(GPIOA->ODR & (1U << 6U), 0U);
    nx_pwm_state_t first{}, second{};
    ASSERT_EQ(nx_pwm_port_state(Left(), &first), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_state(Right(), &second), NX_SUCCESS);
    EXPECT_FALSE(first.running);
    EXPECT_TRUE(second.running);
    EXPECT_EQ(second.duty_ticks, 640U);
    ExpectOuterLeases();
}

TEST_F(G474SharedPwmFactory, PublicLastLaneCenRefusalRetainsThePhysicalOwner) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_stop(Left()), NX_SUCCESS);
    const unsigned resets = g_shared_pwm.reset_asserts;
    g_shared_pwm.refuse_register = &TIM3->CR1;
    EXPECT_EQ(nx_pwm_port_stop(Right()), NX_ERROR_IO);
    EXPECT_NE(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_TRUE(owner->retained);
    EXPECT_EQ(g_shared_pwm.reset_asserts, resets);
    ExpectOuterLeases();
    g_shared_pwm.refuse_register = nullptr;
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ExpectReleased();
}

TEST_F(G474SharedPwmFactory, SecondSafePadFailureHasHonestPartialCleanup) {
    g_shared_factory_faults.refused_mode_fields = 3U << 14U;
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_GT(g_shared_factory_faults.refused_mode_writes, 0U);
    EXPECT_NE(PinMode(7U), 2U);
    EXPECT_EQ(TIM3->CCER, 0U);
    if (result.cleanup == NX_SUCCESS) {
        EXPECT_EQ(result.remaining_effects, 0U);
        ExpectReleased();
    } else {
        EXPECT_EQ(result.cleanup, NX_ERROR_IO);
        EXPECT_NE(result.remaining_effects, 0U);
        EXPECT_TRUE(owner->retained);
        ExpectOuterLeases();
        EXPECT_EQ(nx_platform_start().primary, NX_ERROR_STATE);
    }
    g_shared_factory_faults.refused_mode_fields = 0U;
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ExpectReleased();
}

TEST_F(G474SharedPwmFactory, FailedTimerConstructionRetainsOneCleanupMarker) {
    /* PSC/ARR/CCR are already staged when the fresh UG cannot be observed.
     * Refusing reset release makes full rollback genuinely unproved. */
    g_shared_pwm.refuse_update = true;
    g_shared_pwm.refuse_reset_release = true;
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_TIMEOUT);
    EXPECT_EQ(result.cleanup, NX_ERROR_IO);
    EXPECT_NE(result.remaining_effects, 0U);
    EXPECT_TRUE(owner->retained);
    EXPECT_FALSE(owner->initialized);
    EXPECT_NE(RCC->APB1RSTR1 & RCC_APB1RSTR1_TIM3RST, 0U);
    EXPECT_EQ(TIM3->CCER, 0U);
    ExpectOuterLeases();
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_STATE);
    g_shared_pwm.refuse_update = false;
    g_shared_pwm.refuse_reset_release = false;
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ExpectReleased();
}

TEST_F(G474SharedPwmFactory, RefusedResetReleaseRetainsPinsAndBothClockLeases) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    g_shared_pwm.refuse_reset_release = true;
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    EXPECT_TRUE(owner->retained);
    EXPECT_NE(RCC->APB1RSTR1 & RCC_APB1RSTR1_TIM3RST, 0U);
    EXPECT_NE(GPIOA->MODER & mode_fields, snapshot.MODER & mode_fields);
    ExpectOuterLeases();
    g_shared_pwm.refuse_reset_release = false;
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ExpectReleased();
}

TEST_F(G474SharedPwmFactory, FailedFinalSafeLatchCannotDropOuterLeases) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    /* ODR is a separate real latch from the selected AF output. The fault
     * refuses the explicit inactive-high PA7 latch when close attempts it. */
    GPIOA->ODR &= ~(1U << 7U);
    g_shared_factory_faults.refused_latch_set = 1U << 7U;
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    EXPECT_GT(g_shared_factory_faults.refused_latch_writes, 0U);
    EXPECT_TRUE(owner->retained);
    EXPECT_NE(GPIOA->MODER & mode_fields, snapshot.MODER & mode_fields);
    ExpectOuterLeases();
    g_shared_factory_faults.refused_latch_set = 0U;
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ExpectReleased();
}

TEST_F(G474SharedPwmFactory,
       OuterSnapshotFailureKeepsItsOwnRetryResponsibility) {
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    g_shared_factory_faults.refuse_snapshot_mode = true;
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    EXPECT_GT(g_shared_factory_faults.refused_snapshot_writes, 0U);
    /* The physical owner may finish while the enclosing pin snapshot still
     * retains responsibility; its failure must not be lost as a cold no-op. */
    EXPECT_FALSE(owner->retained);
    ExpectOuterLeases();
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_STATE);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    g_shared_factory_faults.refuse_snapshot_mode = false;
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ExpectReleased();
}

TEST_F(G474SharedPwmFactory, ForeignSharedClockRejectsBeforeSystemEffects) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_EQ(g_shared_pwm.timer_writes, 0U);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_FALSE(owner->retained);
    EXPECT_NE(RCC->APB1ENR1 & RCC_APB1ENR1_TIM3EN, 0U);
    /* The foreign owner, not Platform cleanup, withdraws its test lease. */
    RCC->APB1ENR1 &= ~(uint32_t)RCC_APB1ENR1_TIM3EN;
}

TEST_F(G474SharedPwmFactory, ForeignQuietVectorRejectsBeforeGlobalMutation) {
    g_g474_model.irq_pending[TIM3_IRQn] = true;
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_EQ(g_shared_pwm.timer_writes, 0U);
    EXPECT_FALSE(owner->retained);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_TRUE(g_g474_model.irq_pending[TIM3_IRQn]);
    /* Only the external owner that inserted this pending event removes it. */
    g_g474_model.irq_pending[TIM3_IRQn] = false;
}

TEST_F(G474SharedPwmFactory, JoinedStopAndRestartKeepBothOriginalIdentities) {
    const auto* left = Left();
    const auto* right = Right();
    const auto* left_ref = Reference(left);
    const auto* right_ref = Reference(right);
    ASSERT_EQ(StartBoth(), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_set(Right(), 1000U, 125U), NX_SUCCESS);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ExpectReleased();
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(Left(), left);
    EXPECT_EQ(Right(), right);
    EXPECT_EQ(Reference(Left()), left_ref);
    EXPECT_EQ(Reference(Right()), right_ref);
    EXPECT_EQ(Reference(Left())->owner, owner);
    EXPECT_EQ(Reference(Right())->owner, owner);
    EXPECT_EQ(g_shared_pwm.active_ccr[0], 250U);
    EXPECT_EQ(g_shared_pwm.active_ccr[1], 750U);
    nx_pwm_state_t first{}, second{};
    ASSERT_EQ(nx_pwm_port_state(Left(), &first), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_state(Right(), &second), NX_SUCCESS);
    EXPECT_FALSE(first.running);
    EXPECT_FALSE(second.running);
}
} /* namespace */
