/**
 * \file            stm32_adc_cold_factory_test.cpp
 * \brief           Actual multi-bank ADC factory and retained cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32f407_control_cold.h"
#include "stm32f407_provider.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
}
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
volatile uint32_t* refused;
bool refuse_restore;
bool refuse_powerdown;
bool privileged;
ADC_TypeDef* gated_foreign;
uint32_t gated_clock;
uint32_t foreign_control;
uint32_t refuse_probe_enable;
uint32_t refuse_probe_release;
unsigned starts;
unsigned stops;
nx_time_us_t now;
#if defined(NX_ADC_INTERNAL_CONTRACT)
bool fractional_clock;
uint64_t clock_ns;
uint64_t enabled_ns;
uint64_t conversion_ns;
bool injected_complete;
unsigned injected_launches;
uint32_t injected_sequence;
#endif
constexpr uint32_t original_ahb1 = RCC_AHB1ENR_GPIODEN;

class AdcColdFactory : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        refused = nullptr;
        refuse_restore = false;
        refuse_powerdown = false;
        privileged = true;
        gated_foreign = nullptr;
        gated_clock = foreign_control = 0U;
        refuse_probe_enable = refuse_probe_release = 0U;
        starts = stops = 0U;
        now = 100U;
#if defined(NX_ADC_INTERNAL_CONTRACT)
        fractional_clock = false;
        clock_ns = enabled_ns = conversion_ns = 0U;
        injected_complete = true;
        injected_launches = 0U;
        injected_sequence = 0U;
#endif
        RCC->AHB1ENR = original_ahb1;
    }
    void TearDown() override {
        refused = nullptr;
        refuse_restore = false;
        refuse_powerdown = false;
        privileged = true;
        gated_foreign = nullptr;
        refuse_probe_enable = refuse_probe_release = 0U;
        if (mmio != MAP_FAILED) {
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
            EXPECT_EQ(0, munmap(mmio, 0x80000U));
        }
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
    }
    void Restored() {
        EXPECT_EQ(original_ahb1, RCC->AHB1ENR);
        EXPECT_EQ(0U, RCC->APB2ENR);
        EXPECT_EQ(0U, GPIOA->MODER);
        EXPECT_EQ(0U, GPIOB->MODER);
        EXPECT_EQ(0U, GPIOC->MODER);
    }
};
} /* namespace */

#if defined(NX_ADC_INTERNAL_CONTRACT)
TEST_F(AdcColdFactory, StartupWaitCoversTheFractionalMicrosecondPhase) {
    const uint8_t channels[]{0U};
    const uint8_t timing[]{7U};
    const nx_stm32_adc_config_t config{channels, timing, 1U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_prepare(&state, &config));
    fractional_clock = true;
    clock_ns = now * 1000U;
    const nx_adc_port_t face{&nx_stm32_adc_ops, &state};
    uint16_t sample{};
    size_t count{};
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_port_sample(&face, &sample, 1U, now + 1000U, &count));
    EXPECT_NE(0U, enabled_ns);
    EXPECT_GE(conversion_ns - enabled_ns, 3000U);
    fractional_clock = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
}

TEST_F(AdcColdFactory, InternalRawScanUsesNoPadsAndWaitsForItsSource) {
    const uint8_t channels[]{16U, 17U, 18U};
    const uint8_t timing[]{7U, 7U, 7U};
    const nx_stm32_adc_config_t config{channels, timing, 3U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_prepare(&state, &config));
    const uint32_t paths = ADC_CCR_TSVREFE | ADC_CCR_VBATE;
    EXPECT_EQ(paths, ADC->CCR & paths);
    EXPECT_EQ(original_ahb1, RCC->AHB1ENR);
    EXPECT_EQ(0U, GPIOA->MODER | GPIOB->MODER | GPIOC->MODER);
    fractional_clock = true;
    clock_ns = now * 1000U;
    const nx_adc_port_t face{&nx_stm32_adc_ops, &state};
    uint16_t data[3]{};
    size_t count{};
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_port_sample(&face, data, 3U, now + 1000U, &count));
    EXPECT_GE(conversion_ns - enabled_ns, 10000U);
    EXPECT_EQ(3U, count);
    EXPECT_EQ(1700U, data[0]);
    EXPECT_EQ(1800U, data[1]);
    EXPECT_EQ(1900U, data[2]);
    fractional_clock = false;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(0U, ADC->CCR & paths);
    EXPECT_EQ(original_ahb1, RCC->AHB1ENR);
}

TEST_F(AdcColdFactory, InternalAnalogOwnershipSurvivesRefusedRelease) {
    const uint8_t channel[]{17U};
    const uint8_t timing[]{7U};
    const nx_stm32_adc_config_t config{channel, timing, 1U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_prepare(&state, &config));
    refused = &ADC->CCR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_adc_close(&state));
    EXPECT_NE(0U, ADC->CCR & ADC_CCR_TSVREFE);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    EXPECT_NE(0U, state.cold_effects);
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(0U, ADC->CCR & ADC_CCR_TSVREFE);
    EXPECT_EQ(0U, state.cold_effects);
}

TEST_F(AdcColdFactory, RefusedSourceEnableDoesNotLeaveAnUntrackedEffect) {
    const uint8_t channel[]{16U};
    const uint8_t timing[]{7U};
    const nx_stm32_adc_config_t config{channel, timing, 1U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    ADC->CCR = ADC_CCR_ADCPRE_0;
    refused = &ADC->CCR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_adc_prepare(&state, &config));
    EXPECT_NE(0U, state.cold_effects);
    EXPECT_FALSE(state.initialized);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(0U, state.cold_effects);
    EXPECT_EQ(0U, RCC->APB2ENR);
    refused = nullptr;
}

