/**
 * \file            g474_adc_group_factory_test.cpp
 * \brief           Normal TOML generated owner and retained outer leases
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_adc_group_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32g474_adc_group.h"
#include <cstdlib>
#include <gtest/gtest.h>
#include <sys/mman.h>
extern "C" void ADC1_2_IRQHandler(void);
#ifndef NX_AG_FACTORY_INJECTED
extern "C" void DMA1_Channel1_IRQHandler(void);
#endif
namespace {
class G474ADCGroupFactory : public testing::Test {
  protected:
    uint16_t* frames{};
    nx_adc_group_request_t request{};
    void SetUp() override {
        nx_ag_reset(0x2003U);
        nx_g474_model_reset();
        g_ag_factory_refuse_priority = false;
        void* mapped = mmap(reinterpret_cast<void*>(0x20000000U), 4096U,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_EQ(mapped, reinterpret_cast<void*>(0x20000000U));
        frames = static_cast<uint16_t*>(mapped);
        for (size_t i = 0U; i != 2048U; ++i) {
            frames[i] = 0xDEADU;
        }
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        g_ag.refused = nullptr;
        g_ag.before_read = nullptr;
        g_ag.before_write = nullptr;
        g_ag.event = nullptr;
        g_ag.hold_conversion = false;
        g_ag.hold_trigger_sync = false;
        g_ag.hold_dma = false;
        g_ag.hold_adc_stop = false;
        g_ag.hold_adc_disable = false;
        g_ag.hold_dma_disable = false;
        g_ag.hold_counter_stop = false;
        g_ag.hold_irq_disable = false;
        g_ag.in_isr = false;
        g_ag_factory_refuse_priority = false;
        g_g474_model.hold_irq_off = false;
        g_g474_model.ignore_write = nullptr;
        g_g474_model.irq_active[ADC1_2_IRQn] = false;
        g_g474_model.irq_active[DMA1_Channel1_IRQn] = false;
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned i = 0U; i != 2048U && result != NX_SUCCESS; ++i) {
            nx_ag_factory_advance(1U);
            result = nx_platform_stop();
        }
        if (result != NX_SUCCESS ||
            (nx_request_state(&request.base) != NX_REQUEST_READY &&
             nx_request_state(&request.base) != NX_REQUEST_SETTLED)) {
            std::abort();
        }
        EXPECT_EQ(g_ag.forbidden_writes, 0U);
        EXPECT_EQ(munmap(frames, 4096U), 0);
    }
    static const nx_adc_group_port_t* samples() {
        return nx_factory_adc_group(NX_ADC_GROUP_ID_SAMPLES);
    }
    static size_t count() {
#ifdef NX_AG_FACTORY_INJECTED
        return 1U;
#else
        return 3U;
#endif
    }
    void Start() {
        ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    }
    void Submit() {
        nx_adc_group_info_t info{};
        ASSERT_EQ(nx_adc_group_info(samples(), &info), NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_prepare(
                      &request, frames, info.format.scan_stride * count(),
                      count(), nx_deadline_after(nx_time_now_us(), 10000U)),
                  NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_submit(samples(), &request), NX_SUCCESS);
    }
    void Complete() {
        for (unsigned i = 0U; i != 10000U && nx_request_state(&request.base) !=
                                                 NX_REQUEST_SETTLED;
             ++i) {
            nx_ag_factory_advance(1U);
            if (g_g474_model.irq_pending[ADC1_2_IRQn] &&
                g_g474_model.irq_enabled[ADC1_2_IRQn]) {
                g_g474_model.irq_pending[ADC1_2_IRQn] = false;
                g_g474_model.irq_active[ADC1_2_IRQn] = true;
                g_ag.in_isr = true;
                ADC1_2_IRQHandler();
                g_ag.in_isr = false;
                g_g474_model.irq_active[ADC1_2_IRQn] = false;
            }
#ifndef NX_AG_FACTORY_INJECTED
            if (g_g474_model.irq_pending[DMA1_Channel1_IRQn] &&
                g_g474_model.irq_enabled[DMA1_Channel1_IRQn]) {
                g_g474_model.irq_pending[DMA1_Channel1_IRQn] = false;
                g_g474_model.irq_active[DMA1_Channel1_IRQn] = true;
                g_ag.in_isr = true;
                DMA1_Channel1_IRQHandler();
                g_ag.in_isr = false;
                g_g474_model.irq_active[DMA1_Channel1_IRQn] = false;
            }
#endif
            (void)nx_adc_group_service(samples());
        }
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    }
};
TEST_F(G474ADCGroupFactory, PureTypedLookupRetainsOnlyOneSelectedFace) {
    ASSERT_NE(samples(), nullptr);
    EXPECT_EQ(samples(), samples());
    EXPECT_EQ(samples()->ops, &nx_stm32g474_adc_group_ops);
    EXPECT_EQ(nx_factory_adc_group(NX_ADC_GROUP_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_adc(NX_ADC_ID_COUNT), nullptr);
    EXPECT_EQ(g_g474_model.reads, 0U);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroupFactory, ActualNormalAssemblyProvidesExactGroupAndTrigger) {
    ASSERT_NO_FATAL_FAILURE(Start());
    nx_adc_group_info_t info{};
    ASSERT_EQ(nx_adc_group_info(samples(), &info), NX_SUCCESS);
    EXPECT_EQ(info.format.channel_count, 2U);
    EXPECT_EQ(info.format.scan_stride, 4U);
    EXPECT_EQ(info.adc_clock_hz, 21250000U);
#ifdef NX_AG_FACTORY_INJECTED
    EXPECT_EQ(info.format.group, NX_ADC_INJECTED);
    EXPECT_FALSE(g_g474_model.irq_enabled[DMA1_Channel1_IRQn]);
#else
    EXPECT_EQ(info.format.group, NX_ADC_REGULAR);
#endif
#ifdef NX_AG_FACTORY_SOFTWARE
    EXPECT_EQ(info.format.trigger, NX_ADC_TRIGGER_SOFTWARE);
    EXPECT_EQ(info.format.trigger_identity, 0U);
    EXPECT_EQ(RCC->APB1ENR1 & RCC_APB1ENR1_TIM6EN, 0U);
#else
    EXPECT_EQ(info.format.trigger, NX_ADC_TRIGGER_TIMER);
    EXPECT_NE(info.format.trigger_identity, 0U);
    EXPECT_NE(RCC->APB1ENR1 & RCC_APB1ENR1_TIM6EN, 0U);
#endif
}
TEST_F(G474ADCGroupFactory, GeneratedOwnerExecutesRealMultiRankAcquisition) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Complete());
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, count());
    nx_adc_group_info_t info{};
    ASSERT_EQ(nx_adc_group_info(samples(), &info), NX_SUCCESS);
    for (size_t frame = 0U; frame != count(); ++frame) {
        for (size_t rank = 0U; rank != 2U; ++rank) {
            int64_t value = -1;
            ASSERT_EQ(nx_adc_format_decode(&info.format, frames,
                                           info.format.scan_stride * count(),
                                           frame, rank, &value),
                      NX_SUCCESS);
            EXPECT_EQ(value, g_ag.values[rank == 0U ? 2U : 1U]);
        }
    }
    EXPECT_EQ(ADC1->CR & (ADC_CR_ADEN | ADC_CR_ADSTART | ADC_CR_JADSTART), 0U);
    EXPECT_EQ(DMA1_Channel1->CMAR, 0U);
    EXPECT_EQ(TIM6->CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(G474ADCGroupFactory, ForeignAdcClockRejectsBeforeOuterEffects) {
    RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_BUSY);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_EQ(g_ag.writes, 0U);
}
TEST_F(G474ADCGroupFactory,
       RefusedCalibrationPreservesFailedConstructionMarker) {
    g_ag.refused = &ADC1->CR;
    g_ag.held_bits = ADC_CR_ADCAL;
    const auto result = nx_platform_start();
    EXPECT_NE(result.primary, NX_SUCCESS);
    g_ag.refused = nullptr;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN, 0U);
    EXPECT_EQ(GPIOA->MODER, 0U);
}
TEST_F(G474ADCGroupFactory, ActiveAdcVectorRetainsCommonClockAndPads) {
    ASSERT_NO_FATAL_FAILURE(Start());
    g_g474_model.irq_active[ADC1_2_IRQn] = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN, 0U);
    EXPECT_EQ(GPIOA->MODER & 15U, 15U);
    g_g474_model.irq_active[ADC1_2_IRQn] = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
}
TEST_F(G474ADCGroupFactory, LogicalPriorityRestoreFailureRetainsColdLeases) {
    g_g474_model.priority[ADC1_2_IRQn] = 3U;
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    ASSERT_NO_FATAL_FAILURE(Complete());
    g_ag_factory_refuse_priority = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN, 0U);
    g_ag_factory_refuse_priority = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(g_g474_model.priority[ADC1_2_IRQn], 3U);
}
TEST_F(G474ADCGroupFactory, RefusedAdcDisableKeepsRequestAndEveryOuterLease) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_NO_FATAL_FAILURE(Submit());
    g_ag.hold_adc_disable = true;
    EXPECT_NE(nx_platform_stop(), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN, 0U);
    g_ag.hold_adc_disable = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}
} /* namespace */
