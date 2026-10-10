/**
 * \file            stm32_adc_stream_test.cpp
 *
 * \brief           Timer DMA scan-rate, detached publication and block
 *                  backpressure
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_stream.h"
}
#include "nexus/io/native/model.h"
#include <gtest/gtest.h>

#undef nx_arch_dsb
static ADC_TypeDef* s_power_adc;
static bool s_delay_power_store;

/** \brief Complete the modeled APB power store before the warmup timestamp. */
extern "C" void nx_stm32_adc_model_barrier(void) {
    nx_arch_dsb();
    if (s_delay_power_store && s_power_adc != nullptr &&
        (s_power_adc->CR2 & ADC_CR2_ADON) != 0U) {
        (void)nx_native_clock_advance(5U);
        s_delay_power_store = false;
    }
}

static bool s_hold_dma_enable;

/** \brief Retain EN to model a memory engine without a completed drain proof.
 */
extern "C" void nx_stm32_dma_model_disable(void* raw, uint32_t mask) {
    auto* stream = static_cast<DMA_Stream_TypeDef*>(raw);
    if (s_hold_dma_enable) {
        mask &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    }
    stream->CR &= ~mask;
}

class Stm32AdcStream : public testing::Test {
  protected:
    ADC_TypeDef adc = {};
    ADC_Common_TypeDef common = {};
    TIM_TypeDef timer = {};
    DMA_TypeDef dma = {};
    DMA_Stream_TypeDef memory = {};
    uint8_t channels[2] = {0U, 1U};
    uint8_t sample_times[2] = {7U, 7U};
    uint16_t samples[4] = {};
    nx_dma_memory_region_t region = {};
    nx_stm32_adc_stream_state_t state = {};
    nx_adc_port_t port = {&nx_stm32_adc_stream_ops, &state};
    nx_stream_slot_t slot = {};
    nx_stream_t stream = {};

    void SetUp() override {
        s_hold_dma_enable = false;
        s_delay_power_store = false;
        s_power_adc = &adc;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        state.adc.registers = &adc;
        state.adc.common = &common;
        state.adc.channels = channels;
        state.adc.sample_times = sample_times;
        state.adc.channel_count = 2U;
        state.adc.reference_mv = 3300U;
        state.timer = &timer;
        state.dma = &dma;
        state.memory = &memory;
        state.timer_clock_hz = 84000000U;
        state.adc_clock_hz = 21000000U;
        region = {reinterpret_cast<uintptr_t>(samples), sizeof(samples),
                  NX_DMA_MEMORY_WRITE};
        state.regions = &region;
        state.region_count = 1U;
        slot.data = reinterpret_cast<uint8_t*>(samples);
        slot.capacity = sizeof(samples);
        ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_adc_stream_initialize(&state), NX_SUCCESS);
    }
};

TEST_F(Stm32AdcStream, StabilizationRequiresExplicitServiceBeforeFirstTrigger) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U), NX_SUCCESS);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(adc.CR2 & ADC_CR2_EXTEN, 0U);
    EXPECT_NE(adc.CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_clock_advance(2U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_EQ(nx_native_clock_advance(1U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_SUCCESS);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcStream, ValidatesCompleteScanConversionBudgetBeforeAdmission) {
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 50000U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(state.stream, nullptr);
}

TEST_F(Stm32AdcStream,
       PublishesOnlyAfterDrainAndRequiresExplicitFreeBlockResume) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(3U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_stream_service(&port), NX_SUCCESS);
    EXPECT_EQ(memory.NDTR, 4U);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    samples[0] = 100U;
    samples[1] = 200U;
    samples[2] = 101U;
    samples[3] = 201U;
    memory.NDTR = 0U;
    dma.HISR = 1U << 5U;
    nx_stm32_adc_stream_irq(&state);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(adc.CR2 & (ADC_CR2_ADON | ADC_CR2_EXTEN | ADC_CR2_DMA), 0U);
    nx_stream_block_t block = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.length, sizeof(samples));
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_TRIGGER, 0U);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(samples[0], 100U);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_EQ(nx_native_clock_advance(3U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_SUCCESS);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcStream, RefusedDisableNeverPublishesMutableDmaMemory) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(3U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_stream_service(&port), NX_SUCCESS);
    s_hold_dma_enable = true;
    memory.NDTR = 0U;
    dma.HISR = 1U << 5U;
    nx_stm32_adc_stream_irq(&state);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(state.stream, &stream);
    s_hold_dma_enable = false;
    memory.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
}

TEST_F(Stm32AdcStream, MisalignedOrPartialScanBlocksRejectWithoutTimerEffects) {
    slot.capacity = 3U;
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32AdcStream, WarmupStartsAfterPowerStoreBarrierCompletes) {
    s_delay_power_store = true;
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U), NX_SUCCESS);
    EXPECT_FALSE(s_delay_power_store);
    EXPECT_EQ(nx_time_now_us(), 105U);
    EXPECT_EQ(state.ready_at, 108U);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_EQ(nx_native_clock_advance(2U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_clock_advance(1U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcStream, ObservedDmaFaultCannotBeReplacedByLateTransferComplete) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(3U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_stream_service(&port), NX_SUCCESS);
    s_hold_dma_enable = true;
    memory.NDTR = 1U;
    dma.HISR = 1U << 3U;
    nx_stm32_adc_stream_irq(&state);
    ASSERT_TRUE(state.fault);
    ASSERT_EQ(slot.state, NX_STREAM_SLOT_FILLING);
    s_hold_dma_enable = false;
    memory.NDTR = 0U;
    dma.HISR = 1U << 5U;
    nx_stm32_adc_stream_irq(&state);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_IO);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcStream,
       BasicAdcInitializationCannotBypassStreamClockValidation) {
    state.adc.initialized = false;
    state.timer_clock_hz = 0U;
    ASSERT_EQ(nx_stm32_adc_initialize(&state.adc), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(state.stream, nullptr);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
}

TEST_F(Stm32AdcStream, InvalidStreamClockBoundsRejectBeforeAdmission) {
    state.timer_clock_hz = UINT32_MAX;
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U),
              NX_ERROR_INVALID);
    state.timer_clock_hz = 84000000U;
    state.adc_clock_hz = 0U;
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U),
              NX_ERROR_INVALID);
    state.adc_clock_hz = UINT32_MAX;
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(state.stream, nullptr);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
}