TEST_F(AdcColdFactory, AnalogReleaseWaitsForPowerdownAndPreservesUnownedPath) {
    const uint8_t channel[]{17U};
    const uint8_t timing[]{7U};
    const nx_stm32_adc_config_t config{channel, timing, 1U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    ADC->CCR = ADC_CCR_ADCPRE_0 | ADC_CCR_VBATE;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_prepare(&state, &config));
    ADC1->CR2 = ADC_CR2_ADON;
    refuse_powerdown = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_adc_close(&state));
    EXPECT_NE(0U, ADC->CCR & ADC_CCR_TSVREFE);
    EXPECT_NE(0U, ADC->CCR & ADC_CCR_VBATE);
    refuse_powerdown = false;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(0U, ADC->CCR & ADC_CCR_TSVREFE);
    EXPECT_NE(0U, ADC->CCR & ADC_CCR_VBATE);
}

TEST_F(AdcColdFactory,
       InternalSourcesRejectOtherConvertersAndShortAcquisition) {
    const uint8_t channel[]{16U};
    for (auto* adc : {ADC1, ADC2, ADC3}) {
        const uint8_t timing[]{static_cast<uint8_t>(adc == ADC1 ? 6U : 7U)};
        const nx_stm32_adc_config_t config{channel, timing, 1U, 3300U, 0U, adc};
        nx_stm32_adc_state_t state{};
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_adc_prepare(&state, &config));
        EXPECT_EQ(0U, RCC->APB2ENR);
        EXPECT_EQ(0U, ADC->CCR);
        EXPECT_EQ(0U, state.cold_effects);
    }
}

TEST_F(AdcColdFactory, InjectedGroupsUseEveryRankAndEachConverterJdr) {
    const uint8_t channels[]{3U, 2U, 1U, 0U};
    const uint8_t timings[]{7U, 6U, 5U, 4U};
    for (auto* adc : {ADC1, ADC2, ADC3}) {
        for (size_t ranks = 1U; ranks <= 4U; ++ranks) {
            nx_stm32_adc_state_t state{};
            const nx_stm32_adc_config_t config{channels,
                                               timings,
                                               ranks,
                                               3300U,
                                               static_cast<uint8_t>(ranks - 1U),
                                               adc};
            ASSERT_EQ(NX_SUCCESS,
                      nx_stm32_adc_injected_prepare(&state, &config));
            const nx_adc_port_t face{&nx_stm32_adc_injected_ops, &state};
            uint16_t samples[4]{};
            size_t count = 0U;
            injected_launches = 0U;
            adc->JOFR1 = adc->JOFR2 = adc->JOFR3 = adc->JOFR4 = 7U;
            ASSERT_EQ(NX_SUCCESS, nx_adc_port_sample(&face, samples, 4U,
                                                     now + 200U, &count));
            EXPECT_EQ(ranks, count);
            EXPECT_EQ(1U, injected_launches);
            uint32_t expected = static_cast<uint32_t>(ranks - 1U) << 20U;
            for (size_t i = 0U; i < ranks; ++i) {
                expected |= static_cast<uint32_t>(channels[i])
                            << ((i + 4U - ranks) * 5U);
                const unsigned bits =
                    12U - 2U * static_cast<unsigned>(ranks - 1U);
                EXPECT_EQ((321U + i * 111U) & ((1U << bits) - 1U), samples[i]);
            }
            EXPECT_EQ(expected, injected_sequence);
            EXPECT_EQ(ranks - 1U, (adc->CR1 & ADC_CR1_RES) >> ADC_CR1_RES_Pos);
            EXPECT_EQ(0U, adc->JOFR1);
            if (ranks >= 2U) {
                EXPECT_EQ(0U, adc->JOFR2);
            }
            if (ranks >= 3U) {
                EXPECT_EQ(0U, adc->JOFR3);
            }
            if (ranks >= 4U) {
                EXPECT_EQ(0U, adc->JOFR4);
            }
            EXPECT_EQ(0U, adc->SQR1 | adc->SQR2 | adc->SQR3);
            EXPECT_EQ(0U, adc->CR2);
            ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_injected_close(&state));
        }
    }
}

TEST_F(AdcColdFactory, InjectedInternalRanksUseAnalogLeaseAndStartupMargin) {
    const uint8_t channels[]{16U, 17U, 18U};
    const uint8_t timings[]{7U, 7U, 7U};
    const nx_stm32_adc_config_t config{channels, timings, 3U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    fractional_clock = true;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_injected_prepare(&state, &config));
    const nx_adc_port_t face{&nx_stm32_adc_injected_ops, &state};
    uint16_t samples[3]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_port_sample(&face, samples, 3U, now + 100U, &count));
    EXPECT_EQ(1U, injected_launches);
    EXPECT_GE(conversion_ns - enabled_ns, 10000U);
    EXPECT_EQ(321U, samples[0]);
    EXPECT_EQ(432U, samples[1]);
    EXPECT_EQ(543U, samples[2]);
    EXPECT_EQ(0U, GPIOA->MODER | GPIOB->MODER | GPIOC->MODER);
    EXPECT_NE(0U, ADC->CCR & ADC_CCR_TSVREFE);
    EXPECT_NE(0U, ADC->CCR & ADC_CCR_VBATE);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_injected_close(&state));
    EXPECT_EQ(0U, ADC->CCR & (ADC_CCR_TSVREFE | ADC_CCR_VBATE));
}

TEST_F(AdcColdFactory, InjectedOversizeRejectsBeforeClockOrControllerEffects) {
    const uint8_t channels[]{0U, 1U, 2U, 3U, 4U};
    const uint8_t timings[]{7U, 7U, 7U, 7U, 7U};
    const nx_stm32_adc_config_t config{channels, timings, 5U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_adc_injected_prepare(&state, &config));
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0U, state.cold_effects);
    EXPECT_EQ(0U, GPIOA->MODER);
    if (state.cold_effects != 0U) {
        EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_injected_close(&state));
    }
}

