/**
 * \file            g474_timer_factory_test.cpp
 * \brief           Generated capture and encoder identities with real providers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_timer_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" void TIM5_IRQHandler(void);

class G474TimerFactory : public testing::Test {
  protected:
    bool ready{};
#if defined(NX_G474_CAPTURE_FACTORY)
    nx_timer_capture_event_t ring[2]{};
    static const nx_timer_capture_t* port() {
        return nx_factory_timer_capture(NX_TIMER_CAPTURE_ID_CAPTURE);
    }
#else
    static const nx_encoder_t* port() {
        return nx_factory_encoder(NX_ENCODER_ID_POSITION);
    }
#endif
    void SetUp() override {
        nx_g474_model_reset();
        std::memset(g_g474_factory_timers, 0, sizeof(g_g474_factory_timers));
        g_g474_factory_refused = nullptr;
        g_g474_factory_held = 0U;
    }
    void TearDown() override {
        g_g474_factory_refused = nullptr;
        g_g474_model.ignore_write = nullptr;
        g_g474_model.hold_irq_off = false;
        g_g474_model.irq_active[TIM5_IRQn] = false;
        g_g474_model.irq_active[TIM3_IRQn] = false;
#if defined(NX_G474_CAPTURE_FACTORY)
        if (ready) {
            (void)nx_timer_capture_stop(port());
            nx_timer_capture_event_t event{};
            while (nx_timer_capture_read(port(), &event) == NX_SUCCESS) {
            }
        }
#endif
        EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    }
    void start_platform() {
        const auto result = nx_platform_start();
        ASSERT_EQ(result.primary, NX_SUCCESS);
        ready = true;
    }
};

TEST_F(G474TimerFactory, LookupIsPureAndStableBeforePlatformStart) {
    ASSERT_NE(port(), nullptr);
    EXPECT_EQ(port(), port());
    EXPECT_NE(port()->ops, nullptr);
    EXPECT_NE(port()->context, nullptr);
    EXPECT_EQ(g_g474_model.reads, 0U);
    EXPECT_EQ(g_g474_model.writes, 0U);
}

#if defined(NX_G474_CAPTURE_FACTORY)
TEST_F(G474TimerFactory, ColdAssemblySelectsFullWidthCounterAndExactAfRoute) {
    start_platform();
    EXPECT_EQ(TIM5->ARR, UINT32_MAX);
    EXPECT_EQ(TIM5->PSC, 169U);
    EXPECT_EQ(GPIOA->MODER & 3U, 2U);
    EXPECT_EQ(GPIOA->AFR[0] & 15U, 2U);
    EXPECT_EQ(TIM5->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(TIM5->DIER, 0U);
    EXPECT_TRUE(g_g474_model.irq_enabled[TIM5_IRQn]);
    EXPECT_EQ(g_g474_model.priority[TIM5_IRQn], 5U);
}

TEST_F(G474TimerFactory, GeneratedVectorPublishesIntoTheExactBorrowedRing) {
    start_platform();
    ASSERT_EQ(nx_timer_capture_start(port(), ring, 2U), NX_SUCCESS);
    TIM5->CCR1 = 0xFE654321U;
    TIM5->SR |= TIM_SR_CC1IF;
    TIM5_IRQHandler();
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(port(), &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 0xFE654321U);
    EXPECT_EQ(TIM5->SR & TIM_SR_CC1IF, 0U);
    nx_timer_capture_state_t state{};
    ASSERT_EQ(nx_timer_capture_state(port(), &state), NX_SUCCESS);
    EXPECT_EQ(state.counter_bits, 32U);
    EXPECT_EQ(state.tick_hz, 1000000U);
}

TEST_F(G474TimerFactory, QueuedCapturePreventsPinAndClockRelease) {
    start_platform();
    ASSERT_EQ(nx_timer_capture_start(port(), ring, 2U), NX_SUCCESS);
    TIM5->CCR1 = 100U;
    TIM5->SR |= TIM_SR_CC1IF;
    TIM5_IRQHandler();
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_BUSY);
    EXPECT_NE(RCC->APB1ENR1 & RCC_APB1ENR1_TIM5EN, 0U);
    EXPECT_EQ(GPIOA->MODER & 3U, 2U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(port(), &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 100U);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(GPIOA->MODER & 3U, 0U);
    EXPECT_EQ(RCC->APB1ENR1 & RCC_APB1ENR1_TIM5EN, 0U);
}

TEST_F(G474TimerFactory, RefusedCaptureSourceRetainsTheRingAndOuterLease) {
    start_platform();
    ASSERT_EQ(nx_timer_capture_start(port(), ring, 2U), NX_SUCCESS);
    g_g474_factory_refused = &TIM5->DIER;
    g_g474_factory_held = TIM_DIER_CC1IE;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(RCC->APB1ENR1 & RCC_APB1ENR1_TIM5EN, 0U);
    EXPECT_EQ(GPIOA->MODER & 3U, 2U);
    g_g474_factory_refused = nullptr;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}

TEST_F(G474TimerFactory, ActiveCapturePublisherKeepsStaticStateAndPinLeases) {
    start_platform();
    g_g474_model.irq_active[TIM5_IRQn] = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(RCC->APB1ENR1 & RCC_APB1ENR1_TIM5EN, 0U);
    EXPECT_EQ(GPIOA->MODER & 3U, 2U);
    g_g474_model.irq_active[TIM5_IRQn] = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}

TEST_F(G474TimerFactory, FailedCaptureConstructionUnwindsItsPartialSession) {
    g_g474_model.ignore_write = &TIM5->CCMR1;
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(GPIOA->MODER, 0U);
    EXPECT_EQ(RCC->APB1ENR1 & RCC_APB1ENR1_TIM5EN, 0U);
}

TEST_F(G474TimerFactory, ForeignCaptureClockRejectsBeforeGlobalEffects) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM5EN;
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_g474_model.writes, 0U);
}
#else
TEST_F(G474TimerFactory, ColdAssemblySelectsTheActualTwoInputPersonality) {
    start_platform();
    EXPECT_EQ(TIM3->SMCR, 3U);
    EXPECT_EQ(TIM3->ARR, 65535U);
    EXPECT_EQ(TIM3->PSC, 0U);
    EXPECT_EQ((GPIOA->MODER >> 12U) & 15U, 10U);
    EXPECT_EQ((GPIOA->AFR[0] >> 24U) & 255U, 34U);
    EXPECT_EQ(TIM3->DIER, 0U);
    EXPECT_EQ(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM3_IRQn]);
}

TEST_F(G474TimerFactory, EncoderReadAndRestartUseTheRealModuloCounter) {
    start_platform();
    ASSERT_EQ(nx_encoder_start(port()), NX_SUCCESS);
    TIM3->CNT = 65535U;
    TIM3->CR1 |= TIM_CR1_DIR;
    nx_encoder_sample_t sample{};
    ASSERT_EQ(nx_encoder_read(port(), &sample), NX_SUCCESS);
    EXPECT_EQ(sample.count, 65535U);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(GPIOA->MODER, 0U);
    EXPECT_EQ(RCC->APB1ENR1 & RCC_APB1ENR1_TIM3EN, 0U);
    EXPECT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_encoder_start(port()), NX_SUCCESS);
    EXPECT_EQ(TIM3->CNT, 0U);
}

TEST_F(G474TimerFactory, RefusedInputWithdrawalRetainsTheWholeController) {
    start_platform();
    ASSERT_EQ(nx_encoder_start(port()), NX_SUCCESS);
    g_g474_factory_refused = &TIM3->CCER;
    g_g474_factory_held = TIM_CCER_CC1E | TIM_CCER_CC2E;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(RCC->APB1ENR1 & RCC_APB1ENR1_TIM3EN, 0U);
    EXPECT_NE(GPIOA->MODER & (15U << 12U), 0U);
    g_g474_factory_refused = nullptr;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}

TEST_F(G474TimerFactory, ActiveUnusedVectorPreventsColdStorageRelease) {
    start_platform();
    g_g474_model.irq_active[TIM3_IRQn] = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(RCC->APB1ENR1 & RCC_APB1ENR1_TIM3EN, 0U);
    EXPECT_NE(GPIOA->MODER & (15U << 12U), 0U);
    g_g474_model.irq_active[TIM3_IRQn] = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}

TEST_F(G474TimerFactory, FailedEncoderConstructionRetainsAndClosesItsSession) {
    g_g474_model.ignore_write = &TIM3->CCMR1;
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(GPIOA->MODER, 0U);
    EXPECT_EQ(RCC->APB1ENR1 & RCC_APB1ENR1_TIM3EN, 0U);
}

TEST_F(G474TimerFactory, ForeignPhysicalControllerRejectsBeforeAnyMutation) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_g474_model.writes, 0U);
}
#endif
