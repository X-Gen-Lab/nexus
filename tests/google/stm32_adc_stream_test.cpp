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

/** \brief Model EOC at the last programmed regular rank, without repairing it.
 */
extern "C" void nx_stm32_adc_model_poll(unsigned kind, void* raw) {
    EXPECT_EQ(kind, 5U);
    auto* state = static_cast<nx_stm32_adc_state_t*>(raw);
    ADC_TypeDef* adc = state->registers;
    (void)nx_native_clock_advance(1U);
    if ((adc->CR2 & ADC_CR2_SWSTART) != 0U) {
        unsigned rank =
            (adc->CR1 & ADC_CR1_SCAN) != 0U ? (adc->SQR1 >> 20U) & 15U : 0U;
        uint32_t sequence = rank < 6U    ? adc->SQR3
                            : rank < 12U ? adc->SQR2
                                         : adc->SQR1;
        unsigned shift = rank < 6U    ? rank * 5U
                         : rank < 12U ? (rank - 6U) * 5U
                                      : (rank - 12U) * 5U;
        adc->DR = 100U + ((sequence >> shift) & 31U) * 100U;
        adc->CR2 &= ~static_cast<uint32_t>(ADC_CR2_SWSTART);
        adc->SR |= ADC_SR_EOC;
    }
}

/** \brief Retain EN to model a memory engine without a completed drain proof.
 */
extern "C" void nx_stm32_dma_model_disable(void* raw, uint32_t mask) {
    auto* stream = static_cast<DMA_Stream_TypeDef*>(raw);
    if (s_hold_dma_enable) {
        mask &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    }
    stream->CR &= ~mask;
}

static bool s_refuse_target;
static bool s_flip_target;
static bool s_wrong_target;
static bool s_error_target;
static bool s_error_ack;
static DMA_TypeDef* s_target_dma;
/** \brief Model only W1C status, including facts arriving after
 * acknowledgement.
 */
extern "C" void nx_stm32_adc_model_ack(void* raw, uint32_t flags) {
    auto* dma = static_cast<DMA_TypeDef*>(raw);
    if (s_error_ack) {
        dma->HISR |= 1U << 3U;
    }
    dma->HIFCR = flags;
    dma->HISR &= ~flags;
}
/** \brief Refuse a write or advance CT at the real hardware store boundary. */
extern "C" void nx_stm32_adc_model_target(void* raw, unsigned bank,
                                          uint32_t address) {
    auto* memory = static_cast<DMA_Stream_TypeDef*>(raw);
    if (s_wrong_target) {
        address += 4U;
    }
    if (!s_refuse_target) {
        if (bank == 0U) {
            memory->M0AR = address;
        } else {
            memory->M1AR = address;
        }
    }
    if (s_flip_target) {
        memory->CR ^= DMA_SxCR_CT;
    }
    if (s_error_target && s_target_dma != nullptr) {
        s_target_dma->HISR |= 1U << 3U;
    }
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
        s_refuse_target = false;
        s_flip_target = false;
        s_wrong_target = false;
        s_error_target = false;
        s_error_ack = false;
        s_target_dma = &dma;
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

TEST_F(Stm32AdcStream, StoppedMultiChannelStreamRestoresIndependentPollShots) {
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U), NX_SUCCESS);
    ASSERT_NE(adc.CR1 & ADC_CR1_SCAN, 0U);
    ASSERT_NE(adc.SQR1 & ADC_SQR1_L, 0U);
    ASSERT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
    uint16_t shot[2] = {};
    size_t count = 99U;
    ASSERT_EQ(nx_adc_port_sample(&port, shot, 2U, 200U, &count), NX_SUCCESS);
    EXPECT_EQ(count, 2U);
    EXPECT_EQ(shot[0], 100U);
    EXPECT_EQ(shot[1], 200U);
    EXPECT_EQ(adc.CR1 & ADC_CR1_SCAN, 0U);
    EXPECT_EQ(adc.SQR1 & ADC_SQR1_L, 0U);
    EXPECT_EQ(adc.CR2, 0U);
    EXPECT_FALSE(state.adc.active);
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U), NX_SUCCESS);
    EXPECT_NE(adc.CR1 & ADC_CR1_SCAN, 0U);
    EXPECT_EQ(adc.SQR1 & ADC_SQR1_L, 1U << 20U);
    EXPECT_EQ(adc.SQR3, 1U << 5U);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

class Stm32AdcContinuous : public testing::Test {
  protected:
    ADC_TypeDef adc = {};
    ADC_Common_TypeDef common = {};
    TIM_TypeDef timer = {};
    DMA_TypeDef dma = {};
    DMA_Stream_TypeDef memory = {};
    uint8_t channels[2] = {0U, 1U};
    uint8_t sample_times[2] = {1U, 1U};
    uint16_t samples[3][4] = {};
    nx_dma_memory_region_t regions[3] = {};
    nx_stm32_adc_continuous_state_t state = {};
    nx_adc_port_t port = {&nx_stm32_adc_continuous_ops, &state};
    nx_stream_slot_t slots[3] = {};
    nx_stream_t stream = {};