TEST_F(AdcColdFactory, InjectedTimeoutAndRefusedPowerdownKeepCleanupOnly) {
    const uint8_t channels[]{0U, 1U};
    const uint8_t timings[]{7U, 7U};
    const nx_stm32_adc_config_t config{channels, timings, 2U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_injected_prepare(&state, &config));
    const nx_adc_port_t face{&nx_stm32_adc_injected_ops, &state};
    uint16_t samples[]{0xa5a5U, 0x5a5aU};
    size_t count = 0U;
    injected_complete = false;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_adc_port_sample(&face, samples, 2U, now + 100U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(0xa5a5U, samples[0]);
    EXPECT_EQ(0x5a5aU, samples[1]);
    injected_complete = true;
    refuse_powerdown = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&face, samples, 2U, now + 100U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_TRUE(state.active);
    EXPECT_EQ(0xa5a5U, samples[0]);
    EXPECT_EQ(0x5a5aU, samples[1]);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_adc_injected_close(&state));
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    refuse_powerdown = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_injected_close(&state));
}

TEST_F(AdcColdFactory, InternalDmaModesRetainAnalogPowerUntilControllerClose) {
    const uint8_t channels[]{16U, 17U, 18U};
    const uint8_t timing[]{7U, 7U, 7U};
    uint16_t storage[3][3]{};
    const nx_dma_memory_region_t region{reinterpret_cast<uintptr_t>(storage),
                                        sizeof(storage), NX_DMA_MEMORY_WRITE};
    const nx_stm32_adc_stream_config_t config{
        {channels, timing, 3U, 3300U, 0U, ADC1}, &region, 1U, 5U, 0U};
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    for (bool continuous : {false, true}) {
        SCOPED_TRACE(continuous);
        nx_stm32_adc_continuous_state_t state{};
        nx_stream_slot_t slots[3]{};
        for (unsigned index = 0U; index < 3U; ++index) {
            slots[index].data = reinterpret_cast<uint8_t*>(storage[index]);
            slots[index].capacity = sizeof(storage[index]);
        }
        nx_stream_t stream{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, slots, 3U));
        ASSERT_EQ(NX_SUCCESS,
                  continuous
                      ? nx_stm32_adc_continuous_prepare(&state, &config)
                      : nx_stm32_adc_stream_prepare(&state.base, &config));
        const nx_adc_port_t face{continuous ? &nx_stm32_adc_continuous_ops
                                            : &nx_stm32_adc_stream_ops,
                                 continuous ? static_cast<void*>(&state)
                                            : static_cast<void*>(&state.base)};
        const auto start = now;
        ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_start(&face, &stream, 1000U));
        EXPECT_GE(state.base.ready_at - start, 11U);
        EXPECT_EQ(0U, TIM3->CR1 & TIM_CR1_CEN);
        now += 12U;
        ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_service(&face));
        EXPECT_NE(0U, TIM3->CR1 & TIM_CR1_CEN);
        ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_stop(&face));
        EXPECT_NE(0U, ADC->CCR & ADC_CCR_TSVREFE);
        EXPECT_NE(0U, ADC->CCR & ADC_CCR_VBATE);
        ASSERT_EQ(NX_SUCCESS, continuous
                                  ? nx_stm32_adc_continuous_close(&state)
                                  : nx_stm32_adc_stream_close(&state.base));
        EXPECT_EQ(0U, ADC->CCR & (ADC_CCR_TSVREFE | ADC_CCR_VBATE));
        EXPECT_EQ(0U, RCC->APB2ENR);
    }
}

TEST_F(AdcColdFactory, InternalPathDoesNotAdoptPreexistingAnalogOwnership) {
    const uint8_t channel[]{17U};
    const uint8_t timing[]{7U};
    const nx_stm32_adc_config_t config{channel, timing, 1U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    ADC->CCR = ADC_CCR_ADCPRE_0 | ADC_CCR_TSVREFE;
    ADC1->CR2 = ADC_CR2_ALIGN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_prepare(&state, &config));
    EXPECT_NE(0U, ADC->CCR & ADC_CCR_TSVREFE);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_NE(0U, ADC->CCR & ADC_CCR_TSVREFE);
    EXPECT_EQ(ADC_CR2_ALIGN, ADC1->CR2);
    EXPECT_EQ(0U, RCC->APB2ENR);
}
#endif

TEST_F(AdcColdFactory, LookupIsPureAndStopRestoresAllThreeBanks) {
    const auto* port = nx_factory_adc(static_cast<nx_adc_id_t>(0));
    ASSERT_NE(nullptr, port);
    EXPECT_EQ(0U, starts);
    Restored();
    Start();
    nx_adc_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_info(port, &info));
    EXPECT_EQ(16U, info.channel_count);
    EXPECT_EQ(NX_TEST_ADC_BITS, info.resolution_bits);
    EXPECT_EQ((12U - NX_TEST_ADC_BITS) / 2U,
              (ADC1->CR1 & ADC_CR1_RES) >> ADC_CR1_RES_Pos);
    EXPECT_EQ(0xffffU, GPIOA->MODER);
    EXPECT_EQ(0xfU, GPIOB->MODER);
    EXPECT_EQ(0xfffU, GPIOC->MODER);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, starts);
    EXPECT_EQ(1U, stops);
    Restored();
}

TEST_F(AdcColdFactory, RefusedAdcShutdownKeepsEveryPadAndTheSystemAlive) {
    Start();
    ADC1->CR2 = ADC_CR2_ADON;
    refused = &ADC1->CR2;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    EXPECT_EQ(0xffffU, GPIOA->MODER);
    EXPECT_EQ(0xfU, GPIOB->MODER);
    EXPECT_EQ(0xfffU, GPIOC->MODER);
    EXPECT_EQ(0U, stops);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, stops);
    Restored();
}

TEST_F(AdcColdFactory, RefusedBankRestoreRetainsClocksAndRetriesRemainingPads) {
    Start();
    refuse_restore = true;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    EXPECT_EQ(0U, stops);
    EXPECT_EQ(3U, GPIOB->MODER & 3U);
    EXPECT_EQ(0xffffU, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOC->MODER);
    EXPECT_EQ(7U, RCC->AHB1ENR & 7U);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    refuse_restore = false;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, stops);
    Restored();
}

