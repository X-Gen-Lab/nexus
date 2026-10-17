/**
 * \file            g474_pwm_adc_factory_test.cpp
 * \brief           Actual generated coupled owner and retained outer leases
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pwm_adc_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32g474_pwm_adc.h"
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
extern "C" void DMA1_Channel1_IRQHandler(void);
namespace {
testing::MockFunction<void(nx_pa_event_t)>* factory_events;
void observe_factory(nx_pa_event_t event) {
    factory_events->Call(event);
}
class G474PWMADCFactory : public testing::Test {
  protected:
    uint16_t* samples{};
    nx_pwm_adc_request_t request{};
    uint32_t duties[3]{10U, 25U, 75U};
    const nx_pwm_group_frame_t frame{duties, 3U, 50U};
    void SetUp() override {
        nx_pa_reset();
        nx_g474_model_reset();
        COMP1->CSR = 0U;
        g_pa_factory_refuse_comparator = false;
        g_pa_factory_refuse_priority = false;
        factory_events = nullptr;
        void* mapped = mmap(reinterpret_cast<void*>(0x20000000U), 4096U,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_EQ(mapped, reinterpret_cast<void*>(0x20000000U));
        samples = static_cast<uint16_t*>(mapped);
        for (size_t i = 0U; i != 2048U; ++i) {
            samples[i] = 0xDEADU;
        }
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        g_pa.event = nullptr;
        g_pa.poll = nullptr;
        g_pa.after_read = nullptr;
        factory_events = nullptr;
        g_pa_factory_refuse_comparator = false;
        g_pa_factory_refuse_priority = false;
        g_pa.refused = nullptr;
        g_pa.hold_counter_stop = false;
        g_pa.hold_adc_stop = false;
        g_pa.hold_adc_disable = false;
        g_pa.hold_dma_disable = false;
        g_pa.hold_addresses = false;
        g_g474_model.hold_irq_off = false;
        g_g474_model.ignore_write = nullptr;
        g_g474_model.irq_active[DMA1_Channel1_IRQn] = false;
        g_g474_model.irq_active[TIM1_BRK_TIM15_IRQn] = false;
        nx_pa_break(false);
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned i = 0U; i != 256U && result != NX_SUCCESS; ++i) {
            nx_pa_factory_advance(1U);
            result = nx_platform_stop();
        }
        if (result != NX_SUCCESS) {
            std::abort();
        }
        if (samples != nullptr) {
            EXPECT_EQ(munmap(samples, 4096U), 0);
        }
    }
    static const nx_pwm_adc_t* window() {
        return nx_factory_pwm_adc(NX_PWM_ADC_ID_WINDOW);
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(result.primary, NX_SUCCESS);
    }
    void Submit() {
        ASSERT_EQ(
            nx_pwm_adc_prepare(&request, samples, 4U, &frame,
                               nx_deadline_after(nx_time_now_us(), 10000U)),
            NX_SUCCESS);
        ASSERT_EQ(nx_pwm_adc_submit(window(), &request), NX_SUCCESS);
    }
    void Complete() {
        nx_pa_factory_advance(1000U);
        g_g474_model.irq_pending[DMA1_Channel1_IRQn] = false;
        g_g474_model.irq_active[DMA1_Channel1_IRQn] = true;
        DMA1_Channel1_IRQHandler();
        g_g474_model.irq_active[DMA1_Channel1_IRQn] = false;
        for (unsigned i = 0U;
             i != 256U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
             ++i) {
            nx_pa_factory_advance(1U);
            (void)nx_pwm_adc_service(window());
        }
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    }
};
TEST_F(G474PWMADCFactory, LookupIsStableWithoutHardwareOrIndependentFaces) {
    ASSERT_NE(window(), nullptr);
    EXPECT_EQ(window(), window());
    EXPECT_EQ(window()->ops, &nx_stm32g474_pwm_adc_ops);
    EXPECT_EQ(nx_factory_pwm_adc(NX_PWM_ADC_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_pwm_group(NX_PWM_GROUP_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_adc(NX_ADC_ID_COUNT), nullptr);
    EXPECT_EQ(g_g474_model.reads, 0U);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_EQ(g_pa.writes, 0U);
}
TEST_F(G474PWMADCFactory,
       RealColdClockPinsComparatorAndCalibrationPrecedeReady) {
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(GPIOA->MODER & 0x003F030FU, 0x002A030FU);
    EXPECT_EQ(GPIOA->AFR[1] & 0xFFFU, 0x666U);
    EXPECT_EQ(COMP1->CSR, COMP_CSR_EN | COMP_CSR_INMSEL_2 | COMP_CSR_INMSEL_1);
    EXPECT_EQ(ADC12_COMMON->CCR, 10U << ADC_CCR_PRESC_Pos);
    EXPECT_EQ(ADC1->CFGR2, ADC_CFGR2_BULB);
    EXPECT_EQ(ADC1->CR & (ADC_CR_ADEN | ADC_CR_ADCAL), 0U);
    EXPECT_FALSE(g_g474_model.irq_enabled[DMA1_Channel1_IRQn]);
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM1_BRK_TIM15_IRQn]);
    EXPECT_EQ(g_pa.unsupported_reads, 0U);
}
TEST_F(G474PWMADCFactory, GeneratedFaceRunsRealTwoPrimeAndFiniteDmaMechanism) {
    ASSERT_NO_FATAL_FAILURE(Start());
    testing::StrictMock<testing::MockFunction<void(nx_pa_event_t)>> observed;
    factory_events = &observed;
    g_pa.event = observe_factory;
    {
        testing::InSequence order;
        EXPECT_CALL(observed, Call(NX_PA_PRIME_EOS)).Times(2);
        EXPECT_CALL(observed, Call(NX_PA_COUNTER_STARTED)).Times(1);
    }
    ASSERT_NO_FATAL_FAILURE(Submit());
    g_pa.event = nullptr;
    factory_events = nullptr;
    EXPECT_EQ(samples[0], 0xDEADU);
    EXPECT_TRUE(g_g474_model.irq_enabled[DMA1_Channel1_IRQn]);
    EXPECT_TRUE(g_g474_model.irq_enabled[TIM1_BRK_TIM15_IRQn]);
    ASSERT_NO_FATAL_FAILURE(Complete());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 4U);
    EXPECT_EQ(samples[0], g_pa.analog_value);
    EXPECT_EQ(DMA1_Channel1->CMAR, 0U);
    EXPECT_EQ(ADC1->CR & ADC_CR_ADEN, 0U);
    EXPECT_EQ(TIM1->CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(G474PWMADCFactory, ForeignTimerClockRejectsBeforeColdEffects) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_EQ(g_pa.writes, 0U);
}
TEST_F(G474PWMADCFactory, ForeignAdcCommonClockRejectsBeforeColdEffects) {
    RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_EQ(g_pa.writes, 0U);
}
TEST_F(G474PWMADCFactory, EarlyComparatorEnableFailureRollsBackItsOuterLease) {
    g_pa.refused = &COMP1->CSR;
    g_pa.held_bits = COMP_CSR_EN;
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(COMP1->CSR, 0U);
    EXPECT_EQ(GPIOA->MODER, 0U);
    EXPECT_EQ(RCC->APB2ENR & (RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SYSCFGEN), 0U);
    EXPECT_EQ(RCC->AHB2ENR & (RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_ADC12EN), 0U);
    g_pa.refused = nullptr;
    ASSERT_NO_FATAL_FAILURE(Start());
}
TEST_F(G474PWMADCFactory, EarlyPinAlternateFailureClosesTheEnabledComparator) {
    g_g474_model.ignore_write = &GPIOA->AFR[1];
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(COMP1->CSR, 0U);
    EXPECT_EQ(GPIOA->MODER, 0U);
    EXPECT_EQ(GPIOA->AFR[1], 0U);
    EXPECT_EQ(RCC->APB2ENR & (RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SYSCFGEN), 0U);
    g_g474_model.ignore_write = nullptr;
    ASSERT_NO_FATAL_FAILURE(Start());
}
TEST_F(G474PWMADCFactory, RefusedLiveCounterWithdrawalRetainsEveryOuterLease) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    g_pa.hold_counter_stop = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(COMP1->CSR & COMP_CSR_EN, 0U);
    EXPECT_NE(GPIOA->MODER & 0x003F0000U, 0U);
    EXPECT_NE(RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_TIM1EN, 0U);
    g_pa.hold_counter_stop = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(GPIOA->MODER, 0U);
    EXPECT_EQ(COMP1->CSR, 0U);
}
TEST_F(G474PWMADCFactory, ActiveVectorRetainsComparatorPadsAndAdcClock) {
    ASSERT_NO_FATAL_FAILURE(Start());
    g_g474_model.irq_active[DMA1_Channel1_IRQn] = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(COMP1->CSR & COMP_CSR_EN, 0U);
    EXPECT_NE(RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN, 0U);
    g_g474_model.irq_active[DMA1_Channel1_IRQn] = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}
TEST_F(G474PWMADCFactory, EndedWindowRestoresBothOriginalLogicalPriorities) {
    g_g474_model.priority[DMA1_Channel1_IRQn] = 3U;
    g_g474_model.priority[TIM1_BRK_TIM15_IRQn] = 7U;
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Complete());
    EXPECT_EQ(g_g474_model.priority[DMA1_Channel1_IRQn], 5U);
    EXPECT_EQ(g_g474_model.priority[TIM1_BRK_TIM15_IRQn], 5U);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(g_g474_model.priority[DMA1_Channel1_IRQn], 3U);
    EXPECT_EQ(g_g474_model.priority[TIM1_BRK_TIM15_IRQn], 7U);
}
TEST_F(G474PWMADCFactory,
       RefusedPriorityRestorationRetainsTheColdConstruction) {
    g_g474_model.priority[DMA1_Channel1_IRQn] = 3U;
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Complete());
    g_pa_factory_refuse_priority = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(COMP1->CSR & COMP_CSR_EN, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN, 0U);
    g_pa_factory_refuse_priority = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}
TEST_F(G474PWMADCFactory, RefusedComparatorRestorePreservesItsControlGate) {
    ASSERT_NO_FATAL_FAILURE(Start());
    g_pa_factory_refuse_comparator = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(COMP1->CSR & COMP_CSR_EN, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN, 0U);
    g_pa_factory_refuse_comparator = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}
TEST_F(G474PWMADCFactory, RefusedPartialConstructorRetainsRecoveryForRetry) {
    g_pa.refused = &TIM1->BDTR;
    g_pa.held_bits = TIM_BDTR_BKE;
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_NE(result.cleanup, NX_SUCCESS);
    EXPECT_NE(result.remaining_effects, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_TIM1EN, 0U);
    EXPECT_NE(COMP1->CSR & COMP_CSR_EN, 0U);
    g_pa.refused = nullptr;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Start());
}
} /* namespace */