    void SetUp() override {
        s_hold_dma_enable = false;
        s_refuse_target = false;
        s_flip_target = false;
        s_wrong_target = false;
        s_error_target = false;
        s_error_ack = false;
        s_target_dma = &dma;
        s_delay_power_store = false;
        s_power_adc = &adc;
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        state.base.adc.registers = &adc;
        state.base.adc.common = &common;
        state.base.adc.channels = channels;
        state.base.adc.sample_times = sample_times;
        state.base.adc.channel_count = 2U;
        state.base.adc.reference_mv = 3300U;
        state.base.timer = &timer;
        state.base.dma = &dma;
        state.base.memory = &memory;
        state.base.timer_clock_hz = 84000000U;
        state.base.adc_clock_hz = 21000000U;
        for (size_t i = 0; i < 3; ++i) {
            regions[i] = {reinterpret_cast<uintptr_t>(samples[i]),
                          sizeof(samples[i]), NX_DMA_MEMORY_WRITE};
            slots[i].data = reinterpret_cast<uint8_t*>(samples[i]);
            slots[i].capacity = sizeof(samples[i]);
        }
        state.base.regions = regions;
        state.base.region_count = 3U;
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 3U), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_adc_continuous_initialize(&state), NX_SUCCESS);
    }
    void Ready() {
        ASSERT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U), NX_SUCCESS);
        ASSERT_EQ(nx_native_clock_advance(3U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_port_stream_service(&port), NX_SUCCESS);
    }
    void Complete(unsigned bank) {
        memory.CR = (memory.CR & ~static_cast<uint32_t>(DMA_SxCR_CT)) |
                    (bank == 0U ? DMA_SxCR_CT : 0U);
        memory.NDTR = 4U;
        dma.HISR = 1U << 5U;
        nx_stm32_adc_continuous_irq(&state);
    }
};

TEST_F(Stm32AdcContinuous, TwoPrepostedTargetsStayInvisibleUntilSwapped) {
    Ready();
    EXPECT_NE(memory.CR & DMA_SxCR_DBM, 0U);
    EXPECT_EQ(stream.outstanding, 2U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    Complete(0U);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(memory.M0AR,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(samples[2])));
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, reinterpret_cast<uint8_t*>(samples[0]));
    EXPECT_EQ(block.length, sizeof(samples[0]));
    EXPECT_EQ(memory.M0AR == static_cast<uint32_t>(
                                 reinterpret_cast<uintptr_t>(block.data)) ||
                  memory.M1AR == static_cast<uint32_t>(
                                     reinterpret_cast<uintptr_t>(block.data)),
              false);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    Complete(1U);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, reinterpret_cast<uint8_t*>(samples[1]));
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcContinuous, HeldConsumerForcesStopAndExplicitWarmResume) {
    Ready();
    Complete(0U);
    nx_stream_block_t held = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &held), NX_SUCCESS);
    Complete(1U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(stream.losses, 1U);
    ASSERT_EQ(nx_stream_release(&stream, &held), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_clock_advance(3U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_SUCCESS);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    nx_stream_block_t completed = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &completed), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcContinuous, FailedDrainRetainsBothPrepostedLoans) {
    Ready();
    s_hold_dma_enable = true;
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(stream.outstanding, 2U);
    EXPECT_EQ(state.posted.pending, 2U);
    EXPECT_EQ(slots[0].state, NX_STREAM_SLOT_FILLING);
    EXPECT_EQ(slots[1].state, NX_STREAM_SLOT_FILLING);
    s_hold_dma_enable = false;
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
    EXPECT_EQ(stream.outstanding, 0U);
}