TEST_F(AdcColdFactory, RejectedForeignConverterIsNeverStoppedByCleanup) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{0U};
    nx_stm32_adc_config_t config{};
    config.channels = channels;
    config.sample_times = samples;
    config.channel_count = 1U;
    config.reference_mv = 3300U;
    config.registers = ADC1;
    nx_stm32_adc_state_t state{};
    const uint32_t foreign = ADC_CR2_ADON | ADC_CR2_CONT;
    ADC1->CR2 = foreign;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_prepare(&state, &config));
    EXPECT_EQ(foreign, ADC1->CR2);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(foreign, ADC1->CR2);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0U, GPIOA->MODER);
}

TEST_F(AdcColdFactory, IndependentConverterRejectsForeignCommonMultimode) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{0U};
    nx_stm32_adc_config_t config{};
    config.channels = channels;
    config.sample_times = samples;
    config.channel_count = 1U;
    config.reference_mv = 3300U;
    config.registers = ADC1;
    nx_stm32_adc_state_t state{};
    const uint32_t foreign = ADC_CCR_MULTI_0 | ADC_CCR_DMA_0;
    ADC->CCR = foreign;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_prepare(&state, &config));
    EXPECT_EQ(foreign, ADC->CCR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(foreign, ADC->CCR);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0U, GPIOA->MODER);
}

TEST_F(AdcColdFactory, ThreeConvertersHaveIndependentClocksPadsAndResults) {
    ADC_TypeDef* const converters[]{ADC1, ADC2, ADC3};
    const uint32_t clocks[]{RCC_APB2ENR_ADC1EN, RCC_APB2ENR_ADC2EN,
                            RCC_APB2ENR_ADC3EN};
    const uint8_t channels[]{4U, 8U, 10U};
    const uint8_t timing[]{7U};
    nx_stm32_adc_state_t states[3]{};
    RCC->AHB1ENR |=
        RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    for (size_t index = 0U; index < 3U; ++index) {
        nx_stm32_adc_config_t config{};
        config.channels = &channels[index];
        config.sample_times = timing;
        config.channel_count = 1U;
        config.reference_mv = 3300U;
        config.resolution = static_cast<uint8_t>(index);
        config.registers = converters[index];
        ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_prepare(&states[index], &config));
        EXPECT_EQ(converters[index], states[index].registers);
        EXPECT_NE(0U, RCC->APB2ENR & clocks[index]);
        const nx_adc_port_t face{&nx_stm32_adc_ops, &states[index]};
        uint16_t sample{};
        size_t count{};
        ASSERT_EQ(NX_SUCCESS,
                  nx_adc_port_sample(&face, &sample, 1U, now + 1000U, &count));
        EXPECT_EQ(1U, count);
        const unsigned bits = 12U - static_cast<unsigned>(index) * 2U;
        EXPECT_EQ((100U + 100U * channels[index]) & ((1U << bits) - 1U),
                  sample);
    }
    EXPECT_EQ(3U << 8U, GPIOA->MODER);
    EXPECT_EQ(3U, GPIOB->MODER);
    EXPECT_EQ(3U, GPIOC->MODER);
    const uint32_t common = ADC->CCR;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&states[1]));
    EXPECT_EQ(clocks[0] | clocks[2], RCC->APB2ENR);
    EXPECT_TRUE(states[0].initialized);
    EXPECT_TRUE(states[2].initialized);
    EXPECT_EQ(common, ADC->CCR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&states[0]));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&states[2]));
    EXPECT_EQ(0U, RCC->APB2ENR);
}

TEST_F(AdcColdFactory, EveryAdc3ExternalChannelUsesItsDatasheetPad) {
    /* DS8626 Table7, expressed as physical pads independently of packed data.
     */
    const struct {
        GPIO_TypeDef* gpio;
        uint8_t pin;
    } pads[] = {{GPIOA, 0U},  {GPIOA, 1U}, {GPIOA, 2U}, {GPIOA, 3U},
                {GPIOF, 6U},  {GPIOF, 7U}, {GPIOF, 8U}, {GPIOF, 9U},
                {GPIOF, 10U}, {GPIOF, 3U}, {GPIOC, 0U}, {GPIOC, 1U},
                {GPIOC, 2U},  {GPIOC, 3U}, {GPIOF, 4U}, {GPIOF, 5U}};
    const uint8_t samples[]{7U};
    RCC->AHB1ENR |=
        RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIOFEN;
    for (uint8_t channel = 0U; channel < 16U; ++channel) {
        SCOPED_TRACE(static_cast<unsigned>(channel));
        GPIOA->MODER = GPIOC->MODER = GPIOF->MODER = 0U;
        const nx_stm32_adc_config_t config{&channel, samples, 1U,
                                           3300U,    0U,      ADC3};
        nx_stm32_adc_state_t state{};
        ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_prepare(&state, &config));
        for (auto* gpio : {GPIOA, GPIOC, GPIOF}) {
            const uint32_t mask = gpio == pads[channel].gpio
                                      ? 3U << (2U * pads[channel].pin)
                                      : 0U;
            EXPECT_EQ(mask, gpio->MODER);
        }
        ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
        EXPECT_EQ(0U, RCC->APB2ENR);
    }
}

TEST_F(AdcColdFactory, RefusedShotPowerdownRetainsOnlyDeviceCleanup) {
    Start();
    const auto* port = nx_factory_adc(static_cast<nx_adc_id_t>(0));
    ASSERT_NE(nullptr, port);
    refused = &ADC1->CR2;
    refuse_powerdown = true;
    uint16_t samples[16]{};
    size_t count{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(port, samples, 16U, now + 10000U, &count));
    EXPECT_EQ(16U, count);
    EXPECT_NE(0U, ADC1->CR2 & ADC_CR2_ADON);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_adc_port_sample(port, samples, 16U, now + 10000U, &count));
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    refused = nullptr;
    refuse_powerdown = false;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    Restored();
}

