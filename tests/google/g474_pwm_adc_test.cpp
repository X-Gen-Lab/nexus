/**
 * \file            g474_pwm_adc_test.cpp
 * \brief           Coupled provider boundaries against external G474 events
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pwm_adc_model.h"
#include "stm32g474_pwm_adc.h"
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
namespace {
testing::MockFunction<void(nx_pa_event_t)>* events;
bool late_dma_error_raised;
void observe(nx_pa_event_t event) {
    events->Call(event);
}
class G474PWMADC : public testing::Test {
  protected:
    nx_stm32g474_pwm_adc_state_t hardware{};
    const nx_pwm_adc_t port{&nx_stm32g474_pwm_adc_ops, &hardware};
    uint16_t* samples{};
    nx_dma_memory_region_t domain{};
    nx_stm32g474_pwm_adc_config_t config{};
    nx_pwm_adc_request_t request{};
    uint32_t duties[3]{10U, 25U, 75U};
    nx_pwm_group_frame_t frame{duties, 3U, 50U};
    void SetUp() override {
        nx_pa_reset();
        events = nullptr;
        void* mapped = mmap(reinterpret_cast<void*>(0x20000000U), 4096U,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_EQ(mapped, reinterpret_cast<void*>(0x20000000U));
        samples = static_cast<uint16_t*>(mapped);
        for (size_t i = 0U; i != 2048U; ++i) {
            samples[i] = 0xDEADU;
        }
        domain = {reinterpret_cast<uintptr_t>(mapped), 4096U,
                  NX_DMA_MEMORY_WRITE};
        config = {&domain, 1U,    1000000U, 100U, 50U,
                  170U,    3300U, 100000U,  1U,   true};
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        g_pa.event = nullptr;
        g_pa.poll = nullptr;
        g_pa.after_read = nullptr;
        events = nullptr;
        g_pa.refused = nullptr;
        g_pa.hold_conversion = false;
        g_pa.hold_dma_beat = false;
        g_pa.hold_adc_stop = false;
        g_pa.hold_adc_disable = false;
        g_pa.hold_dma_disable = false;
        g_pa.hold_counter_stop = false;
        g_pa.hold_addresses = false;
        g_pa.privileged = true;
        g_pa.in_isr = false;
        g_pa.mask = 0U;
        g_pa.irq_active[DMA1_Channel1_IRQn] = false;
        g_pa.irq_active[TIM1_BRK_TIM15_IRQn] = false;
        nx_pa_break(false);
        if (hardware.active != nullptr) {
            (void)nx_pwm_adc_cancel(&port, &request);
            for (unsigned i = 0U; i != 256U && hardware.active != nullptr;
                 ++i) {
                nx_pa_advance_cycles(170U);
                (void)nx_pwm_adc_service(&port);
            }
        }
        if (hardware.retained &&
            nx_stm32g474_pwm_adc_close(&hardware) != NX_SUCCESS) {
            std::abort();
        }
        if (hardware.active != nullptr) {
            std::abort();
        }
        if (samples != nullptr) {
            EXPECT_EQ(munmap(samples, 4096U), 0);
        }
    }
    void Initialize() {
        ASSERT_EQ(nx_stm32g474_pwm_adc_initialize(&hardware, &config),
                  NX_SUCCESS);
    }
    void Submit(size_t count = 4U) {
        ASSERT_EQ(
            nx_pwm_adc_prepare(&request, samples, count, &frame,
                               nx_deadline_after(nx_time_now_us(), 10000U)),
            NX_SUCCESS);
        ASSERT_EQ(nx_pwm_adc_submit(&port, &request), NX_SUCCESS);
    }
    void DMAIRQ() {
        g_pa.in_isr = true;
        g_pa.irq_active[DMA1_Channel1_IRQn] = true;
        g_pa.irq_pending[DMA1_Channel1_IRQn] = false;
        nx_stm32g474_pwm_adc_dma_irq(&hardware);
        g_pa.irq_active[DMA1_Channel1_IRQn] = false;
        g_pa.in_isr = false;
    }
    void ServiceUntilSettled() {
        for (unsigned i = 0U;
             i != 256U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
             ++i) {
            nx_pa_advance_cycles(170U);
            (void)nx_pwm_adc_service(&port);
        }
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(hardware.active, nullptr);
    }
};
TEST_F(G474PWMADC, PrimeTwiceBeforeCounterStartAndNeverDmaEitherPrime) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    testing::StrictMock<testing::MockFunction<void(nx_pa_event_t)>> observed;
    events = &observed;
    g_pa.event = observe;
    {
        testing::InSequence order;
        EXPECT_CALL(observed, Call(NX_PA_PRIME_EOS)).Times(2);
        EXPECT_CALL(observed, Call(NX_PA_COUNTER_STARTED)).Times(1);
    }
    ASSERT_NO_FATAL_FAILURE(Submit());
    g_pa.event = nullptr;
    events = nullptr;
    EXPECT_EQ(g_pa.primes, 2U);
    EXPECT_EQ(g_pa.edges, 0U);
    EXPECT_EQ(g_pa.beats, 0U);
    EXPECT_EQ(samples[0], 0xDEADU);
    nx_pa_advance_cycles(49U * 170U);
    EXPECT_EQ(samples[0], 0xDEADU);
    nx_pa_advance_cycles(16U * 170U);
    EXPECT_EQ(g_pa.edges, 1U);
    EXPECT_EQ(g_pa.beats, 1U);
    EXPECT_EQ(samples[0], g_pa.analog_value);
}
TEST_F(G474PWMADC, ExactRegularRouteAndDmaAddressExistBeforeFirstTrigger) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    EXPECT_EQ((ADC1->CFGR & ADC_CFGR_EXTSEL) >> ADC_CFGR_EXTSEL_Pos, 10U);
    EXPECT_EQ(ADC1->CFGR & ADC_CFGR_EXTEN, ADC_CFGR_EXTEN_0);
    EXPECT_NE(ADC1->CFGR & ADC_CFGR_DMAEN, 0U);
    EXPECT_EQ(ADC1->CFGR & ADC_CFGR_DMACFG, 0U);
    EXPECT_NE(ADC1->CR & ADC_CR_ADSTART, 0U);
    EXPECT_EQ(DMAMUX1_Channel0->CCR, 5U);
    EXPECT_EQ(DMA1_Channel1->CNDTR, 4U);
    EXPECT_EQ(DMA1_Channel1->CMAR, (uint32_t)(uintptr_t)samples);
    EXPECT_EQ(DMA1_Channel1->CPAR, (uint32_t)(uintptr_t)&ADC1->DR);
    EXPECT_EQ(DMA1_Channel1->CCR & DMA_CCR_CIRC, 0U);
    EXPECT_EQ(TIM1->PSC, 169U);
    EXPECT_EQ(TIM1->ARR, 99U);
    EXPECT_EQ(TIM1->CCR4, 50U);
    EXPECT_EQ(g_pa.edges, 0U);
}
TEST_F(G474PWMADC, LimitedDmaDoesNotOverwriteBufferWhenTerminalIrqIsLate) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    nx_pa_advance_cycles(1000U * 170U);
    EXPECT_EQ(g_pa.beats, 4U);
    EXPECT_EQ(samples[4], 0xDEADU);
    EXPECT_EQ(DMA1_Channel1->CNDTR, 0U);
    EXPECT_TRUE(g_pa.limited_complete);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    /* TC is not an automatic proof of MOE/CEN withdrawal. */
    ASSERT_NO_FATAL_FAILURE(DMAIRQ());
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 4U);
    EXPECT_EQ(DMA1_Channel1->CCR & DMA_CCR_EN, 0U);
    EXPECT_EQ(DMA1_Channel1->CMAR, 0U);
    EXPECT_EQ(DMA1_Channel1->CPAR, 0U);
    EXPECT_EQ(ADC1->CR &
                  (ADC_CR_ADEN | ADC_CR_ADSTART | ADC_CR_ADDIS | ADC_CR_ADSTP),
              0U);
    EXPECT_EQ(TIM1->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(TIM1->BDTR & TIM_BDTR_MOE, 0U);
    const unsigned prior_edges = g_pa.edges;
    nx_pa_advance_cycles(1000U * 170U);
    EXPECT_EQ(g_pa.edges, prior_edges);
    EXPECT_EQ(g_pa.beats, 4U);
    EXPECT_FALSE(g_pa.global_clear_while_active);
}
TEST_F(G474PWMADC, EveryLaterWindowReenablesAndPrimesAfterOneShotCompletion) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    nx_pa_advance_cycles(1000U * 170U);
    DMAIRQ();
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    const unsigned enables = g_pa.enables;
    nx_pa_advance_cycles(2000U * 170U);
    ASSERT_NO_FATAL_FAILURE(Submit());
    EXPECT_GT(g_pa.enables, enables);
    EXPECT_EQ(g_pa.primes, 4U);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474PWMADC, UnobservedCompletionPastDeadlineCannotInventTimelySuccess) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    nx_pa_advance_cycles(10001U * 170U);
    ASSERT_EQ(DMA1_Channel1->CNDTR, 0U);
    ASSERT_NE(DMA1->ISR & DMA_ISR_TCIF1, 0U);
    ASSERT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474PWMADC, PublicStopDrainsTheLoanAndClosesFurtherAdmission) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    const nx_result_t stopped = nx_pwm_adc_stop(&port);
    EXPECT_TRUE(stopped == NX_SUCCESS || stopped == NX_ERROR_BUSY);
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(TIM1->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(TIM1->BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(DMA1_Channel1->CCR & DMA_CCR_EN, 0U);
    ASSERT_EQ(nx_pwm_adc_prepare(&request, samples, 4U, &frame,
                                 nx_deadline_after(nx_time_now_us(), 10000U)),
              NX_SUCCESS);
    const unsigned before = g_pa.writes;
    EXPECT_EQ(nx_pwm_adc_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(g_pa.writes, before);
}
TEST_F(G474PWMADC, CurrentOneMillisecondPwmProfileIsRejectedWithoutEffects) {
    config.period_ticks = 1000U;
    EXPECT_EQ(nx_stm32g474_pwm_adc_initialize(&hardware, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_pa.writes, 0U);
    EXPECT_FALSE(hardware.retained);
}
TEST_F(G474PWMADC, MissingAdcClockRejectsBeforeReadingPeripheralOrAnyEffect) {
    RCC->AHB2ENR &= ~(uint32_t)RCC_AHB2ENR_ADC12EN;
    EXPECT_EQ(nx_stm32g474_pwm_adc_initialize(&hardware, &config),
              NX_ERROR_STATE);
    EXPECT_EQ(g_pa.unsupported_reads, 0U);
    EXPECT_EQ(g_pa.writes, 0U);
    EXPECT_FALSE(hardware.retained);
}
TEST_F(G474PWMADC, PrimeTimeoutIsAnAcceptedLoanUntilItsRealDrainCompletes) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    g_pa.hold_conversion = true;
    ASSERT_NO_FATAL_FAILURE(Submit());
    g_pa.hold_conversion = false;
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(g_pa.beats, 0U);
    EXPECT_EQ(samples[0], 0xDEADU);
}
TEST_F(G474PWMADC, RefusedTriggerRouteNeverStartsCounterOrReturnsPrimeData) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    g_pa.refused = &ADC1->CFGR;
    g_pa.held_bits = ADC_CFGR_EXTSEL;
    ASSERT_NO_FATAL_FAILURE(Submit());
    EXPECT_EQ(TIM1->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(g_pa.beats, 0U);
    g_pa.refused = nullptr;
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(samples[0], 0xDEADU);
}
TEST_F(G474PWMADC, RefusedDmaTargetCannotAdmitAPhysicalTrigger) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    g_pa.refused = &DMA1_Channel1->CMAR;
    g_pa.held_bits = UINT32_MAX;
    ASSERT_NO_FATAL_FAILURE(Submit());
    EXPECT_EQ(TIM1->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(g_pa.beats, 0U);
    g_pa.refused = nullptr;
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(samples[0], 0xDEADU);
}
TEST_F(G474PWMADC, UnsafePhaseAndDmaCountRejectBeforeAdmission) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    frame.trigger_ticks = 49U;
    ASSERT_EQ(nx_pwm_adc_prepare(&request, samples, 4U, &frame, 10000U),
              NX_SUCCESS);
    const unsigned writes = g_pa.writes;
    EXPECT_EQ(nx_pwm_adc_submit(&port, &request), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(g_pa.writes, writes);
    frame.trigger_ticks = 50U;
    request.sample_count = 65536U;
    request.frame = frame;
    EXPECT_EQ(nx_pwm_adc_submit(&port, &request), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(g_pa.writes, writes);
}
TEST_F(G474PWMADC, ComparatorBreakDoesNotPretendCounterOrAdcHasDrained) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    nx_pa_break(true);
    EXPECT_EQ(TIM1->BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_NE(TIM1->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    nx_stm32g474_pwm_adc_break_irq(&hardware);
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    nx_pa_break(false);
    nx_pa_advance_cycles(1000U * 170U);
    EXPECT_EQ(TIM1->BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(TIM1->CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(G474PWMADC, RefusedCounterStopCannotCreateAnEnableRouteRisingEdge) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_EQ(TIM1->CNT, 0U);
    g_pa.hold_counter_stop = true;
    const unsigned before = g_pa.edges;
    EXPECT_EQ(nx_pwm_adc_cancel(&port, &request), NX_SUCCESS);
    (void)nx_pwm_adc_service(&port);
    EXPECT_EQ(g_pa.edges, before);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(hardware.active, &request);
    g_pa.hold_counter_stop = false;
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
}
enum class Refusal { ADCStop, ADCDisable, DMADisable, AddressDetach };
class G474PWMADCRefusal : public G474PWMADC,
                          public testing::WithParamInterface<Refusal> {};
TEST_P(G474PWMADCRefusal, RefusedWithdrawalRetainsLoanUntilRealRetryProof) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    switch (GetParam()) {
        case Refusal::ADCStop:
            g_pa.hold_adc_stop = true;
            break;
        case Refusal::ADCDisable:
            g_pa.hold_adc_disable = true;
            break;
        case Refusal::DMADisable:
            g_pa.hold_dma_disable = true;
            break;
        case Refusal::AddressDetach:
            g_pa.hold_addresses = true;
            break;
    }
    EXPECT_EQ(nx_pwm_adc_cancel(&port, &request), NX_SUCCESS);
    for (unsigned i = 0U; i != 8U; ++i) {
        nx_pa_advance_cycles(170U);
        (void)nx_pwm_adc_service(&port);
    }
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(hardware.active, &request);
    EXPECT_NE(nx_stm32g474_pwm_adc_close(&hardware), NX_SUCCESS);
    EXPECT_TRUE(hardware.retained);
    g_pa.hold_adc_stop = false;
    g_pa.hold_adc_disable = false;
    g_pa.hold_dma_disable = false;
    g_pa.hold_addresses = false;
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}
INSTANTIATE_TEST_SUITE_P(IndependentSourceProofs, G474PWMADCRefusal,
                         testing::Values(Refusal::ADCStop, Refusal::ADCDisable,
                                         Refusal::DMADisable,
                                         Refusal::AddressDetach));
TEST_F(G474PWMADC, BusyConversionIgnoresTriggersAndDeadlineKeepsItsLoan) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    g_pa.hold_conversion = true;
    nx_pa_advance_cycles(1000U * 170U);
    EXPECT_GT(g_pa.ignored_edges, 0U);
    EXPECT_EQ(g_pa.beats, 0U);
    g_pa.hold_adc_stop = true;
    nx_pa_advance_cycles(10000U * 170U);
    (void)nx_pwm_adc_service(&port);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(hardware.active, &request);
    g_pa.hold_adc_stop = false;
    g_pa.hold_conversion = false;
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474PWMADC, OvrAndDmaErrorCannotPublishACompleteValidWindow) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ADC1->ISR |= ADC_ISR_OVR;
    DMA1->ISR |= DMA_ISR_TEIF1;
    DMAIRQ();
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_FALSE(g_pa.global_clear_while_active);
}
TEST_F(G474PWMADC, RealUnservedDmaRequestsRaiseOvrAndNeverInventAPrefix) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    g_pa.hold_dma_beat = true;
    nx_pa_advance_cycles(350U * 170U);
    EXPECT_NE(ADC1->ISR & ADC_ISR_OVR, 0U);
    EXPECT_EQ(g_pa.beats, 0U);
    EXPECT_EQ(samples[0], 0xDEADU);
    (void)nx_pwm_adc_service(&port);
    g_pa.hold_dma_beat = false;
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(G474PWMADC, LateVectorCannotAccessReusedSettledSampleStorage) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    nx_pa_advance_cycles(1000U * 170U);
    DMAIRQ();
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EXIT(
        {
            if (mprotect(samples, 4096U, PROT_NONE) != 0) {
                std::_Exit(2);
            }
            nx_stm32g474_pwm_adc_dma_irq(&hardware);
            nx_stm32g474_pwm_adc_break_irq(&hardware);
            std::_Exit(0);
        },
        testing::ExitedWithCode(0), "");
}
TEST_F(G474PWMADC, ColdCloseDoesNotReleaseAnActiveVectorOrRestartCounter) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    g_pa.irq_active[DMA1_Channel1_IRQn] = true;
    EXPECT_NE(nx_stm32g474_pwm_adc_close(&hardware), NX_SUCCESS);
    EXPECT_TRUE(hardware.retained);
    EXPECT_EQ(TIM1->CR1 & TIM_CR1_CEN, 0U);
    g_pa.irq_active[DMA1_Channel1_IRQn] = false;
    EXPECT_EQ(nx_stm32g474_pwm_adc_close(&hardware), NX_SUCCESS);
}
TEST_F(G474PWMADC, StateProjectsActualIntegerFormatAndSeparateTimeFacts) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    nx_pwm_adc_state_t facts{};
    ASSERT_EQ(nx_pwm_adc_state(&port, &facts), NX_SUCCESS);
    EXPECT_EQ(nx_adc_format_validate(&facts.format), NX_SUCCESS);
    EXPECT_EQ(facts.format.encoding, NX_ADC_UNSIGNED);
    EXPECT_EQ(facts.format.storage_bytes, 2U);
    EXPECT_EQ(facts.format.significant_bits, 12U);
    EXPECT_EQ(facts.format.sample_stride, 2U);
    EXPECT_EQ(facts.format.scan_stride, 2U);
    EXPECT_EQ(facts.format.group, NX_ADC_REGULAR);
    EXPECT_EQ(facts.format.trigger, NX_ADC_TRIGGER_TIMER);
    EXPECT_NE(facts.format.trigger_identity, 0U);
    EXPECT_EQ(facts.period_ticks, 100U);
    EXPECT_EQ(facts.trigger_ticks, 50U);
    EXPECT_FALSE(facts.running);
}
TEST_F(G474PWMADC, StateCannotOverwriteItsKnownNvicControlStorage) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    EXPECT_EXIT(
        {
            void* page =
                mmap(reinterpret_cast<void*>(SCS_BASE), 4096U, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            if (page != reinterpret_cast<void*>(SCS_BASE)) {
                std::_Exit(2);
            }
            auto* output = reinterpret_cast<nx_pwm_adc_state_t*>(
                const_cast<uint32_t*>(&NVIC->ISER[0]));
            /* The guard faults on either a read or a write, including under
             * ASan where the physical SCS address is not application memory. */
            const nx_result_t result = nx_pwm_adc_state(&port, output);
            std::_Exit(result == NX_ERROR_INVALID ? 0 : 3);
        },
        testing::ExitedWithCode(0), "");
}
TEST_F(G474PWMADC, KnownControlRequestRejectsBeforeDescriptorReads) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    EXPECT_EXIT(
        {
            void* page =
                mmap(reinterpret_cast<void*>(SCS_BASE), 4096U, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            if (page != reinterpret_cast<void*>(SCS_BASE)) {
                std::_Exit(2);
            }
            auto* descriptor = reinterpret_cast<nx_pwm_adc_request_t*>(
                const_cast<uint32_t*>(&NVIC->ISER[0]));
            const unsigned before = g_pa.writes;
            const nx_result_t result = nx_pwm_adc_submit(&port, descriptor);
            std::_Exit(result == NX_ERROR_INVALID && g_pa.writes == before ? 0
                                                                           : 3);
        },
        testing::ExitedWithCode(0), "");
}
TEST_F(G474PWMADC, CapturedLateDmaErrorCannotBeClearedBeforeItIsLatched) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    nx_pa_advance_cycles(1000U * 170U);
    DMAIRQ();
    late_dma_error_raised = false;
    g_pa.after_read = [](volatile uint32_t* address) {
        if (address == &DMA1->ISR &&
            (ADC1->CR & (ADC_CR_ADEN | ADC_CR_ADSTART)) == 0U &&
            (DMA1_Channel1->CCR & DMA_CCR_EN) == 0U) {
            /* A real flag may arrive after an earlier volatile snapshot.
             * Injection depends on physical quiet, never read ordinal. */
            DMA1->ISR |= DMA_ISR_TEIF1;
            late_dma_error_raised = true;
            g_pa.after_read = nullptr;
        }
    };
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_TRUE(late_dma_error_raised);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_TRUE(hardware.fault);
}
enum class LateFault { Overrun, DMAError, ComparatorBreak };
class G474PWMADCLateFault : public G474PWMADC,
                            public testing::WithParamInterface<LateFault> {};