TEST_F(Stm32AdcContinuous, WholeScanEqualCapacityAndDomainsValidateBeforeLoan) {
    slots[2].capacity = 4U;
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(stream.epoch, 0U);
    slots[2].capacity = sizeof(samples[2]);
    regions[2].permissions = NX_DMA_MEMORY_READ;
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(stream.epoch, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32AdcContinuous, WrongCompletionPhaseNeverPublishesUnprovenMemory) {
    Ready();
    Complete(1U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcContinuous, RefusedTargetWriteRetainsSpareUntilProvedDrain) {
    Ready();
    s_refuse_target = true;
    s_hold_dma_enable = true;
    Complete(0U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(state.posted.pending, 3U);
    EXPECT_EQ(stream.outstanding, 3U);
    EXPECT_TRUE(state.base.filling);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_BUSY);
    s_hold_dma_enable = false;
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_IO);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcContinuous, BankFlipDuringAddressWriteNeverPublishesOldTarget) {
    Ready();
    s_flip_target = true;
    Complete(0U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcContinuous, SequenceFactsAreColdAndDescribeCompleteScanCost) {
    nx_adc_sequence_info_t facts = {};
    ASSERT_EQ(nx_adc_port_sequence_info(&port, &facts), NX_SUCCESS);
    EXPECT_EQ(facts.channels, channels);
    EXPECT_EQ(facts.sample_times, sample_times);
    EXPECT_EQ(facts.channel_count, 2U);
    EXPECT_EQ(facts.adc_clock_hz, 21000000U);
    EXPECT_EQ(facts.timer_clock_hz, 84000000U);
    EXPECT_EQ(facts.scan_cycles, 54U);
    EXPECT_EQ(facts.conversion_cycles, 12U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(stream.epoch, 0U);
}

TEST_F(Stm32AdcContinuous, ObserverUsesLoanEpochAndSoftwareObservationClock) {
    nx_adc_observation_t records[3] = {};
    nx_adc_observer_t observer = {};
    ASSERT_EQ(nx_adc_observer_initialize(&observer, records, 3U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_attach_observer(&port, &observer), NX_SUCCESS);
    Ready();
    ASSERT_EQ(nx_native_clock_advance(7U), NX_SUCCESS);
    Complete(0U);
    nx_stream_block_t first = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &first), NX_SUCCESS);
    nx_adc_observation_t observed = {};
    ASSERT_EQ(nx_adc_port_block_observation(&port, &first, &observed),
              NX_SUCCESS);
    EXPECT_EQ(observed.epoch, first.epoch);
    EXPECT_EQ(observed.sequence, 1U);
    EXPECT_EQ(observed.observed_at, 110U);
    EXPECT_NE(observed.flags & NX_ADC_OBSERVATION_DISCONTINUITY, 0U);
    EXPECT_EQ(nx_adc_port_attach_observer(&port, nullptr), NX_ERROR_BUSY);
    ASSERT_EQ(nx_stream_release(&stream, &first), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_block_observation(&port, &first, &observed),
              NX_ERROR_STATE);
    Complete(1U);
    nx_stream_block_t second = {};
    ASSERT_EQ(nx_stream_acquire(&stream, &second), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_block_observation(&port, &second, &observed),
              NX_SUCCESS);
    EXPECT_EQ(observed.sequence, 2U);
    EXPECT_EQ(observed.flags & NX_ADC_OBSERVATION_DISCONTINUITY, 0U);
    ASSERT_EQ(nx_stream_release(&stream, &second), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
    EXPECT_EQ(observer.stream, nullptr);
    EXPECT_EQ(nx_adc_port_attach_observer(&port, nullptr), NX_SUCCESS);
}

TEST_F(Stm32AdcContinuous, ObserverCountAndPayloadAliasRejectBeforeDmaLoan) {
    nx_adc_observation_t records[2] = {};
    nx_adc_observer_t observer = {};
    ASSERT_EQ(nx_adc_observer_initialize(&observer, records, 2U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_attach_observer(&port, &observer), NX_SUCCESS);
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(stream.epoch, 0U);
    EXPECT_EQ(observer.stream, nullptr);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
}

TEST_F(Stm32AdcContinuous, ObserverSequenceNeverWrapsOrPublishesMissingRecord) {
    nx_adc_observation_t records[3] = {};
    nx_adc_observer_t observer = {};
    ASSERT_EQ(nx_adc_observer_initialize(&observer, records, 3U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_port_attach_observer(&port, &observer), NX_SUCCESS);
    Ready();
    observer.sequence = UINT64_MAX;
    Complete(0U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(observer.sequence, UINT64_MAX);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcContinuous, WrongAddressReadbackMustNotReleasePreviousTarget) {
    Ready();
    s_wrong_target = true;
    Complete(0U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcContinuous, ErrorArrivingDuringTargetWriteRejectsPublication) {
    Ready();
    s_error_target = true;
    Complete(0U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcContinuous, NewErrorAtCompletionAcknowledgementCannotDisappear) {
    Ready();
    s_error_ack = true;
    Complete(0U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}

TEST_F(Stm32AdcContinuous,
       ExhaustedEpochRejectsBeforePartialDoubleTargetAdmission) {
    stream.epoch = UINT64_MAX - 1U;
    EXPECT_EQ(nx_adc_port_stream_start(&port, &stream, 1000U),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(state.posted.pending, 0U);
    EXPECT_EQ(state.base.stream, nullptr);
}

TEST_F(Stm32AdcContinuous,
       EpochExhaustionAtReplacementHaltsWithoutConsumerOverwrite) {
    Ready();
    stream.epoch = UINT64_MAX;
    Complete(0U);
    nx_stream_block_t block = {};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_adc_port_stream_service(&port), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(nx_adc_port_stream_stop(&port), NX_SUCCESS);
}