TEST_F(AdcColdFactory, MatchingCommonClockPreservesARunningSibling) {
    const uint8_t channels[]{8U};
    const uint8_t samples[]{7U};
    nx_stm32_adc_config_t config{};
    config.channels = channels;
    config.sample_times = samples;
    config.channel_count = 1U;
    config.reference_mv = 3300U;
    config.registers = ADC2;
    nx_stm32_adc_state_t state{};
    RCC->APB2ENR = RCC_APB2ENR_ADC1EN;
    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_DMA;
    ADC->CCR = ADC_CCR_ADCPRE_0;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_prepare(&state, &config));
    EXPECT_EQ(ADC_CCR_ADCPRE_0, ADC->CCR);
    EXPECT_EQ(ADC_CR2_ADON | ADC_CR2_DMA, ADC1->CR2);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(RCC_APB2ENR_ADC1EN, RCC->APB2ENR);
    EXPECT_EQ(ADC_CR2_ADON | ADC_CR2_DMA, ADC1->CR2);
}

TEST_F(AdcColdFactory, CommonClockCannotBeRetunedWithAnyConverterEnabled) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    nx_stm32_adc_config_t config{};
    config.channels = channels;
    config.sample_times = samples;
    config.channel_count = 1U;
    config.reference_mv = 3300U;
    config.registers = ADC1;
    nx_stm32_adc_state_t state{};
    RCC->APB2ENR = RCC_APB2ENR_ADC2EN;
    ADC2->CR2 = ADC_CR2_ADON;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_prepare(&state, &config));
    EXPECT_EQ(0U, ADC->CCR);
    EXPECT_EQ(0U, GPIOA->MODER);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(RCC_APB2ENR_ADC2EN, RCC->APB2ENR);
    EXPECT_EQ(ADC_CR2_ADON, ADC2->CR2);
}

TEST_F(AdcColdFactory, SecondaryConvertersSelectTheirOwnDmaStreams) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_dma_memory_region_t region{UINT32_C(0x20000000), 1024U,
                                        NX_DMA_MEMORY_WRITE};
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    NVIC_EnableIRQ(DMA2_Stream4_IRQn);
    for (auto* converter : {ADC2, ADC3}) {
        SCOPED_TRACE(converter == ADC2 ? "ADC2" : "ADC3");
        nx_stm32_adc_stream_config_t config{};
        config.adc.channels = channels;
        config.adc.sample_times = samples;
        config.adc.channel_count = 1U;
        config.adc.reference_mv = 3300U;
        config.adc.registers = converter;
        config.regions = &region;
        config.region_count = 1U;
        config.priority = 5U;
        nx_stm32_adc_stream_state_t state{};
        ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_stream_prepare(&state, &config));
        auto* expected = converter == ADC2 ? DMA2_Stream2 : DMA2_Stream1;
        auto irq = converter == ADC2 ? DMA2_Stream2_IRQn : DMA2_Stream1_IRQn;
        EXPECT_EQ(converter, state.adc.registers);
        EXPECT_EQ(expected, state.memory);
        EXPECT_EQ(1U, NVIC_GetEnableIRQ(irq));
        EXPECT_EQ(5U, NVIC_GetPriority(irq));
        EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
        EXPECT_EQ(0U, state.adc.cold_effects);
        EXPECT_EQ(0U, NVIC_GetEnableIRQ(irq));
        EXPECT_EQ(1U, NVIC_GetEnableIRQ(DMA2_Stream4_IRQn));
        EXPECT_EQ(0U, RCC->APB2ENR);
        EXPECT_EQ(0U, RCC->APB1ENR);
    }
    NVIC_DisableIRQ(DMA2_Stream4_IRQn);
}

TEST_F(AdcColdFactory, SecondaryDmaPreservesForeignPendingAndActivePublishers) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_dma_memory_region_t region{0x20000000U, 1024U,
                                        NX_DMA_MEMORY_WRITE};
    for (auto* converter : {ADC2, ADC3}) {
        const auto irq =
            converter == ADC2 ? DMA2_Stream2_IRQn : DMA2_Stream1_IRQn;
        const unsigned index = static_cast<unsigned>(irq);
        const uint32_t bit = 1U << (index % 32U);
        const nx_stm32_adc_stream_config_t config{
            {channels, samples, 1U, 3300U, 0U, converter}, &region, 1U, 5U, 0U};
        for (bool active : {false, true}) {
            SCOPED_TRACE(index);
            SCOPED_TRACE(active);
            auto* facts = active ? g_f4_model_nvic.IABR : g_f4_model_nvic.ISPR;
            facts[index / 32U] = bit;
            nx_stm32_adc_stream_state_t state{};
            EXPECT_EQ(NX_ERROR_BUSY,
                      nx_stm32_adc_stream_prepare(&state, &config));
            EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
            EXPECT_EQ(bit, facts[index / 32U]);
            EXPECT_EQ(0U, RCC->APB2ENR);
            EXPECT_EQ(0U, RCC->APB1ENR);
            facts[index / 32U] = 0U;
        }
    }
}

