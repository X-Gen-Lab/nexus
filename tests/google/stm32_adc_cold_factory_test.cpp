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
            {channels, samples, 1U, 3300U, 0U, converter}, &region, 1U, 5U};
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
            {channels, timing, 1U, 3300U, 0U, converter}, &region, 1U, 5U};
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
        {channels, samples, 1U, 3300U, 0U, ADC1}, &region, 1U, 5U};
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
        {channels, samples, 1U, 3300U, 0U, ADC1}, &region, 1U, 5U};
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
    return ++now;
}
extern "C" void nx_stm32_adc_cold_model_poll(unsigned kind, void* raw) {
    EXPECT_EQ(5U, kind);
    auto* state = static_cast<nx_stm32_adc_state_t*>(raw);
    auto* adc = state->registers;
    if ((adc->CR2 & ADC_CR2_SWSTART) != 0U) {
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