TEST_P(G474PWMADCLateFault, WithdrawalCannotEraseALaterPhysicalFault) {
    ASSERT_NO_FATAL_FAILURE(Initialize());
    ASSERT_NO_FATAL_FAILURE(Submit());
    nx_pa_advance_cycles(1000U * 170U);
    DMAIRQ();
    ASSERT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    switch (GetParam()) {
        case LateFault::Overrun:
            g_pa.poll = [] {
                g_pa.poll = nullptr;
                ADC1->ISR |= ADC_ISR_OVR;
            };
            break;
        case LateFault::DMAError:
            g_pa.poll = [] {
                g_pa.poll = nullptr;
                DMA1->ISR |= DMA_ISR_TEIF1;
            };
            break;
        case LateFault::ComparatorBreak:
            g_pa.poll = [] {
                g_pa.poll = nullptr;
                nx_pa_break(true);
            };
            break;
    }
    ASSERT_NO_FATAL_FAILURE(ServiceUntilSettled());
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_TRUE(hardware.fault);
}
INSTANTIATE_TEST_SUITE_P(IndependentLateFaults, G474PWMADCLateFault,
                         testing::Values(LateFault::Overrun,
                                         LateFault::DMAError,
                                         LateFault::ComparatorBreak));
TEST(PWMADCDispatch, MissingMethodsAndSelfOverwritingRequestRejectSafely) {
    EXPECT_EQ(nx_pwm_adc_submit(nullptr, nullptr), NX_ERROR_INVALID);
    nx_pwm_adc_request_t request{};
    nx_request_initialize(&request.base);
    uint32_t duties[3]{1U, 2U, 3U};
    const nx_pwm_group_frame_t frame{duties, 3U, 50U};
    EXPECT_EQ(nx_pwm_adc_prepare(&request,
                                 reinterpret_cast<uint16_t*>(&request), 4U,
                                 &frame, 10000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    const nx_pwm_adc_ops_t absent{};
    unsigned context = 0U;
    const nx_pwm_adc_t port{&absent, &context};
    EXPECT_EQ(nx_pwm_adc_stop(&port), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(context, 0U);
}
TEST(PWMADCDispatch, PreparationRejectsBoundsAndAliasesWithoutMutation) {
    nx_pwm_adc_request_t request{};
    nx_request_initialize(&request.base);
    alignas(uint32_t) uint16_t samples[4]{};
    uint32_t duties[3]{1U, 2U, 3U};
    const nx_pwm_group_frame_t frame{duties, 3U, 50U};
    request.samples = samples;
    request.sample_count = 2U;
    request.frame = frame;
    request.base.result = NX_ERROR_IO;
    request.base.transferred = 17U;
    request.base.deadline = 222U;
    const auto Unchanged = [&] {
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
        EXPECT_EQ(request.base.result, NX_ERROR_IO);
        EXPECT_EQ(request.base.transferred, 17U);
        EXPECT_EQ(request.base.deadline, 222U);
        EXPECT_EQ(request.samples, samples);
        EXPECT_EQ(request.sample_count, 2U);
        EXPECT_EQ(request.frame.duty_ticks, duties);
        EXPECT_EQ(request.frame.channel_count, 3U);
        EXPECT_EQ(request.frame.trigger_ticks, 50U);
    };
    EXPECT_EQ(
        nx_pwm_adc_prepare(&request, samples, 4U, &frame, NX_DEADLINE_NEVER),
        NX_ERROR_INVALID);
    Unchanged();
    EXPECT_EQ(nx_pwm_adc_prepare(&request, samples, 0U, &frame, 333U),
              NX_ERROR_INVALID);
    Unchanged();
    EXPECT_EQ(nx_pwm_adc_prepare(&request, samples,
                                 SIZE_MAX / sizeof(*samples) + 1U, &frame,
                                 333U),
              NX_ERROR_INVALID);
    Unchanged();
    EXPECT_EQ(nx_pwm_adc_prepare(&request,
                                 reinterpret_cast<uint16_t*>(
                                     reinterpret_cast<uint8_t*>(samples) + 1U),
                                 1U, &frame, 333U),
              NX_ERROR_INVALID);
    Unchanged();
    EXPECT_EQ(nx_pwm_adc_prepare(&request,
                                 reinterpret_cast<uint16_t*>(UINTPTR_MAX - 1U),
                                 2U, &frame, 333U),
              NX_ERROR_INVALID);
    Unchanged();
    const nx_pwm_group_frame_t alias{reinterpret_cast<const uint32_t*>(samples),
                                     2U, 1U};
    EXPECT_EQ(nx_pwm_adc_prepare(&request, samples, 4U, &alias, 333U),
              NX_ERROR_INVALID);
    Unchanged();
    EXPECT_EQ(nx_pwm_adc_prepare(&request, samples, 4U, &request.frame, 333U),
              NX_ERROR_INVALID);
    Unchanged();
}
TEST(PWMADCDispatch, PreparationCannotRevokeBorrowAndCopiesFrameMetadata) {
    nx_pwm_adc_request_t request{};
    nx_request_initialize(&request.base);
    uint16_t first[4]{}, second[4]{};
    uint32_t duties[3]{1U, 2U, 3U};
    nx_pwm_group_frame_t frame{duties, 3U, 50U};
    ASSERT_EQ(nx_pwm_adc_prepare(&request, first, 4U, &frame, 333U),
              NX_SUCCESS);
    frame.trigger_ticks = 27U;
    EXPECT_EQ(request.frame.trigger_ticks, 50U);
    ASSERT_EQ(nx_request_admit(&request.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    EXPECT_EQ(nx_pwm_adc_prepare(&request, second, 4U, &frame, 444U),
              NX_ERROR_STATE);
    EXPECT_EQ(request.samples, first);
    EXPECT_EQ(request.base.deadline, 333U);
    EXPECT_EQ(request.frame.trigger_ticks, 50U);
    nx_request_settle(&request.base, NX_ERROR_CANCELLED, 0U);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(nx_pwm_adc_prepare(&request, second, 4U, &frame, 444U),
              NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(request.samples, second);
    EXPECT_EQ(request.base.deadline, 444U);
    EXPECT_EQ(request.frame.trigger_ticks, 27U);
}
} /* namespace */