TEST_F(AdcColdFactory, SecondaryDmaPublishesOnlyItsOwnCompletedDetachedBlock) {
    const uint8_t channels[]{0U};
    const uint8_t timing[]{7U};
    uint16_t data[4]{};
    const nx_dma_memory_region_t region{reinterpret_cast<uintptr_t>(data),
                                        sizeof(data), NX_DMA_MEMORY_WRITE};
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    for (auto* converter : {ADC2, ADC3}) {
        SCOPED_TRACE(converter == ADC2 ? "ADC2" : "ADC3");
        const unsigned shift = converter == ADC2 ? 16U : 6U;
        const unsigned channel = converter == ADC2 ? 1U : 2U;
        const nx_stm32_adc_stream_config_t config{
            {channels, timing, 1U, 3300U, 0U, converter}, &region, 1U, 5U, 0U};
        nx_stm32_adc_stream_state_t state{};
        nx_stream_slot_t slot{};
        slot.data = reinterpret_cast<uint8_t*>(data);
        slot.capacity = sizeof(data);
        nx_stream_t stream{};
        ASSERT_EQ(NX_SUCCESS, nx_stream_initialize(&stream, &slot, 1U));
        ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_stream_prepare(&state, &config));
        const nx_adc_port_t face{&nx_stm32_adc_stream_ops, &state};
        ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_start(&face, &stream, 1000U));
        EXPECT_EQ(channel, (state.memory->CR & DMA_SxCR_CHSEL) >> 25U);
        EXPECT_EQ(4U, state.memory->NDTR);
        now += 3U;
        ASSERT_EQ(NX_SUCCESS, nx_adc_port_stream_service(&face));
        DMA2->HISR = 0x20U;
        DMA2->LISR = 0x20U;
        nx_stm32_adc_stream_irq(&state);
        nx_stream_block_t block{};
        EXPECT_EQ(NX_ERROR_EMPTY, nx_stream_acquire(&stream, &block));
        state.memory->NDTR = 0U;
        DMA2->LISR |= 0x20U << shift;
        nx_stm32_adc_stream_irq(&state);
        EXPECT_EQ(0x3DU << shift, DMA2->LIFCR);
        EXPECT_EQ(0x20U, DMA2->HISR);
        EXPECT_EQ(0U, state.memory->CR & DMA_SxCR_EN);
        EXPECT_EQ(0U, TIM3->CR1 & TIM_CR1_CEN);
        ASSERT_EQ(NX_SUCCESS, nx_stream_acquire(&stream, &block));
        EXPECT_EQ(sizeof(data), block.length);
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_stream_close(&state));
        ASSERT_EQ(NX_SUCCESS, nx_stream_release(&stream, &block));
        ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
    }
}

TEST_F(AdcColdFactory, ClockOffDigitalReadsCannotProveAnalogPowerdown) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_stm32_adc_config_t config{channels, samples, 1U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    gated_foreign = ADC2;
    gated_clock = RCC_APB2ENR_ADC2EN;
    foreign_control = ADC_CR2_ADON;
    EXPECT_EQ(0U, ADC2->CR2); /* The digital interface is still gated. */
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_prepare(&state, &config));
    EXPECT_EQ(0U, ADC->CCR);
    EXPECT_EQ(0U, GPIOA->MODER);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(ADC_CR2_ADON, foreign_control);
}

TEST_F(AdcColdFactory, RefusedObservationClockCannotAuthorizeARetune) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_stm32_adc_config_t config{channels, samples, 1U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    refuse_probe_enable = RCC_APB2ENR_ADC3EN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_adc_prepare(&state, &config));
    EXPECT_EQ(0U, ADC->CCR);
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_FALSE(state.initialized);
    refuse_probe_enable = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(0U, RCC->APB2ENR);
}

TEST_F(AdcColdFactory, RefusedObservationClockReleaseRetainsCleanupForRetry) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_stm32_adc_config_t config{channels, samples, 1U, 3300U, 0U, ADC1};
    nx_stm32_adc_state_t state{};
    refuse_probe_release = RCC_APB2ENR_ADC2EN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_adc_prepare(&state, &config));
    EXPECT_FALSE(state.initialized);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_ADC2EN);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_adc_close(&state));
    EXPECT_NE(0U, state.cold_effects);
    refuse_probe_release = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
    EXPECT_EQ(0U, state.cold_effects);
    EXPECT_EQ(0U, RCC->APB2ENR);
}

TEST_F(AdcColdFactory, UnprivilegedSamplingDoesNotAccessTheConverter) {
    Start();
    const auto* port = nx_factory_adc(static_cast<nx_adc_id_t>(0));
    ASSERT_NE(nullptr, port);
    const uint32_t control1 = ADC1->CR1;
    const uint32_t control2 = ADC1->CR2;
    uint16_t samples[16]{0xBEEFU};
    size_t count = 99U;
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_adc_port_sample(port, samples, 16U, now + 10000U, &count));
    privileged = true;
    EXPECT_EQ(0xBEEFU, samples[0]);
    EXPECT_EQ(0U, count);
    EXPECT_EQ(control1, ADC1->CR1);
    EXPECT_EQ(control2, ADC1->CR2);
}

TEST_F(AdcColdFactory, AdcOnlyFailureDoesNotAuthorizeDmaIrqCleanup) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_stm32_adc_config_t config{channels, samples, 1U, 3300U, 0U, ADC1};
    nx_stm32_adc_stream_state_t state{};
    ADC1->CR2 = ADC_CR2_ADON;
    ASSERT_EQ(NX_ERROR_BUSY, nx_stm32_adc_cold_begin(&state.adc, &config));
    NVIC_EnableIRQ(DMA2_Stream4_IRQn);
    g_f4_model_nvic.ISPR[static_cast<unsigned>(DMA2_Stream4_IRQn) / 32U] |=
        UINT32_C(1) << (static_cast<unsigned>(DMA2_Stream4_IRQn) % 32U);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
    EXPECT_NE(0U, NVIC_GetEnableIRQ(DMA2_Stream4_IRQn));
    EXPECT_NE(0U, NVIC_GetPendingIRQ(DMA2_Stream4_IRQn));
    EXPECT_EQ(ADC_CR2_ADON, ADC1->CR2);
    NVIC_DisableIRQ(DMA2_Stream4_IRQn);
    NVIC_ClearPendingIRQ(DMA2_Stream4_IRQn);
}

TEST_F(AdcColdFactory, DmaInitializationCannotAcquireAnOccupiedIrq) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_dma_memory_region_t region{UINT32_C(0x20000000), 1024U,
                                        NX_DMA_MEMORY_WRITE};
    const nx_stm32_adc_stream_config_t config{
        {channels, samples, 1U, 3300U, 0U, ADC1}, &region, 1U, 5U, 0U};
    nx_stm32_adc_stream_state_t state{};
    NVIC_EnableIRQ(DMA2_Stream4_IRQn);
    g_f4_model_nvic.ISPR[static_cast<unsigned>(DMA2_Stream4_IRQn) / 32U] |=
        UINT32_C(1) << (static_cast<unsigned>(DMA2_Stream4_IRQn) % 32U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_stream_prepare(&state, &config));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
    EXPECT_NE(0U, NVIC_GetEnableIRQ(DMA2_Stream4_IRQn));
    EXPECT_NE(0U, NVIC_GetPendingIRQ(DMA2_Stream4_IRQn));
    NVIC_DisableIRQ(DMA2_Stream4_IRQn);
    NVIC_ClearPendingIRQ(DMA2_Stream4_IRQn);
}

TEST_F(AdcColdFactory, OccupiedTimerIsRejectedBeforeAcquiringAdcEffects) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_dma_memory_region_t region{UINT32_C(0x20000000), 1024U,
                                        NX_DMA_MEMORY_WRITE};
    const nx_stm32_adc_stream_config_t config{
        {channels, samples, 1U, 3300U, 0U, ADC1}, &region, 1U, 5U, 0U};
    nx_stm32_adc_stream_state_t state{};
    RCC->APB1ENR = RCC_APB1ENR_TIM3EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_stream_cold_begin(&state, &config));
    EXPECT_EQ(0U, state.adc.cold_effects);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0U, GPIOA->MODER);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
    EXPECT_EQ(RCC_APB1ENR_TIM3EN, RCC->APB1ENR);
}

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
#if defined(NX_ADC_INTERNAL_CONTRACT)
    if (fractional_clock && enabled_ns == 0U &&
        (ADC1->CR2 & ADC_CR2_ADON) != 0U) {
        /* Enable near a tick boundary; the public clock reports whole us. */
        clock_ns = ((clock_ns + 999U) / 1000U) * 1000U + 950U;
        enabled_ns = clock_ns;
    }
#endif
    if (refuse_restore && (GPIOB->MODER & 3U) == 0U)
        GPIOB->MODER |= 3U;
    if (refuse_powerdown && ADC1->CR2 == 0U)
        ADC1->CR2 = ADC_CR2_ADON;
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{g_f4_model_mask};
    g_f4_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_f4_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
#if defined(NX_ADC_INTERNAL_CONTRACT)
    if (fractional_clock) {
        clock_ns += 10U;
        now = clock_ns / 1000U;
        return now;
    }
#endif
    return ++now;
}
extern "C" void nx_stm32_adc_cold_model_poll(unsigned kind, void* raw) {
    EXPECT_EQ(5U, kind);
    auto* state = static_cast<nx_stm32_adc_state_t*>(raw);
    auto* adc = state->registers;
#if defined(NX_ADC_INTERNAL_CONTRACT)
    if ((adc->CR2 & ADC_CR2_JSWSTART) != 0U) {
        ++injected_launches;
        injected_sequence = adc->JSQR;
        if (fractional_clock && conversion_ns == 0U)
            conversion_ns = clock_ns;
        adc->CR2 &= ~static_cast<uint32_t>(ADC_CR2_JSWSTART);
        if (injected_complete) {
            const unsigned bits = 12U - 2U * state->resolution;
            const uint32_t mask = (1U << bits) - 1U;
            adc->JDR1 = 321U & mask;
            adc->JDR2 = 432U & mask;
            adc->JDR3 = 543U & mask;
            adc->JDR4 = 654U & mask;
            adc->SR |= ADC_SR_JEOC;
        }
    }
#endif
    if ((adc->CR2 & ADC_CR2_SWSTART) != 0U) {
#if defined(NX_ADC_INTERNAL_CONTRACT)
        if (fractional_clock && conversion_ns == 0U)
            conversion_ns = clock_ns;
#endif
        const unsigned bits = 12U - 2U * state->resolution;
        adc->DR = (100U + 100U * (adc->SQR3 & 31U)) & ((1U << bits) - 1U);
        adc->CR2 &= ~static_cast<uint32_t>(ADC_CR2_SWSTART);
        adc->SR |= ADC_SR_EOC;
    }
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    if (reg == &RCC->APB2ENR && gated_foreign != nullptr &&
        (*reg & gated_clock) != 0U)
        foreign_control = gated_foreign->CR2;
    if (reg == &RCC->APB2ENR) {
        value &= ~refuse_probe_enable;
        value |= *reg & refuse_probe_release;
    }
    if (reg != refused)
        *reg = value;
    if (reg == &RCC->APB2ENR && gated_foreign != nullptr)
        gated_foreign->CR2 = (*reg & gated_clock) != 0U ? foreign_control : 0U;
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    ++starts;
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    ++stops;
    return 0;
}

#if defined(NX_ADC_INTERNAL_CONTRACT)
TEST_F(AdcColdFactory, TimerTriggersOwnTheSelectedApbGateAndIdentity) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_dma_memory_region_t region{UINT32_C(0x20000000), 1024U,
                                        NX_DMA_MEMORY_WRITE};
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    TIM_TypeDef* const timers[]{TIM3, TIM1, TIM1, TIM1, TIM3, TIM4,
                                TIM5, TIM5, TIM5, TIM8, TIM8};
    for (uint8_t trigger = 0U; trigger < 11U; ++trigger) {
        SCOPED_TRACE(trigger);
        const bool apb2 = timers[trigger] == TIM1 || timers[trigger] == TIM8;
        const uint32_t gate = timers[trigger] == TIM1   ? RCC_APB2ENR_TIM1EN
                              : timers[trigger] == TIM8 ? RCC_APB2ENR_TIM8EN
                              : timers[trigger] == TIM3 ? RCC_APB1ENR_TIM3EN
                              : timers[trigger] == TIM4 ? RCC_APB1ENR_TIM4EN
                                                        : RCC_APB1ENR_TIM5EN;
        const nx_stm32_adc_stream_config_t config{
            {channels, samples, 1U, 3300U, 0U, ADC1}, &region, 1U, 5U, trigger};
        nx_stm32_adc_stream_state_t state{};
        ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_stream_prepare(&state, &config));
        EXPECT_EQ(timers[trigger], state.timer);
        EXPECT_EQ(trigger, state.trigger);
        EXPECT_EQ(apb2 ? 168000000U : 84000000U, state.timer_clock_hz);
        EXPECT_NE(0U, (apb2 ? RCC->APB2ENR : RCC->APB1ENR) & gate);
        if (timers[trigger] != TIM3) {
            EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_TIM3EN);
        }
        EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
        EXPECT_EQ(0U, state.adc.cold_effects);
        EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
    }
}

TEST_F(AdcColdFactory, InvalidOrForeignTriggerHasNoNewResourceEffects) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_dma_memory_region_t region{UINT32_C(0x20000000), 1024U,
                                        NX_DMA_MEMORY_WRITE};
    nx_stm32_adc_stream_config_t config{
        {channels, samples, 1U, 3300U, 0U, ADC1}, &region, 1U, 5U, UINT8_MAX};
    nx_stm32_adc_stream_state_t state{};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_adc_stream_prepare(&state, &config));
    EXPECT_EQ(0U, state.adc.cold_effects);
    EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
    config.trigger = NX_STM32_ADC_TIM8_TRGO;
    RCC->APB2ENR = RCC_APB2ENR_TIM8EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_stream_prepare(&state, &config));
    EXPECT_EQ(0U, state.adc.cold_effects);
    EXPECT_EQ(RCC_APB2ENR_TIM8EN, RCC->APB2ENR);
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
    RCC->APB2ENR = 0U;
}
#endif

#if defined(NX_ADC_INTERNAL_CONTRACT)
TEST_F(AdcColdFactory, SelectedTimerReleaseFailureRetainsItsOriginalIdentity) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_dma_memory_region_t region{UINT32_C(0x20000000), 1024U,
                                        NX_DMA_MEMORY_WRITE};
    nx_stm32_adc_stream_config_t config{
        {channels, samples, 1U, 3300U, 0U, ADC1},
        &region,
        1U,
        5U,
        NX_STM32_ADC_TIM8_TRGO};
    nx_stm32_adc_stream_state_t state{};
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    RCC->APB1ENR = RCC_APB1ENR_TIM3EN;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_adc_stream_prepare(&state, &config));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_adc_close(&state.adc));
    refuse_probe_release = RCC_APB2ENR_TIM8EN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_adc_stream_close(&state));
    EXPECT_EQ(NX_STM32_ADC_TIMER, state.adc.cold_effects);
    EXPECT_EQ(RCC_APB2ENR_TIM8EN, RCC->APB2ENR);
    config.trigger = NX_STM32_ADC_TIM4_CH4;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_stream_prepare(&state, &config));
    EXPECT_EQ(TIM8, state.timer);
    EXPECT_EQ(NX_STM32_ADC_TIM8_TRGO, state.trigger);
    refuse_probe_release = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
    EXPECT_EQ(0U, state.adc.cold_effects);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(RCC_APB1ENR_TIM3EN, RCC->APB1ENR);
    RCC->APB1ENR = 0U;
}

TEST_F(AdcColdFactory, FailedTimerGateAcquireKeepsItsExactCleanupReceipt) {
    const uint8_t channels[]{0U};
    const uint8_t samples[]{7U};
    const nx_dma_memory_region_t region{UINT32_C(0x20000000), 1024U,
                                        NX_DMA_MEMORY_WRITE};
    const nx_stm32_adc_stream_config_t config{
        {channels, samples, 1U, 3300U, 0U, ADC1},
        &region,
        1U,
        5U,
        NX_STM32_ADC_TIM8_CH1};
    nx_stm32_adc_stream_state_t state{};
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    refuse_probe_enable = RCC_APB2ENR_TIM8EN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_adc_stream_prepare(&state, &config));
    EXPECT_NE(0U, state.adc.cold_effects & NX_STM32_ADC_TIMER);
    EXPECT_EQ(TIM8, state.timer);
    EXPECT_EQ(NX_STM32_ADC_TIM8_CH1, state.trigger);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_TIM8EN);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_stream_close(&state));
    EXPECT_EQ(0U, state.adc.cold_effects);
    EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
    refuse_probe_enable = 0U;
}
#endif

#include <unistd.h>
#if defined(NX_ADC_INTERNAL_CONTRACT)
TEST_F(AdcColdFactory, InjectedWarmupExhaustionCannotWaitOnAnInfiniteSentinel) {
    EXPECT_EXIT(
        ([&] {
            (void)alarm(2U);
            const uint8_t channels[]{0U};
            const uint8_t timing[]{7U};
            const nx_stm32_adc_config_t config{channels, timing, 1U,
                                               3300U,    0U,     ADC1};
            nx_stm32_adc_state_t state{};
            if (nx_stm32_adc_injected_prepare(&state, &config) != NX_SUCCESS)
                _exit(2);
            const nx_adc_port_t face{&nx_stm32_adc_injected_ops, &state};
            now = NX_DEADLINE_NEVER - 4U;
            uint16_t output = 0xBEEFU;
            size_t count{};
            const nx_result_t result = nx_adc_port_sample(
                &face, &output, 1U, NX_DEADLINE_NEVER - 1U, &count);
            const bool correct = result == NX_ERROR_EXHAUSTED && count == 0U &&
                                 output == 0xBEEFU && !state.active &&
                                 ADC1->CR2 == 0U && injected_launches == 0U;
            _exit(correct ? 0 : 1);
        }()),
        testing::ExitedWithCode(0), "");
}
#endif
