/**
 * \file            stm32_adc_group_test.cpp
 * \brief           Common-ADC CDR, injected order and final-access contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_adc_group.h"
#include "stm32f407_clock.h"
}
#include "nexus/io/native/model.h"
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" {
nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
}

#undef nx_arch_dsb
static bool s_hold_dma;
static bool s_hold_address;
static bool s_refuse_clock;
static bool s_late_error;
static bool s_refuse_adc;
static bool s_refuse_release;
static bool s_hidden_owner;
static bool s_hidden_common;
static bool s_refuse_pad;
static bool s_late_adc;
static bool s_lose_domain_on_data_read;
static nx_stm32_adc_group_config_t* s_hardware;
static bool s_blocks_refuse_target;
static bool s_blocks_flip_target;
static bool s_blocks_lose_clock;
static bool s_blocks_jump_start;
static bool s_blocks_sticky_at_target;
static bool s_blocks_refuse_ack;
static bool s_combined_expire_on_enable;
static bool s_combined_watch_start;
static bool s_combined_saw_start;
static bool s_blocks_live_irq_after_gate;
static unsigned s_blocks_refused_acks;
static size_t s_blocks_loans_at_gate;
static nx_stream_t* s_blocks_stream;
static TIM_TypeDef* s_injected_timer_model;
static RCC_TypeDef* s_injected_rcc_model;
static bool s_injected_counter_seen;
static bool s_injected_clock_seen;
static bool s_injected_refuse_stop;
static bool s_injected_refuse_clock_release;
static bool s_automatic_expire_on_arm;
static bool s_automatic_watch_late_source;
static bool s_automatic_saw_late_source;
static bool s_automatic_refuse_jeoc_clear;
static bool s_interleaved_jump_start;
static bool s_interleaved_refuse_width;
static bool s_interleaved_refuse_direct;
static bool s_interleaved_saw_refused_source;
extern "C" void nx_stm32_dma_model_disable(void* raw, uint32_t mask) {
    auto* memory = static_cast<DMA_Stream_TypeDef*>(raw);
    memory->CR &= ~(mask & ~(s_hold_dma ? uint32_t(DMA_SxCR_EN) : 0U));
}
extern "C" void nx_stm32_adc_group_model_barrier(void) {
    nx_arch_dsb();
    if (s_injected_timer_model != nullptr) {
        s_injected_counter_seen |=
            (s_injected_timer_model->CR1 & TIM_CR1_CEN) != 0U;
        s_injected_clock_seen |=
            (s_injected_rcc_model->APB1ENR & RCC_APB1ENR_TIM4EN) != 0U;
        if (s_injected_refuse_stop && s_injected_counter_seen)
            s_injected_timer_model->CR1 |= TIM_CR1_CEN;
        if (s_injected_refuse_clock_release && s_injected_clock_seen)
            s_injected_rcc_model->APB1ENR |= RCC_APB1ENR_TIM4EN;
    }
    if (s_hardware == nullptr)
        return;
    if ((s_interleaved_refuse_width || s_interleaved_refuse_direct) &&
        (s_hardware->memory->CR & DMA_SxCR_DBM) != 0U) {
        if (s_interleaved_refuse_width)
            s_hardware->memory->CR =
                (s_hardware->memory->CR & ~uint32_t(DMA_SxCR_MSIZE)) |
                DMA_SxCR_MSIZE_1;
        if (s_interleaved_refuse_direct)
            s_hardware->memory->FCR |= DMA_SxFCR_DMDIS;
        s_interleaved_saw_refused_source |=
            (s_hardware->adc[0]->CR2 & ADC_CR2_SWSTART) != 0U;
    }
    if (s_interleaved_jump_start &&
        (s_hardware->adc[0]->CR2 & ADC_CR2_SWSTART) != 0U) {
        s_interleaved_jump_start = false;
        (void)nx_native_clock_advance(5000U);
        s_hardware->memory->CR |= DMA_SxCR_CT;
        s_hardware->dma->LISR |= DMA_LISR_TCIF0;
    }
    if (s_automatic_refuse_jeoc_clear &&
        (s_hardware->adc[0]->CR2 & ADC_CR2_ADON) == 0U)
        for (auto* converter : s_hardware->adc)
            converter->SR |= ADC_SR_JEOC;
    if (s_automatic_expire_on_arm &&
        (s_hardware->memory->CR & DMA_SxCR_EN) != 0U &&
        (s_hardware->adc[0]->CR2 & ADC_CR2_EXTEN) == 0U) {
        s_automatic_expire_on_arm = false;
        (void)nx_native_clock_advance(100U);
        s_automatic_watch_late_source = true;
    }
    if (s_automatic_watch_late_source &&
        (s_hardware->timer->CR1 & TIM_CR1_CEN) != 0U)
        s_automatic_saw_late_source = true;
    if (s_combined_expire_on_enable &&
        (s_hardware->adc[0]->CR1 & ADC_CR1_JEOCIE) != 0U &&
        (s_hardware->adc[0]->CR2 & ADC_CR2_JSWSTART) == 0U) {
        s_combined_expire_on_enable = false;
        (void)nx_native_clock_advance(100U);
    }
    if (s_combined_watch_start &&
        (s_hardware->adc[0]->CR2 & ADC_CR2_JSWSTART) != 0U)
        s_combined_saw_start = true;
    if (s_blocks_jump_start && s_hardware->timer != nullptr &&
        (s_hardware->timer->CR1 & TIM_CR1_CEN) != 0U) {
        s_blocks_jump_start = false;
        (void)nx_native_clock_advance(24100U);
        s_hardware->memory->CR |= DMA_SxCR_CT;
        s_hardware->dma->LISR |= DMA_LISR_TCIF0;
    }
    if (s_refuse_clock)
        s_hardware->rcc->APB2ENR &= ~uint32_t(RCC_APB2ENR_ADC2EN);
    if (s_hold_address && s_hardware->memory->M0AR == 0U)
        s_hardware->memory->M0AR = 4U;
    if (s_refuse_adc)
        s_hardware->adc[1]->CR2 &= ~uint32_t(ADC_CR2_ADON);
    if (s_refuse_release && s_hardware->rcc->APB2ENR == 0U)
        s_hardware->rcc->APB2ENR |= RCC_APB2ENR_ADC1EN;
    if (s_hidden_owner &&
        (s_hardware->rcc->APB2ENR & RCC_APB2ENR_ADC3EN) != 0U) {
        s_hardware->adc[2]->CR2 |= ADC_CR2_ADON;
        s_hidden_owner = false;
    }
    if (s_hidden_common &&
        (s_hardware->rcc->APB2ENR & RCC_APB2ENR_ADC1EN) != 0U) {
        s_hardware->common->CCR = ADC_CCR_ADCPRE_1;
        s_hidden_common = false;
    }
    if (s_refuse_pad)
        GPIOA->MODER &= ~uint32_t(3U);
    if (s_late_adc && (s_hardware->adc[0]->CR2 & ADC_CR2_ADON) == 0U) {
        s_hardware->adc[1]->SR |= ADC_SR_OVR;
        s_late_adc = false;
    }
    if (s_blocks_refuse_ack && (s_hardware->adc[0]->CR2 & ADC_CR2_ADON) == 0U &&
        (s_hardware->dma->LISR & DMA_LISR_TEIF0) != 0U) {
        /* Trace the actual gate chain before Thread drain can make progress.
         * A masked CPU does not remove this level when interrupts restore. */
        s_blocks_live_irq_after_gate |=
            (s_hardware->memory->CR & DMA_SxCR_TEIE) != 0U;
        if (s_blocks_stream != nullptr)
            s_blocks_loans_at_gate = s_blocks_stream->outstanding;
    }
}
extern "C" void nx_stm32_adc_group_model_ack(void* raw, uint32_t flags) {
    auto* dma = static_cast<DMA_TypeDef*>(raw);
    dma->LIFCR = flags;
    if (!s_blocks_refuse_ack)
        dma->LISR &= ~flags;
    else if ((flags & DMA_LISR_TEIF0) != 0U)
        ++s_blocks_refused_acks;
    if (s_late_error) {
        dma->LISR |= DMA_LISR_TEIF0;
        s_late_error = false;
    }
}

/** \brief A hardware JDR read can outlive the previously checked domain. */
extern "C" uint32_t
nx_stm32_adc_group_model_read(const volatile uint32_t* address, uint32_t mask) {
    uint32_t value = *address & mask;
    if (s_lose_domain_on_data_read && s_hardware != nullptr &&
        address == &s_hardware->adc[1]->JDR2) {
        s_lose_domain_on_data_read = false;
        g_nx_stm32_system.timer_error = NX_ERROR_IO;
    }
    return value;
}

class Stm32AdcGroup : public testing::Test {
  protected:
    ADC_TypeDef* adc = nullptr;
    void* gpio_mmio = MAP_FAILED;
    ADC_Common_TypeDef common{};
    TIM_TypeDef timer{};
    DMA_TypeDef dma{};
    DMA_Stream_TypeDef memory{};
    RCC_TypeDef rcc{};
    uint8_t channels[12]{0U, 1U, 2U, 3U};
    uint8_t samples[12]{7U, 7U, 7U, 7U};
    uint8_t converters[12]{1U, 2U, 1U, 2U};
    alignas(nx_adc_group_info_t) uint16_t frames[128]{};
    nx_dma_memory_region_t region{};
    nx_stm32_adc_group_config_t config{};
    nx_stm32_adc_group_state_t state{};
    nx_adc_group_request_t request{};
    nx_adc_group_port_t port{&nx_stm32_adc_group_ops, &state};

    void SetUp() override {
        gpio_mmio =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40020000)), 0x3000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(gpio_mmio, MAP_FAILED);
        /* The locked LL register-offset helpers intentionally use the ARM
         * 32-bit address ABI. Give the unchanged helper real low addresses. */
        void* registers = mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
        ASSERT_NE(registers, MAP_FAILED);
        adc = static_cast<ADC_TypeDef*>(registers);
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        s_automatic_expire_on_arm = s_automatic_watch_late_source = false;
        s_automatic_saw_late_source = false;
        s_automatic_refuse_jeoc_clear = false;
        s_interleaved_jump_start = false;
        s_interleaved_refuse_width = s_interleaved_refuse_direct = false;
        s_interleaved_saw_refused_source = false;
        s_hold_dma = s_hold_address = s_refuse_clock = s_late_error = false;
        s_refuse_adc = false;
        s_refuse_release = s_hidden_owner = s_hidden_common = false;
        s_refuse_pad = s_late_adc = false;
        s_lose_domain_on_data_read = false;
        g_nx_stm32_gpioa_model = {};
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &rcc;
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.core_hz = NEXUS_CORE_HZ;
        g_nx_stm32_system.time_epoch = 1U;
        rcc.CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        rcc.PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        rcc.CFGR = NEXUS_CLOCK_CFGR | RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
        rcc.AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                      RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIOFEN;
        region = {reinterpret_cast<uintptr_t>(frames), sizeof(frames),
                  NX_DMA_MEMORY_WRITE};
        for (unsigned i = 0; i < 3; ++i)
            config.adc[i] = &adc[i];
        config.common = &common;
        config.timer = &timer;
        config.dma = &dma;
        config.memory = &memory;
        config.rcc = &rcc;
        config.regions = &region;
        config.region_count = 1U;
        config.channels = channels;
        config.sample_times = samples;
        config.converters = converters;
        config.adc_clock_hz = 21000000U;
        config.timer_clock_hz = 84000000U;
        config.period_us = 1000U;
        config.trigger_identity = 1U;
        config.drain_timeout_us = 100U;
        config.ranks = 2U;
        config.delay_cycles = 5U;
        config.priority = 5U;
        config.mode = NX_STM32_ADC_DUAL_REGULAR;
        s_hardware = &config;
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        s_hold_dma = s_hold_address = s_refuse_clock = s_refuse_adc = false;
        s_refuse_release = s_hidden_owner = s_hidden_common = false;
        s_refuse_pad = s_late_adc = false;
        s_lose_domain_on_data_read = false;
        s_hardware = nullptr;
        if (adc != nullptr) {
            ASSERT_EQ(munmap(adc, 4096U), 0);
        }
        if (gpio_mmio != MAP_FAILED) {
            EXPECT_EQ(munmap(gpio_mmio, 0x3000U), 0);
        }
    }
    void PrepareRequest(size_t count, size_t bytes) {
        ASSERT_EQ(nx_adc_group_prepare(&request, frames, bytes, count, 5000U),
                  NX_SUCCESS);
    }
    void Start(size_t count = 2U, size_t bytes = 16U) {
        ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
        PrepareRequest(count, bytes);
        ASSERT_EQ(nx_adc_group_submit(&port, &request), NX_SUCCESS);
        ASSERT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    }
    void CompleteDma() {
        memory.NDTR = 0U;
        memory.CR &= ~uint32_t(DMA_SxCR_EN);
        dma.LISR |= DMA_LISR_TCIF0;
        nx_stm32_adc_group_irq(&port);
    }
};

TEST_F(Stm32AdcGroup, ColdAcquisitionLeavesAllSamplingSourcesStopped) {
    common.CCR = ADC_CCR_ADCPRE_0;
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    for (unsigned i = 0U; i < 3U; ++i)
        EXPECT_EQ(adc[i].CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
    EXPECT_EQ(common.CCR, ADC_CCR_ADCPRE_0);
    EXPECT_EQ(rcc.APB2ENR, 0U);
    EXPECT_EQ(rcc.APB1ENR, 0U);
}

TEST_F(Stm32AdcGroup, ColdAcquisitionRequiresALiveSelectedSystem) {
    g_nx_stm32_system.started = false;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_STATE);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_EQ(rcc.APB2ENR, 0U);
}
TEST_F(Stm32AdcGroup, AdmissionRejectsALostClockWithoutBorrowing) {
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    PrepareRequest(2U, 16U);
    rcc.CFGR ^= RCC_CFGR_PPRE1_0;
    EXPECT_EQ(nx_adc_group_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(adc[0].CR2, 0U);
}
TEST_F(Stm32AdcGroup, ANewEpochCannotArmAnOldAcceptedRequest) {
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    PrepareRequest(2U, 16U);
    ASSERT_EQ(nx_adc_group_submit(&port, &request), NX_SUCCESS);
    ++g_nx_stm32_system.time_epoch;
    ASSERT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_STATE);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(Stm32AdcGroup, ClockFailurePreventsPublishingCompletedSamples) {
    Start();
    CompleteDma();
    g_nx_stm32_system.timer_error = NX_ERROR_IO;
    ASSERT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_STATE);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(Stm32AdcGroup, DeadTimeDomainStillRetainsARefusedDmaDrain) {
    Start();
    s_hold_dma = true;
    g_nx_stm32_system.started = false;
    EXPECT_EQ(nx_adc_group_service(&port), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_NE(memory.M0AR, 0U);
    s_hold_dma = false;
    EXPECT_EQ(nx_adc_group_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_STATE);
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
}
TEST_F(Stm32AdcGroup, ForeignThirdConverterCannotBeStoppedByDualOwner) {
    adc[2].CR2 = ADC_CR2_ADON;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(rcc.APB2ENR, 0U);
    EXPECT_EQ(adc[2].CR2, ADC_CR2_ADON);
    EXPECT_EQ(state.config, nullptr);
}
TEST_F(Stm32AdcGroup, SimultaneousSamplingRejectsTheSamePhysicalInput) {
    channels[1] = channels[0];
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(common.CCR, 0U);
    EXPECT_EQ(rcc.APB2ENR, 0U);
}
TEST_F(Stm32AdcGroup, RankTimingMustBeCompatibleBeforeAnyClockEffect) {
    samples[1] = 0U;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(rcc.APB2ENR, 0U);
}
TEST_F(Stm32AdcGroup, FailedClockAcquisitionRetainsItsOriginalIdentity) {
    s_refuse_clock = true;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_IO);
    EXPECT_EQ(state.config, &config);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_BUSY);
    s_refuse_clock = false;
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
    EXPECT_EQ(state.config, nullptr);
}
TEST_F(Stm32AdcGroup, DualRegularUsesWordCdrDmaAndRankMajorHalfwordOrder) {
    Start();
    ASSERT_NE(state.active, nullptr);
    EXPECT_EQ(common.CCR & ADC_CCR_MULTI, 6U);
    EXPECT_EQ(common.CCR & ADC_CCR_DMA, ADC_CCR_DMA_1);
    EXPECT_EQ(memory.NDTR, 4U);
    EXPECT_EQ(memory.CR & DMA_SxCR_PSIZE, DMA_SxCR_PSIZE_1);
    EXPECT_EQ(memory.CR & DMA_SxCR_MSIZE, DMA_SxCR_MSIZE_1);
    EXPECT_EQ(memory.PAR, uint32_t(reinterpret_cast<uintptr_t>(&common.CDR)));
    EXPECT_EQ(adc[0].SQR3 & 1023U, 0U | (2U << 5U));
    EXPECT_EQ(adc[1].SQR3 & 1023U, 1U | (3U << 5U));
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    CompleteDma();
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 2U);
    EXPECT_EQ(memory.M0AR, 0U);
    EXPECT_EQ(memory.PAR, 0U);
    EXPECT_EQ(state.active, nullptr);
}
TEST_F(Stm32AdcGroup, TripleRegularUsesHalfwordDmaModeOneWithoutRepacking) {
    config.mode = NX_STM32_ADC_TRIPLE_REGULAR;
    config.ranks = 1U;
    converters[2] = 3U;
    Start(3U, 18U);
    EXPECT_EQ(common.CCR & ADC_CCR_MULTI, 22U);
    EXPECT_EQ(common.CCR & ADC_CCR_DMA, ADC_CCR_DMA_0);
    EXPECT_EQ(memory.NDTR, 9U);
    EXPECT_EQ(memory.CR & DMA_SxCR_PSIZE, DMA_SxCR_PSIZE_0);
    EXPECT_EQ(memory.CR & DMA_SxCR_MSIZE, DMA_SxCR_MSIZE_0);
}
TEST_F(Stm32AdcGroup, TripleInterleavedFramesCoverExactlyThreeWordPairs) {
    config.mode = NX_STM32_ADC_TRIPLE_INTERLEAVED;
    config.period_us = 0U;
    config.ranks = 1U;
    converters[2] = 3U;
    channels[1] = channels[2] = 0U;
    for (unsigned i = 0U; i < 3U; ++i) {
        converters[i + 3U] = converters[i];
        channels[i + 3U] = channels[i];
        samples[i + 3U] = samples[i];
    }
    Start(2U, 24U);
    EXPECT_EQ(common.CCR & ADC_CCR_MULTI, 23U);
    EXPECT_EQ(memory.NDTR, 6U);
    EXPECT_NE(adc[0].CR2 & ADC_CR2_CONT, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_NE(adc[0].CR2 & ADC_CR2_SWSTART, 0U);
    EXPECT_EQ(nx_adc_group_cancel(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(common.CCR & ADC_CCR_MULTI, 0U);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}
TEST_F(Stm32AdcGroup, InterleaveRequiresOneIdenticalInputAndSamplingTime) {
    config.mode = NX_STM32_ADC_DUAL_INTERLEAVED;
    config.period_us = 0U;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_INVALID);
    config.ranks = 1U;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_INVALID);
    channels[1] = 0U;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
}
TEST_F(Stm32AdcGroup, RequestGeometryRejectsWithoutRetainingOrStarting) {
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    PrepareRequest(2U, 14U);
    const nx_adc_group_request_t before = request;
    EXPECT_EQ(nx_adc_group_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&request, &before, sizeof(request)), 0);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(adc[0].CR2, 0U);
}
TEST_F(Stm32AdcGroup, NonDmaAndControlStorageCannotBecomeWritableOutput) {
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    PrepareRequest(2U, 16U);
    request.output.frames = &state;
    EXPECT_EQ(nx_adc_group_submit(&port, &request), NX_ERROR_INVALID);
    request.output.frames = frames;
    region.permissions = NX_DMA_MEMORY_READ;
    EXPECT_NE(nx_adc_group_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(state.active, nullptr);
}
TEST_F(Stm32AdcGroup, DeadlineExpiresDuringWarmupWithoutStartingTrigger) {
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    PrepareRequest(2U, 16U);
    request.base.deadline = 102U;
    ASSERT_EQ(nx_adc_group_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(3U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(Stm32AdcGroup, RefusedConverterEnableIsAcceptedFailureWithFullDrain) {
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    PrepareRequest(2U, 16U);
    s_refuse_adc = true;
    ASSERT_EQ(nx_adc_group_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(Stm32AdcGroup, CoObservedDmaFaultWinsOverTransferComplete) {
    Start();
    CompleteDma();
    dma.LISR |= DMA_LISR_TEIF0;
    nx_stm32_adc_group_irq(&port);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(Stm32AdcGroup, IncompleteNdtrNeverPublishesPartialFrames) {
    Start();
    dma.LISR |= DMA_LISR_TCIF0;
    nx_stm32_adc_group_irq(&port);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(Stm32AdcGroup, CompletionFirstObservedAtDeadlineIsTimeout) {
    Start();
    CompleteDma();
    ASSERT_EQ(nx_native_clock_advance(5000U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
}
TEST_F(Stm32AdcGroup, RefusedDmaDisableQuarantinesAndKeepsThePayloadBorrow) {
    Start();
    s_hold_dma = true;
    ASSERT_EQ(nx_adc_group_cancel(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_clock_advance(101U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_ERROR_IO);
    EXPECT_NE(memory.M0AR, 0U);
    s_hold_dma = false;
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(memory.M0AR, 0U);
}
TEST_F(Stm32AdcGroup, RefusedHardwareAddressClearRetainsTheSettlingRequest) {
    Start();
    CompleteDma();
    s_hold_address = true;
    EXPECT_EQ(nx_adc_group_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(state.active, &request);
    s_hold_address = false;
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(Stm32AdcGroup, LateErrorAtAcknowledgeCannotTurnIntoSuccessfulSamples) {
    Start();
    CompleteDma();
    s_late_error = true;
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(Stm32AdcGroup, HardwareWatchdogFromAnyParticipantStopsTheWholeGroup) {
    config.watchdog = true;
    config.lower = 20U;
    config.upper = 4000U;
    Start();
    EXPECT_EQ(adc[1].LTR, 20U);
    EXPECT_EQ(adc[1].HTR, 4000U);
    EXPECT_NE(adc[1].CR1 & ADC_CR1_AWDEN, 0U);
    adc[1].SR |= ADC_SR_AWD;
    nx_stm32_adc_group_irq(&port);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
}
TEST_F(Stm32AdcGroup, InvalidWatchdogThresholdsArePureAdmissionFailures) {
    config.watchdog = true;
    config.lower = 200U;
    config.upper = 100U;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(rcc.APB2ENR, 0U);
}
TEST_F(Stm32AdcGroup, SoftwareInjectedTripleUsesRightAlignedJsqrAndJdrOrder) {
    config.mode = NX_STM32_ADC_TRIPLE_INJECTED;
    config.period_us = 0U;
    converters[2] = 3U;
    converters[3] = 1U;
    converters[4] = 2U;
    converters[5] = 3U;
    channels[4] = 4U;
    channels[5] = 5U;
    samples[4] = samples[5] = 7U;
    Start(1U, 12U);
    EXPECT_EQ(common.CCR & ADC_CCR_MULTI, 21U);
    EXPECT_EQ(adc[0].JSQR, (1U << 20U) | (0U << 10U) | (3U << 15U));
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    for (unsigned index = 0U; index < 3U; ++index) {
        adc[index].JDR1 = 10U + index;
        adc[index].JDR2 = 20U + index;
        adc[index].SR |= ADC_SR_JEOC;
    }
    nx_stm32_adc_group_irq(&port);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    const uint16_t expected[]{10U, 11U, 12U, 20U, 21U, 22U};
    EXPECT_EQ(std::memcmp(frames, expected, sizeof(expected)), 0);
    EXPECT_EQ(request.base.transferred, 1U);
}
TEST_F(Stm32AdcGroup,
       PartialInjectedCompletionKeepsBorrowAndAllSamplesPrivate) {
    config.mode = NX_STM32_ADC_DUAL_INJECTED;
    config.period_us = 0U;
    Start(1U, 8U);
    adc[0].JDR1 = 123U;
    adc[0].SR |= ADC_SR_JEOC;
    nx_stm32_adc_group_irq(&port);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(frames[0], 0U);
    EXPECT_EQ(nx_adc_group_stop(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}
TEST_F(Stm32AdcGroup, InjectedTimerUsesOnePulseAndCannotOverwriteASecondFrame) {
    config.mode = NX_STM32_ADC_DUAL_INJECTED;
    config.trigger = 2U;
    config.period_us = 1000U;
    Start(1U, 8U);
    EXPECT_NE(timer.CR1 & TIM_CR1_OPM, 0U);
    EXPECT_NE(adc[0].CR2 & ADC_CR2_JEXTEN, 0U);
    EXPECT_EQ(adc[1].CR2 & ADC_CR2_JEXTEN, 0U);
}
TEST_F(Stm32AdcGroup, StopIsIdempotentButCloseWithdrawsTheColdLeases) {
    Start();
    EXPECT_EQ(nx_adc_group_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_stop(&port), NX_SUCCESS);
    EXPECT_NE(rcc.APB2ENR & RCC_APB2ENR_ADC1EN, 0U);
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_EQ(rcc.APB2ENR, 0U);
    EXPECT_EQ(rcc.APB1ENR, 0U);
}
TEST_F(Stm32AdcGroup, TerminalDmaIrqGatesSamplingBeforeTaskService) {
    Start();
    CompleteDma();
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(adc[0].CR2 & (ADC_CR2_EXTEN | ADC_CR2_ADON), 0U);
    EXPECT_EQ(adc[1].CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(state.active, &request);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(Stm32AdcGroup, InfoCannotWriteIntoActiveRequestOrItsDmaBuffer) {
    Start(16U, 128U);
    nx_adc_group_info_t* output =
        reinterpret_cast<nx_adc_group_info_t*>(frames);
    EXPECT_EQ(nx_adc_group_info(&port, output), NX_ERROR_INVALID);
    EXPECT_EQ(nx_adc_group_info(
                  &port, reinterpret_cast<nx_adc_group_info_t*>(&request)),
              NX_ERROR_INVALID);
}
TEST_F(Stm32AdcGroup, ReducedResolutionWatchdogUsesTheTwelveBitComparator) {
    config.resolution = 2U;
    config.watchdog = true;
    config.lower = 20U;
    config.upper = 240U;
    Start();
    EXPECT_EQ(adc[0].LTR, 320U);
    EXPECT_EQ(adc[1].HTR, 3840U);
}
TEST_F(Stm32AdcGroup, EqualChannelNumbersCanNameDifferentPhysicalAdcThreePads) {
    config.mode = NX_STM32_ADC_TRIPLE_REGULAR;
    config.ranks = 1U;
    channels[0] = 4U;
    channels[1] = 5U;
    channels[2] = 4U;
    converters[2] = 3U;
    /* ADC1_IN4 is PA4, ADC2_IN5 is PA5, ADC3_IN4 is PF6. */
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
}
TEST_F(Stm32AdcGroup, FailedColdGateWithdrawalSealsAdmissionUntilJoinedClose) {
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    s_refuse_release = true;
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_ERROR_IO);
    PrepareRequest(2U, 16U);
    EXPECT_EQ(nx_adc_group_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    s_refuse_release = false;
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
}
TEST_F(Stm32AdcGroup, ForeignConverterRevealedAfterGateProbeIsNeverStopped) {
    s_hidden_owner = true;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_FALSE(state.initialized);
    EXPECT_NE(adc[2].CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
    EXPECT_NE(adc[2].CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(rcc.APB2ENR, 0U);
}
TEST_F(Stm32AdcGroup, CommonSnapshotIsReadOnlyAfterTheClockProbeCompletes) {
    s_hidden_common = true;
    Start();
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
    EXPECT_EQ(common.CCR, ADC_CCR_ADCPRE_1);
}
TEST_F(Stm32AdcGroup, ExplicitStopClearsOldErrorBeforeStartingANewRequest) {
    Start();
    CompleteDma();
    s_late_error = true;
    ASSERT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    ASSERT_EQ(request.base.result, NX_ERROR_IO);
    ASSERT_EQ(nx_adc_group_stop(&port), NX_SUCCESS);
    PrepareRequest(2U, 16U);
    ASSERT_EQ(nx_adc_group_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(20U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
}
TEST_F(Stm32AdcGroup, ColdPrepareInstallsAnalogModesOnlyOnSelectedInputs) {
    GPIOA->MODER = 1U << 20U;
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    EXPECT_EQ(GPIOA->MODER & 255U, 255U);
    EXPECT_EQ(GPIOA->MODER & (3U << 20U), 1U << 20U);
    EXPECT_EQ(GPIOA->PUPDR & 255U, 0U);
}
TEST_F(Stm32AdcGroup, RefusedAnalogReadbackRetainsAColdRollbackReceipt) {
    s_refuse_pad = true;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_IO);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(state.config, &config);
    s_refuse_pad = false;
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
}
TEST_F(Stm32AdcGroup, LateInjectedErrorAfterGatingInvalidatesTheWholeFrame) {
    config.mode = NX_STM32_ADC_DUAL_INJECTED;
    config.period_us = 0U;
    Start(1U, 8U);
    adc[0].SR |= ADC_SR_JEOC;
    adc[1].SR |= ADC_SR_JEOC;
    nx_stm32_adc_group_irq(&port);
    s_late_adc = true;
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(Stm32AdcGroup, InterleavedInfoDoesNotInventAFullFrameTimeBudget) {
    config.mode = NX_STM32_ADC_TRIPLE_INTERLEAVED;
    config.period_us = 0U;
    config.ranks = 1U;
    config.delay_cycles = 20U;
    for (unsigned i = 0U; i < 6U; ++i) {
        channels[i] = samples[i] = 0U;
        converters[i] = uint8_t(i % 3U + 1U);
    }
    Start(2U, 24U);
    nx_adc_group_info_t facts{};
    ASSERT_EQ(nx_adc_group_info(&port, &facts), NX_SUCCESS);
    EXPECT_EQ(facts.physical_scan_halfcycles, 0U);
    EXPECT_EQ(facts.physical_ranks, 6U);
    EXPECT_EQ(facts.interleave_delay_cycles, 20U);
}
TEST_F(Stm32AdcGroup,
       LowResolutionInterleaveUsesNativeBytePairsWithoutPacking) {
    config.mode = NX_STM32_ADC_DUAL_INTERLEAVED;
    config.resolution = 2U;
    config.period_us = 0U;
    config.ranks = 1U;
    channels[1] = 0U;
    Start(2U, 4U);
    EXPECT_EQ(common.CCR & ADC_CCR_DMA, ADC_CCR_DMA);
    EXPECT_EQ(memory.NDTR, 2U);
    EXPECT_EQ(memory.CR & DMA_SxCR_PSIZE, DMA_SxCR_PSIZE_0);
    EXPECT_EQ(memory.CR & DMA_SxCR_MSIZE, DMA_SxCR_MSIZE_0);
    nx_adc_group_info_t facts{};
    ASSERT_EQ(nx_adc_group_info(&port, &facts), NX_SUCCESS);
    EXPECT_EQ(facts.format.storage_bytes, 1U);
    EXPECT_EQ(facts.format.sample_stride, 1U);
    EXPECT_EQ(facts.format.scan_stride, 2U);
    EXPECT_EQ(facts.buffer_alignment, 2U);
}
TEST_F(Stm32AdcGroup, RepeatedTripleMetadataMustDescribeTheActualPairOrder) {
    config.mode = NX_STM32_ADC_TRIPLE_INTERLEAVED;
    config.period_us = 0U;
    config.ranks = 1U;
    for (unsigned i = 0U; i < 6U; ++i) {
        channels[i] = samples[i] = 0U;
        converters[i] = uint8_t(i % 3U + 1U);
    }
    converters[5] = 2U;
    EXPECT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_EQ(rcc.APB2ENR, 0U);
}

TEST_F(Stm32AdcGroup, DomainLossDuringLastInjectedReadCannotPublishSuccess) {
    config.mode = NX_STM32_ADC_DUAL_INJECTED;
    config.period_us = 0U;
    Start(1U, 8U);
    for (unsigned i = 0U; i < 2U; ++i) {
        adc[i].JDR1 = 10U + i;
        adc[i].JDR2 = 20U + i;
        adc[i].SR |= ADC_SR_JEOC;
    }
    nx_stm32_adc_group_irq(&port);
    s_lose_domain_on_data_read = true;
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_FALSE(s_lose_domain_on_data_read);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_STATE);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(Stm32AdcGroup, WarmupExhaustionSettlesTheAcceptedRequestWithoutTrigger) {
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    PrepareRequest(2U, 16U);
    request.base.deadline = NX_DEADLINE_NEVER - 1U;
    ASSERT_EQ(nx_native_clock_configure(true, NX_DEADLINE_NEVER - 3U),
              NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
}

TEST_F(Stm32AdcGroup, UnrepresentableDrainGraceQuarantinesWithoutInfiniteWait) {
    Start();
    s_hold_dma = true;
    ASSERT_EQ(
        nx_native_clock_advance(NX_DEADLINE_NEVER - 3U - nx_time_now_us()),
        NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_cancel(&port, &request), NX_SUCCESS);
    EXPECT_NE(state.drain_deadline, NX_DEADLINE_NEVER);
    EXPECT_EQ(nx_adc_group_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(memory.M0AR, 0U);
    s_hold_dma = false;
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}

TEST_F(Stm32AdcGroup, RegularSixteenRanksNeverProgramInjectedReservedBits) {
    uint8_t selected_channels[32]{};
    uint8_t selected_samples[32]{};
    uint8_t selected_converters[32]{};
    for (unsigned i = 0U; i < 32U; ++i) {
        selected_channels[i] = static_cast<uint8_t>(i % 2U);
        selected_samples[i] = 7U;
        selected_converters[i] = static_cast<uint8_t>(i % 2U + 1U);
    }
    config.channels = selected_channels;
    config.sample_times = selected_samples;
    config.converters = selected_converters;
    config.ranks = 16U;
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(PrepareRequest(1U, 64U));
    ASSERT_EQ(nx_adc_group_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(adc[0].JSQR, 0U);
    EXPECT_EQ(adc[1].JSQR, 0U);
    EXPECT_EQ(adc[0].CR2 & ADC_CR2_JSWSTART, 0U);
    EXPECT_EQ(nx_stm32_adc_group_close(&state), NX_SUCCESS);
}

extern "C" {
#include "stm32f407_adc_group_blocks.h"
nx_result_t
nx_stm32_adc_group_blocks_prepare(nx_stm32_adc_group_blocks_state_t*,
                                  const nx_stm32_adc_group_blocks_config_t*)
    __attribute__((weak));
nx_result_t nx_stm32_adc_group_blocks_close(nx_stm32_adc_group_blocks_state_t*)
    __attribute__((weak));
void nx_stm32_adc_group_blocks_irq(const nx_adc_group_port_t*)
    __attribute__((weak));
extern const nx_adc_group_ops_t nx_stm32_adc_group_blocks_ops
    __attribute__((weak));
nx_result_t nx_adc_group_start_blocks(const nx_adc_group_port_t*, nx_stream_t*)
    __attribute__((weak));
nx_result_t nx_adc_group_attach_observer(const nx_adc_group_port_t*,
                                         nx_adc_observer_t*)
    __attribute__((weak));
nx_result_t nx_adc_group_block_observation(const nx_adc_group_port_t*,
                                           const nx_stream_block_t*,
                                           nx_adc_observation_t*)
    __attribute__((weak));
}

/** \brief External inactive-bank store, never a mock of the producer logic. */
/** \brief ADC_SR is RC_W0; clearing JEOC preserves regular OVR and AWD. */
extern "C" void nx_stm32_adc_group_model_jeoc_ack(void* raw) {
    auto* adc = static_cast<ADC_TypeDef*>(raw);
    adc->SR &= ~uint32_t(ADC_SR_JEOC);
}

extern "C" void nx_stm32_adc_group_model_target(void* raw, unsigned bank,
                                                uint32_t address) {
    auto* memory = static_cast<DMA_Stream_TypeDef*>(raw);
    if (!s_blocks_refuse_target) {
        if (bank == 0U)
            memory->M0AR = address;
        else
            memory->M1AR = address;
    }
    if (s_blocks_flip_target) {
        memory->CR ^= DMA_SxCR_CT;
        s_blocks_flip_target = false;
    }
    if (s_blocks_lose_clock && s_hardware != nullptr) {
        s_hardware->rcc->AHB1ENR &= ~uint32_t(RCC_AHB1ENR_DMA2EN);
        s_blocks_lose_clock = false;
    }
    if (s_blocks_sticky_at_target && s_hardware != nullptr) {
        /* The third producer loan already exists. A late hardware error
         * asserts the level while the inactive-address transaction proceeds. */
        s_hardware->dma->LISR |= DMA_LISR_TEIF0;
        s_blocks_sticky_at_target = false;
    }
}

namespace {
class AdcGroupBlocks : public Stm32AdcGroup {
  protected:
    nx_stm32_adc_group_blocks_config_t block_config{};
    nx_stm32_adc_group_blocks_state_t blocks{};
    nx_adc_group_port_t block_port{&nx_stm32_adc_group_blocks_ops, &blocks};
    alignas(8) uint8_t pool[5][64]{};
    nx_stream_slot_t slots[5]{};
    nx_stream_t stream{};
    nx_dma_memory_region_t block_regions[2]{};
    nx_adc_observation_t records[5]{};
    nx_adc_observer_t observer{};

    void SetUp() override {
        Stm32AdcGroup::SetUp();
        s_blocks_refuse_target = s_blocks_flip_target = false;
        s_blocks_lose_clock = s_blocks_jump_start = false;
        s_blocks_sticky_at_target = s_blocks_refuse_ack = false;
        s_blocks_live_irq_after_gate = false;
        s_blocks_refused_acks = 0U;
        s_blocks_loans_at_gate = 0U;
        s_blocks_stream = &stream;
        rcc.AHB1ENR |= RCC_AHB1ENR_DMA2EN;
        block_config.adc = config;
        block_regions[0] = {reinterpret_cast<uintptr_t>(pool), sizeof(pool),
                            NX_DMA_MEMORY_WRITE};
        block_regions[1] = {reinterpret_cast<uintptr_t>(&blocks),
                            sizeof(blocks), NX_DMA_MEMORY_WRITE};
        block_config.adc.regions = block_regions;
        block_config.adc.region_count = 2U;
        block_config.max_service_gap_us = 1000U;
        for (size_t i = 0U; i < 5U; ++i) {
            slots[i].data = pool[i];
            slots[i].capacity = sizeof(pool[i]);
        }
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 5U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_observer_initialize(&observer, records, 5U),
                  NX_SUCCESS);
    }
    void TearDown() override {
        s_blocks_refuse_target = s_blocks_flip_target = false;
        s_blocks_lose_clock = s_blocks_jump_start = false;
        s_blocks_sticky_at_target = s_blocks_refuse_ack = false;
        s_blocks_stream = nullptr;
        Stm32AdcGroup::TearDown();
    }
    void PrepareBlocks() {
        ASSERT_NE(&nx_stm32_adc_group_blocks_ops, nullptr);
        ASSERT_NE(nx_stm32_adc_group_blocks_prepare, nullptr);
        ASSERT_NE(nx_stm32_adc_group_blocks_close, nullptr);
        ASSERT_NE(nx_stm32_adc_group_blocks_irq, nullptr);
        ASSERT_NE(nx_adc_group_start_blocks, nullptr);
        ASSERT_NE(nx_adc_group_attach_observer, nullptr);
        ASSERT_NE(nx_adc_group_block_observation, nullptr);
        ASSERT_EQ(nx_stm32_adc_group_blocks_prepare(&blocks, &block_config),
                  NX_SUCCESS);
    }
    void ReadyBlocks() {
        ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
        ASSERT_EQ(nx_adc_group_start_blocks(&block_port, &stream), NX_SUCCESS);
        ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
    }
    void Complete(unsigned bank) {
        const unsigned count =
            block_config.adc.mode == NX_STM32_ADC_TRIPLE_REGULAR ? 3U : 2U;
        const uint64_t frame_bytes = 2U * count * block_config.adc.ranks;
        uint64_t elapsed =
            slots[0].capacity / frame_bytes * block_config.adc.period_us + 100U;
        while (elapsed > block_config.max_service_gap_us) {
            ASSERT_EQ(nx_native_clock_advance(block_config.max_service_gap_us),
                      NX_SUCCESS);
            ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
            elapsed -= block_config.max_service_gap_us;
        }
        ASSERT_EQ(nx_native_clock_advance(elapsed), NX_SUCCESS);
        memory.CR = (memory.CR & ~uint32_t(DMA_SxCR_CT)) |
                    (bank == 0U ? DMA_SxCR_CT : 0U);
        memory.NDTR =
            static_cast<uint32_t>(slots[0].capacity / (count == 3U ? 2U : 4U));
        dma.LISR |= DMA_LISR_TCIF0;
        nx_stm32_adc_group_blocks_irq(&block_port);
    }
    void StopClose() {
        ASSERT_EQ(nx_adc_group_stop(&block_port), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_adc_group_blocks_close(&blocks), NX_SUCCESS);
    }
};

TEST_F(Stm32AdcGroup, FiniteInstancesKeepAllOptionalBlockMethodsAbsent) {
    EXPECT_EQ(nx_stm32_adc_group_ops.start_blocks, nullptr);
    EXPECT_EQ(nx_stm32_adc_group_ops.attach_observer, nullptr);
    EXPECT_EQ(nx_stm32_adc_group_ops.block_observation, nullptr);
}

TEST_F(AdcGroupBlocks, PreparationNeverStartsAConverterTimerOrMemoryEngine) {
    ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
    EXPECT_EQ(adc[0].CR2 | adc[1].CR2 | adc[2].CR2, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(stream.outstanding, 0U);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}

TEST_F(AdcGroupBlocks, DualCdrUsesWordDmaAndTwoPrepostedCompleteScanTargets) {
    ASSERT_NO_FATAL_FAILURE(ReadyBlocks());
    EXPECT_EQ(2U, stream.outstanding);
    EXPECT_EQ(16U, memory.NDTR);
    EXPECT_EQ(DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1,
              memory.CR & (DMA_SxCR_PSIZE | DMA_SxCR_MSIZE));
    EXPECT_EQ(DMA_SxCR_DBM | DMA_SxCR_CIRC,
              memory.CR & (DMA_SxCR_DBM | DMA_SxCR_CIRC));
    EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&common.CDR)),
              memory.PAR);
    nx_stream_block_t block{};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}

TEST_F(AdcGroupBlocks, TripleCdrKeepsThreeHalfwordsPerLogicalScan) {
    block_config.adc.mode = NX_STM32_ADC_TRIPLE_REGULAR;
    block_config.adc.ranks = 1U;
    converters[2] = 3U;
    for (auto& slot : slots)
        slot.capacity = 60U;
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 5U), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(ReadyBlocks());
    EXPECT_EQ(30U, memory.NDTR);
    EXPECT_EQ(DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0,
              memory.CR & (DMA_SxCR_PSIZE | DMA_SxCR_MSIZE));
    nx_adc_group_info_t info{};
    ASSERT_EQ(nx_adc_group_info(&block_port, &info), NX_SUCCESS);
    EXPECT_EQ(6U, info.format.scan_stride);
    EXPECT_EQ(3U, info.format.channel_count);
    EXPECT_TRUE(info.active);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}

TEST_F(AdcGroupBlocks,
       ReplacementReadbackPrecedesDataAndObservationPublication) {
    ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
    ASSERT_EQ(nx_adc_group_attach_observer(&block_port, &observer), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_start_blocks(&block_port, &stream), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Complete(0U));
    nx_stream_block_t block{};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
    EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pool[2])),
              memory.M0AR);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(block.data, pool[0]);
    EXPECT_EQ(block.length, sizeof(pool[0]));
    nx_adc_observation_t observation{};
    ASSERT_EQ(nx_adc_group_block_observation(&block_port, &block, &observation),
              NX_SUCCESS);
    EXPECT_EQ(observation.epoch, block.epoch);
    EXPECT_EQ(observation.sequence, 1U);
    EXPECT_EQ(observation.observed_at, nx_time_now_us());
    EXPECT_NE(observation.flags & NX_ADC_OBSERVATION_DISCONTINUITY, 0U);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}

TEST_F(AdcGroupBlocks, HeldConsumerPausesWithoutOverwriteAndResumesExplicitly) {
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 3U), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(ReadyBlocks());
    ASSERT_NO_FATAL_FAILURE(Complete(0U));
    ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
    nx_stream_block_t block{};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Complete(1U));
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_ERROR_BUSY);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(stream.outstanding, 1U);
    EXPECT_EQ(block.data, pool[0]);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_ERROR_BUSY);
    ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}

TEST_F(AdcGroupBlocks, RefusedReplacementAndDrainKeepAllThreeProducerLoans) {
    ASSERT_NO_FATAL_FAILURE(ReadyBlocks());
    ASSERT_NO_FATAL_FAILURE(Complete(0U));
    s_blocks_refuse_target = s_hold_dma = true;
    EXPECT_NE(nx_adc_group_service(&block_port), NX_SUCCESS);
    EXPECT_EQ(stream.outstanding, 3U);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(nx_adc_group_stop(&block_port), NX_SUCCESS);
    EXPECT_EQ(stream.outstanding, 3U);
    s_blocks_refuse_target = s_hold_dma = false;
    ASSERT_NO_FATAL_FAILURE(StopClose());
    EXPECT_EQ(stream.outstanding, 0U);
}

TEST_F(AdcGroupBlocks,
       RefusedIrqAckClosesEveryOwnedInterruptGateBeforeThreadDrain) {
    ASSERT_NO_FATAL_FAILURE(ReadyBlocks());
    ASSERT_NO_FATAL_FAILURE(Complete(0U));
    s_blocks_sticky_at_target = s_blocks_refuse_ack = s_hold_dma = true;
    EXPECT_NE(nx_adc_group_service(&block_port), NX_SUCCESS);
    EXPECT_NE(s_blocks_refused_acks, 0U);
    EXPECT_EQ(s_blocks_loans_at_gate, 3U);
    /* These trace facts precede task withdrawal: it must be possible to return
     * to Thread mode even when W1C and the memory engine refuse to stop. */
    EXPECT_FALSE(s_blocks_live_irq_after_gate);
    EXPECT_EQ(memory.CR & (DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE), 0U);
    EXPECT_EQ(memory.FCR & DMA_SxFCR_FEIE, 0U);
    EXPECT_NE(dma.LISR & DMA_LISR_TEIF0, 0U);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(stream.outstanding, 3U);
    nx_stream_block_t block{};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    s_blocks_refuse_ack = s_hold_dma = false;
    ASSERT_NO_FATAL_FAILURE(StopClose());
}

TEST_F(AdcGroupBlocks, BankFlipAtAddressWriteNeverPublishesTheUnprovedBank) {
    ASSERT_NO_FATAL_FAILURE(ReadyBlocks());
    ASSERT_NO_FATAL_FAILURE(Complete(0U));
    s_blocks_flip_target = true;
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_ERROR_IO);
    nx_stream_block_t block{};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}

TEST_F(AdcGroupBlocks, ClockLossAtReplacementCannotReleaseOrPublishAnyLoan) {
    ASSERT_NO_FATAL_FAILURE(ReadyBlocks());
    ASSERT_NO_FATAL_FAILURE(Complete(0U));
    s_blocks_lose_clock = true;
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_ERROR_STATE);
    EXPECT_EQ(stream.outstanding, 3U);
    nx_stream_block_t block{};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_adc_group_stop(&block_port), NX_ERROR_STATE);
    /* The isolated model world ends here. No storage release or successful
     * close is claimed after the physical clock history was lost. */
}

TEST_F(AdcGroupBlocks, PayloadOverTheExtendedOwnerRejectsWithoutAnyLoan) {
    block_config.max_service_gap_us = 100U;
    for (auto& slot : slots)
        slot.capacity = 16U;
    slots[0].data = reinterpret_cast<uint8_t*>(&blocks.observed_at);
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 5U), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
    EXPECT_EQ(nx_adc_group_start_blocks(&block_port, &stream),
              NX_ERROR_INVALID);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(stream.epoch, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(adc[0].CR2, 0U);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}

TEST_F(AdcGroupBlocks, ObserverExhaustionNeverPublishesMissingMetadata) {
    ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
    observer.sequence = UINT64_MAX;
    ASSERT_EQ(nx_adc_group_attach_observer(&block_port, &observer), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_start_blocks(&block_port, &stream), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(Complete(0U));
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_ERROR_EXHAUSTED);
    nx_stream_block_t block{};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(observer.sequence, UINT64_MAX);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}

TEST_F(AdcGroupBlocks, APreemptedArmCannotEraseItsUnobservedDmaBudget) {
    ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
    ASSERT_EQ(nx_adc_group_start_blocks(&block_port, &stream), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
    s_blocks_jump_start = true;
    (void)nx_adc_group_service(&block_port);
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_ERROR_IO);
    nx_stream_block_t block{};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}
} /* namespace */

extern "C" {
#include "stm32f407_adc_group_combined.h"
nx_result_t
nx_stm32_adc_group_combined_prepare(nx_stm32_adc_group_combined_state_t*,
                                    const nx_stm32_adc_group_combined_config_t*)
    __attribute__((weak));
nx_result_t
nx_stm32_adc_group_combined_close(nx_stm32_adc_group_combined_state_t*)
    __attribute__((weak));
void nx_stm32_adc_group_combined_irq(const nx_adc_group_port_t*)
    __attribute__((weak));
extern const nx_adc_group_ops_t nx_stm32_adc_group_combined_ops
    __attribute__((weak));
nx_result_t nx_adc_group_submit_injected(const nx_adc_group_port_t*,
                                         nx_adc_group_request_t*)
    __attribute__((weak));
nx_result_t nx_adc_group_injected_info(const nx_adc_group_port_t*,
                                       nx_adc_group_info_t*)
    __attribute__((weak));
}

namespace {
class AdcGroupCombined : public AdcGroupBlocks {
  protected:
    nx_stm32_adc_group_combined_config_t selected{};
    nx_stm32_adc_group_combined_state_t combined{};
    nx_adc_group_port_t combined_port{&nx_stm32_adc_group_combined_ops,
                                      &combined};
    uint8_t injected_channels[12]{4U, 5U};
    uint8_t injected_samples[12]{7U, 7U};
    uint8_t injected_converters[12]{1U, 2U};
    uint16_t injected_values[12]{};
    nx_adc_group_request_t injection{};
    void* sample_memory = MAP_FAILED;
    unsigned regular_rank = 0U;

    void SetUp() override {
        AdcGroupBlocks::SetUp();
        sample_memory = mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
        ASSERT_NE(sample_memory, MAP_FAILED);
        for (size_t i = 0U; i < 5U; ++i)
            slots[i].data = static_cast<uint8_t*>(sample_memory) + i * 64U;
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 5U), NX_SUCCESS);
        block_regions[0] = {reinterpret_cast<uintptr_t>(sample_memory), 320U,
                            NX_DMA_MEMORY_WRITE};
        block_regions[1] = {reinterpret_cast<uintptr_t>(&combined),
                            sizeof(combined), NX_DMA_MEMORY_WRITE};
        selected.regular = block_config;
        selected.regular.adc.mode = NX_STM32_ADC_DUAL_REGULAR;
        selected.injected_channels = injected_channels;
        selected.injected_samples = injected_samples;
        selected.injected_converters = injected_converters;
        selected.injected_ranks = 1U;
        selected.min_injected_interval_us = 1100U;
        s_hardware = &selected.regular.adc;
        nx_request_initialize(&injection.base);
        s_combined_expire_on_enable = s_combined_watch_start = false;
        s_combined_saw_start = false;
    }
    void TearDown() override {
        s_combined_expire_on_enable = s_combined_watch_start = false;
        if (sample_memory != MAP_FAILED) {
            EXPECT_EQ(munmap(sample_memory, 4096U), 0);
        }
        AdcGroupBlocks::TearDown();
    }
    void PrepareCombined() {
        ASSERT_NE(&nx_stm32_adc_group_combined_ops, nullptr);
        ASSERT_NE(+nx_stm32_adc_group_combined_prepare, nullptr);
        ASSERT_NE(+nx_stm32_adc_group_combined_close, nullptr);
        ASSERT_NE(+nx_stm32_adc_group_combined_irq, nullptr);
        ASSERT_NE(+nx_adc_group_submit_injected, nullptr);
        ASSERT_NE(+nx_adc_group_injected_info, nullptr);
        ASSERT_EQ(nx_stm32_adc_group_combined_prepare(&combined, &selected),
                  NX_SUCCESS);
    }
    void ReadyCombined() {
        ASSERT_NO_FATAL_FAILURE(PrepareCombined());
        ASSERT_EQ(nx_adc_group_start_blocks(&combined_port, &stream),
                  NX_SUCCESS);
        ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    }
    unsigned Count() const {
        return selected.regular.adc.mode == NX_STM32_ADC_TRIPLE_REGULAR ? 3U
                                                                        : 2U;
    }
    void PrepareInjection(nx_time_us_t deadline = 5000U) {
        const size_t bytes = size_t(Count()) * selected.injected_ranks * 2U;
        ASSERT_EQ(nx_adc_group_prepare(&injection, injected_values, bytes, 1U,
                                       deadline),
                  NX_SUCCESS);
    }
    void Inject() {
        ASSERT_NO_FATAL_FAILURE(PrepareInjection());
        ASSERT_EQ(nx_adc_group_submit_injected(&combined_port, &injection),
                  NX_SUCCESS);
    }
    /** The peripheral owns conversion/JDR updates; no driver is called here. */
    void HardwareJeoc(unsigned count) {
        ASSERT_NE(adc[0].CR2 & ADC_CR2_JSWSTART, 0U);
        for (unsigned i = 0U; i < count; ++i) {
            for (unsigned rank = 0U; rank < selected.injected_ranks; ++rank) {
                const unsigned shift =
                    (rank + 4U - selected.injected_ranks) * 5U;
                const uint32_t channel = (adc[i].JSQR >> shift) & 31U;
                auto* jdr = &adc[i].JDR1 + rank;
                *jdr = 0x100U * (i + 1U) + channel;
            }
            adc[i].SR |= ADC_SR_JEOC;
        }
        if (count == Count())
            adc[0].CR2 &= ~uint32_t(ADC_CR2_JSWSTART);
        nx_stm32_adc_group_combined_irq(&combined_port);
    }
    /** One completed regular simultaneous pair writes the actual DBM target. */
    void HardwareRegularPair() {
        ASSERT_EQ(Count(), 2U);
        ASSERT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
        ASSERT_NE(adc[0].CR2 & ADC_CR2_EXTEN, 0U);
        ASSERT_NE(memory.CR & DMA_SxCR_EN, 0U);
        ASSERT_EQ(adc[0].CR2 & ADC_CR2_JSWSTART, 0U);
        const uint32_t a = (adc[0].SQR3 >> (regular_rank * 5U)) & 31U;
        const uint32_t b = (adc[1].SQR3 >> (regular_rank * 5U)) & 31U;
        common.CDR = ((0x200U + b) << 16U) | (0x100U + a);
        const bool bank = (memory.CR & DMA_SxCR_CT) != 0U;
        const uintptr_t address = bank ? memory.M1AR : memory.M0AR;
        const size_t words = slots[0].capacity / sizeof(uint32_t);
        ASSERT_NE(memory.NDTR, 0U);
        auto* target = reinterpret_cast<uint32_t*>(address);
        target[words - memory.NDTR] = common.CDR;
        --memory.NDTR;
        regular_rank = (regular_rank + 1U) % selected.regular.adc.ranks;
        if (memory.NDTR == 0U) {
            memory.NDTR = static_cast<uint32_t>(words);
            memory.CR ^= DMA_SxCR_CT;
            dma.LISR |= DMA_LISR_TCIF0;
        }
    }
    void StopCombined() {
        ASSERT_EQ(nx_adc_group_stop(&combined_port), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_adc_group_combined_close(&combined), NX_SUCCESS);
    }
};

TEST_F(AdcGroupCombined, BothColdRankSetsExistBeforeAnySamplingSourceStarts) {
    ASSERT_NO_FATAL_FAILURE(PrepareCombined());
    EXPECT_EQ(adc[0].CR2 | adc[1].CR2 | adc[2].CR2, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_EQ(nx_adc_group_start_blocks(&combined_port, &stream), NX_SUCCESS);
    EXPECT_EQ(common.CCR & ADC_CCR_MULTI, ADC_CCR_MULTI_0);
    EXPECT_EQ(adc[0].JSQR, 4U << 15U);
    EXPECT_EQ(adc[1].JSQR, 5U << 15U);
    EXPECT_EQ((adc[0].SMPR2 >> 12U) & 7U, 7U);
    EXPECT_EQ(adc[0].CR1 & (ADC_CR1_JAUTO | ADC_CR1_JDISCEN), 0U);
    EXPECT_EQ(adc[0].CR2 & (ADC_CR2_JEXTEN | ADC_CR2_JSWSTART), 0U);
    ASSERT_NO_FATAL_FAILURE(StopCombined());
}
TEST_F(AdcGroupCombined, RegularAndInjectedFormatsRemainExplicitAndDistinct) {
    ASSERT_NO_FATAL_FAILURE(PrepareCombined());
    nx_adc_group_info_t regular{}, injected{};
    ASSERT_EQ(nx_adc_group_info(&combined_port, &regular), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_injected_info(&combined_port, &injected),
              NX_SUCCESS);
    EXPECT_EQ(regular.format.group, NX_ADC_REGULAR);
    EXPECT_EQ(regular.format.scan_stride, 8U);
    EXPECT_EQ(injected.format.group, NX_ADC_INJECTED);
    EXPECT_EQ(injected.format.scan_stride, 4U);
    EXPECT_EQ(injected.format.trigger, NX_ADC_TRIGGER_SOFTWARE);
    EXPECT_EQ(injected.format.trigger_identity, 0U);
    EXPECT_EQ(injected.max_frames, 1U);
    EXPECT_EQ(injected.channels, injected_channels);
    EXPECT_FALSE(injected.active);
    ASSERT_NO_FATAL_FAILURE(StopCombined());
}
TEST_F(AdcGroupCombined, SoftwareInjectionNeverStopsOrRewritesTheRegularRank) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(HardwareRegularPair());
    const uint32_t before = memory.NDTR;
    const uint32_t sqr = adc[0].SQR3;
    const uint32_t smpr = adc[0].SMPR2;
    ASSERT_NO_FATAL_FAILURE(Inject());
    EXPECT_NE(adc[0].CR2 & ADC_CR2_JSWSTART, 0U);
    EXPECT_EQ(adc[1].CR2 & ADC_CR2_JSWSTART, 0U);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(memory.NDTR, before);
    ASSERT_NO_FATAL_FAILURE(HardwareJeoc(Count()));
    ASSERT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(injection.base.result, NX_SUCCESS);
    EXPECT_EQ(injection.base.transferred, 1U);
    EXPECT_EQ(injected_values[0], 0x104U);
    EXPECT_EQ(injected_values[1], 0x205U);
    EXPECT_EQ(adc[0].SQR3, sqr);
    EXPECT_EQ(adc[0].SMPR2, smpr);
    ASSERT_NO_FATAL_FAILURE(HardwareRegularPair());
    EXPECT_EQ(memory.NDTR, before - 1U);
    auto* words = static_cast<uint32_t*>(sample_memory);
    EXPECT_EQ(words[0], (0x201U << 16U) | 0x100U);
    EXPECT_EQ(words[1], (0x203U << 16U) | 0x102U);
    ASSERT_NO_FATAL_FAILURE(StopCombined());
}
TEST_F(AdcGroupCombined, EveryConverterMustCompleteBeforeAnyJdrPublication) {
    selected.regular.adc.mode = NX_STM32_ADC_TRIPLE_REGULAR;
    selected.regular.adc.ranks = 1U;
    converters[2] = 3U;
    injected_channels[2] = 6U;
    injected_samples[2] = 7U;
    injected_converters[2] = 3U;
    for (auto& slot : slots)
        slot.capacity = 60U;
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 5U), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(Inject());
    ASSERT_NO_FATAL_FAILURE(HardwareJeoc(2U));
    EXPECT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(injected_values[0] | injected_values[1] | injected_values[2], 0U);
    EXPECT_EQ(adc[0].CR1 & ADC_CR1_JEOCIE, 0U);
    EXPECT_NE(adc[2].CR1 & ADC_CR1_JEOCIE, 0U);
    ASSERT_NO_FATAL_FAILURE(HardwareJeoc(3U));
    ASSERT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_EQ(injection.base.result, NX_SUCCESS);
    EXPECT_EQ(injected_values[2], 0x306U);
    ASSERT_NO_FATAL_FAILURE(StopCombined());
}
TEST_F(AdcGroupCombined, CancellingOneInjectionWaitsWithoutStoppingRegularDma) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(Inject());
    ASSERT_EQ(nx_adc_group_cancel(&combined_port, &injection), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_DRAINING);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_NO_FATAL_FAILURE(HardwareJeoc(Count()));
    ASSERT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(injection.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(injection.base.transferred, 0U);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(adc[0].CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(injected_values[0] | injected_values[1], 0U);
    ASSERT_NO_FATAL_FAILURE(StopCombined());
}
TEST_F(AdcGroupCombined, MissingJeocAtDeadlineDrainsBothDomainsBeforeRelease) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(PrepareInjection(nx_time_now_us() + 50U));
    ASSERT_EQ(nx_adc_group_submit_injected(&combined_port, &injection),
              NX_SUCCESS);
    s_hold_dma = true;
    ASSERT_EQ(nx_native_clock_advance(100U), NX_SUCCESS);
    EXPECT_NE(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(stream.outstanding, 2U);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    /* Completion facts can be observed after the owner entered joint drain.
     * They cannot turn a retained failure into a standalone JDR join. */
    for (unsigned i = 0U; i < Count(); ++i)
        adc[i].SR |= ADC_SR_JEOC;
    EXPECT_NE(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_NE(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(stream.outstanding, 2U);
    EXPECT_NE(memory.M0AR | memory.M1AR, 0U);
    EXPECT_NE(nx_adc_group_stop(&combined_port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    s_hold_dma = false;
    ASSERT_NO_FATAL_FAILURE(StopCombined());
    EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(injection.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(injection.base.transferred, 0U);
}
TEST_F(AdcGroupCombined, RegularOverrunCannotSettleInjectionBeforeMemoryJoin) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(Inject());
    s_hold_dma = true;
    adc[1].SR |= ADC_SR_OVR;
    nx_stm32_adc_group_combined_irq(&combined_port);
    EXPECT_NE(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(stream.outstanding, 2U);
    EXPECT_NE(memory.M0AR, 0U);
    s_hold_dma = false;
    ASSERT_NO_FATAL_FAILURE(StopCombined());
    EXPECT_EQ(injection.base.result, NX_ERROR_IO);
    EXPECT_EQ(injection.base.transferred, 0U);
}
TEST_F(AdcGroupCombined, TheSelectedInjectionIntervalIsCheckedBeforeBorrowing) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(Inject());
    ASSERT_NO_FATAL_FAILURE(HardwareJeoc(Count()));
    ASSERT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(PrepareInjection());
    EXPECT_EQ(nx_adc_group_submit_injected(&combined_port, &injection),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_READY);
    EXPECT_EQ(adc[0].CR2 & ADC_CR2_JSWSTART, 0U);
    ASSERT_EQ(nx_native_clock_advance(500U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(600U), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_submit_injected(&combined_port, &injection),
              NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(HardwareJeoc(Count()));
    ASSERT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(StopCombined());
}
TEST_F(AdcGroupCombined,
       PayloadCannotAliasCombinedOwnerOrRegularStreamStorage) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    for (void* bytes : {static_cast<void*>(&combined.last_injected),
                        static_cast<void*>(slots[0].data),
                        static_cast<void*>(&selected.injected_ranks)}) {
        ASSERT_EQ(nx_adc_group_prepare(&injection, bytes, 4U, 1U, 5000U),
                  NX_SUCCESS);
        EXPECT_EQ(nx_adc_group_submit_injected(&combined_port, &injection),
                  NX_ERROR_INVALID);
        EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_READY);
        EXPECT_EQ(adc[0].CR2 & ADC_CR2_JSWSTART, 0U);
    }
    ASSERT_NO_FATAL_FAILURE(StopCombined());
}
TEST_F(AdcGroupCombined, ColdSamplingConflictsAndFullPreemptionBudgetReject) {
    ASSERT_NE(+nx_stm32_adc_group_combined_prepare, nullptr);
    injected_channels[0] = channels[0];
    injected_samples[0] = 6U;
    EXPECT_EQ(nx_stm32_adc_group_combined_prepare(&combined, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(rcc.APB2ENR, 0U);
    EXPECT_EQ(combined.regular.config, nullptr);
    injected_channels[0] = 4U;
    injected_samples[0] = 7U;
    selected.regular.adc.period_us = 50U;
    selected.min_injected_interval_us = 52U;
    EXPECT_EQ(nx_stm32_adc_group_combined_prepare(&combined, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(rcc.APB2ENR, 0U);
    selected.regular.adc.period_us = 1000U;
    selected.min_injected_interval_us = 1000U;
    EXPECT_EQ(nx_stm32_adc_group_combined_prepare(&combined, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(rcc.APB2ENR, 0U);
}
TEST_F(AdcGroupCombined, LateJeocFromStoppedSessionCannotReopenAnyBorrow) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(Inject());
    ASSERT_NO_FATAL_FAILURE(StopCombined());
    EXPECT_EQ(injection.base.result, NX_ERROR_CANCELLED);
    for (unsigned i = 0U; i < Count(); ++i)
        adc[i].SR |= ADC_SR_JEOC;
    nx_stm32_adc_group_combined_irq(&combined_port);
    EXPECT_EQ(combined.regular.base.active, nullptr);
    EXPECT_EQ(combined.regular.stream, nullptr);
    EXPECT_EQ(memory.M0AR | memory.M1AR | memory.PAR, 0U);
    EXPECT_EQ(injection.base.result, NX_ERROR_CANCELLED);
}

TEST_F(AdcGroupCombined,
       ImmutableRegularFactsCannotLiveInTheCombinedPacingState) {
    ASSERT_NE(+nx_stm32_adc_group_combined_prepare, nullptr);
    combined.last_injected = UINT64_C(0x03020100);
    selected.regular.adc.channels =
        reinterpret_cast<const uint8_t*>(&combined.last_injected);
    EXPECT_EQ(nx_stm32_adc_group_combined_prepare(&combined, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(combined.regular.config, nullptr);
    EXPECT_EQ(rcc.APB2ENR, 0U);
    EXPECT_EQ(adc[0].CR2, 0U);
}
TEST_F(AdcGroupCombined, LosingTheEpochDuringJdrReadCannotPublishAValidFrame) {
    selected.injected_ranks = 2U;
    injected_channels[2] = 6U;
    injected_channels[3] = 7U;
    injected_samples[2] = injected_samples[3] = 7U;
    injected_converters[2] = 1U;
    injected_converters[3] = 2U;
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(Inject());
    ASSERT_NO_FATAL_FAILURE(HardwareJeoc(Count()));
    s_lose_domain_on_data_read = true;
    EXPECT_EQ(nx_adc_group_service(&combined_port), NX_ERROR_STATE);
    EXPECT_NE(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(injected_values[0] | injected_values[1] | injected_values[2] |
                  injected_values[3],
              0U);
    EXPECT_EQ(stream.outstanding, 2U);
    EXPECT_EQ(nx_adc_group_stop(&combined_port), NX_ERROR_STATE);
    /* The independent model ends with all borrows retained. No reset/clock
     * revival can recreate the lost conversion epoch or claim HIL recovery. */
}

TEST_F(AdcGroupCombined, DeadlineAtJeocEnableCannotStartASoftwareInjection) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(PrepareInjection(nx_time_now_us() + 50U));
    s_hold_dma = s_combined_expire_on_enable = s_combined_watch_start = true;
    ASSERT_EQ(nx_adc_group_submit_injected(&combined_port, &injection),
              NX_SUCCESS);
    EXPECT_FALSE(s_combined_saw_start);
    EXPECT_EQ(adc[0].CR2 & ADC_CR2_JSWSTART, 0U);
    EXPECT_NE(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(stream.outstanding, 2U);
    s_hold_dma = false;
    ASSERT_NO_FATAL_FAILURE(StopCombined());
    EXPECT_EQ(injection.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(injection.base.transferred, 0U);
}
} /* namespace */

namespace {
class AdcGroupTriggered : public AdcGroupCombined {
  protected:
    TIM_TypeDef injection_timer{};
    nx_stm32_adc_group_injected_timer_t external{};
    void SetUp() override {
        AdcGroupCombined::SetUp();
        external = {&injection_timer, 84000000U, 44U, 10U, 83U, 9U, 7U};
        selected.injected_timer = &external;
        s_injected_timer_model = &injection_timer;
        s_injected_rcc_model = &rcc;
        s_injected_counter_seen = s_injected_clock_seen = false;
        s_injected_refuse_stop = s_injected_refuse_clock_release = false;
    }
    void TearDown() override {
        s_injected_timer_model = nullptr;
        s_injected_rcc_model = nullptr;
        s_injected_refuse_stop = s_injected_refuse_clock_release = false;
        AdcGroupCombined::TearDown();
    }
    void HardwareExternalJeoc() {
        ASSERT_EQ(adc[0].CR2 & ADC_CR2_JSWSTART, 0U);
        ASSERT_EQ(adc[0].CR2 & ADC_CR2_JEXTEN, ADC_CR2_JEXTEN_0);
        ASSERT_EQ((adc[0].CR2 & ADC_CR2_JEXTSEL) >> ADC_CR2_JEXTSEL_Pos, 9U);
        ASSERT_NE(injection_timer.CR1 & TIM_CR1_CEN, 0U);
        ASSERT_NE(injection_timer.CR1 & TIM_CR1_OPM, 0U);
        /* One real update stops an OPM timer; it cannot issue another edge. */
        injection_timer.CR1 &= ~uint32_t(TIM_CR1_CEN);
        injection_timer.SR |= TIM_SR_UIF;
        for (unsigned i = 0U; i < Count(); ++i) {
            adc[i].SR |= ADC_SR_JSTRT | ADC_SR_JEOC;
            for (unsigned rank = 0U; rank < selected.injected_ranks; ++rank)
                (&adc[i].JDR1)[rank] =
                    0x100U * (i + 1U) + injected_channels[rank * Count() + i];
        }
        nx_stm32_adc_group_combined_irq(&combined_port);
    }
};
TEST_F(AdcGroupTriggered, InfoNamesTheSelectedOnePulseTimerAndClockLease) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    nx_adc_group_info_t info{};
    ASSERT_EQ(nx_adc_group_injected_info(&combined_port, &info), NX_SUCCESS);
    EXPECT_EQ(info.format.trigger, NX_ADC_TRIGGER_TIMER);
    EXPECT_EQ(info.format.trigger_identity, 44U);
    EXPECT_EQ(info.timer_period_us, 10U);
    EXPECT_NE(rcc.APB1ENR & RCC_APB1ENR_TIM4EN, 0U);
    EXPECT_EQ(injection_timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_NO_FATAL_FAILURE(StopCombined());
    EXPECT_EQ(rcc.APB1ENR & RCC_APB1ENR_TIM4EN, 0U);
}
TEST_F(AdcGroupTriggered, OneTimerEdgeCompletesInjectionWhileRegularDmaLives) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(Inject());
    ASSERT_NO_FATAL_FAILURE(HardwareExternalJeoc());
    EXPECT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(injection.base.result, NX_SUCCESS);
    EXPECT_EQ(injection.base.transferred, 1U);
    EXPECT_EQ(injected_values[0], 0x104U);
    EXPECT_EQ(injected_values[1], 0x205U);
    EXPECT_EQ(adc[0].CR2 & ADC_CR2_JEXTEN, 0U);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(stream.outstanding, 2U);
    ASSERT_NO_FATAL_FAILURE(StopCombined());
}
TEST_F(AdcGroupTriggered, RegularTimerCannotBeSilentlyReusedForInjection) {
    external.timer = &timer;
    external.trigger = 2U;
    EXPECT_EQ(nx_stm32_adc_group_combined_prepare(&combined, &selected),
              NX_ERROR_INVALID);
    EXPECT_EQ(combined.regular.config, nullptr);
    EXPECT_EQ(rcc.APB2ENR & (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN |
                             RCC_APB2ENR_ADC3EN),
              0U);
}
TEST_F(AdcGroupTriggered, CancelledInjectionStillWaitsForTheOneHardwareJeoc) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(Inject());
    ASSERT_EQ(nx_adc_group_cancel(&combined_port, &injection), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    ASSERT_NO_FATAL_FAILURE(HardwareExternalJeoc());
    EXPECT_EQ(nx_adc_group_service(&combined_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(injection.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(injection.base.transferred, 0U);
    EXPECT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_NO_FATAL_FAILURE(StopCombined());
}
} /* namespace */

namespace {
TEST_F(AdcGroupTriggered, LostInjectedTimerClockCannotBeRepairedNumerically) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(Inject());
    rcc.APB1ENR &= ~uint32_t(RCC_APB1ENR_TIM4EN);
    EXPECT_EQ(nx_adc_group_service(&combined_port), NX_ERROR_STATE);
    EXPECT_NE(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(stream.outstanding, 2U);
    rcc.APB1ENR |= RCC_APB1ENR_TIM4EN;
    EXPECT_EQ(nx_adc_group_stop(&combined_port), NX_ERROR_STATE);
    EXPECT_NE(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(stream.outstanding, 2U);
}
} /* namespace */

namespace {
TEST_F(AdcGroupTriggered, RefusedOnePulseStopRetainsBothOwnershipDomains) {
    ASSERT_NO_FATAL_FAILURE(ReadyCombined());
    ASSERT_NO_FATAL_FAILURE(Inject());
    s_injected_refuse_stop = true;
    EXPECT_EQ(nx_adc_group_stop(&combined_port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(stream.outstanding, 2U);
    EXPECT_NE(memory.M0AR, 0U);
    s_injected_refuse_stop = false;
    EXPECT_EQ(nx_adc_group_stop(&combined_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&injection.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(injection.base.result, NX_ERROR_IO);
    EXPECT_EQ(injection.base.transferred, 0U);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(nx_stm32_adc_group_combined_close(&combined), NX_SUCCESS);
}
TEST_F(AdcGroupTriggered, ExtraClockReleaseFailureRetainsItsActualColdReceipt) {
    ASSERT_NO_FATAL_FAILURE(PrepareCombined());
    s_injected_refuse_clock_release = true;
    EXPECT_EQ(nx_stm32_adc_group_combined_close(&combined), NX_ERROR_IO);
    EXPECT_NE(combined.regular.config, nullptr);
    EXPECT_NE(rcc.APB1ENR & RCC_APB1ENR_TIM4EN, 0U);
    s_injected_refuse_clock_release = false;
    EXPECT_EQ(nx_stm32_adc_group_combined_close(&combined), NX_SUCCESS);
    EXPECT_EQ(combined.regular.config, nullptr);
    EXPECT_EQ(rcc.APB1ENR & RCC_APB1ENR_TIM4EN, 0U);
}
TEST_F(AdcGroupTriggered, AnInheritedInjectionTimerGateIsNeverWithdrawn) {
    rcc.APB1ENR |= RCC_APB1ENR_TIM4EN;
    ASSERT_NO_FATAL_FAILURE(PrepareCombined());
    EXPECT_EQ(nx_stm32_adc_group_combined_close(&combined), NX_SUCCESS);
    EXPECT_NE(rcc.APB1ENR & RCC_APB1ENR_TIM4EN, 0U);
}
} /* namespace */

namespace {
TEST_F(AdcGroupTriggered, AForeignSlaveTriggerIsNeverAcquiredAsAnIdleTimer) {
    rcc.APB1ENR |= RCC_APB1ENR_TIM4EN;
    injection_timer.SMCR = TIM_SMCR_SMS_2 | TIM_SMCR_SMS_1;
    EXPECT_EQ(nx_stm32_adc_group_combined_prepare(&combined, &selected),
              NX_ERROR_BUSY);
    EXPECT_EQ(combined.regular.config, nullptr);
    EXPECT_EQ(injection_timer.SMCR, uint32_t(TIM_SMCR_SMS_2 | TIM_SMCR_SMS_1));
    EXPECT_EQ(injection_timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(rcc.APB2ENR & (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN |
                             RCC_APB2ENR_ADC3EN),
              0U);
}
} /* namespace */

extern "C" {
nx_result_t nx_adc_group_prepare_results(nx_adc_group_request_t*,
                                         nx_adc_group_results_t*, size_t,
                                         nx_time_us_t) __attribute__((weak));
}
namespace {
TEST_F(Stm32AdcGroup,
       PairedPreparationNamesBothSpansAndRawPreparationResetsTheTag) {
    ASSERT_NE(+nx_adc_group_prepare_results, nullptr);
    uint16_t injected[4]{};
    nx_adc_group_results_t results{{frames, 8U}, {injected, sizeof(injected)}};
    ASSERT_EQ(nx_adc_group_prepare_results(&request, &results, 1U, 5000U),
              NX_SUCCESS);
    EXPECT_EQ(request.results_kind, NX_ADC_GROUP_REGULAR_INJECTED_RESULTS);
    EXPECT_EQ(request.output.results, &results);
    EXPECT_EQ(request.bytes, sizeof(results));
    EXPECT_EQ(request.frame_count, 1U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(nx_adc_group_prepare(&request, frames, 4U, 1U, 6000U),
              NX_SUCCESS);
    EXPECT_EQ(request.results_kind, NX_ADC_GROUP_RAW_RESULTS);
    EXPECT_EQ(request.output.frames, frames);
}
TEST_F(Stm32AdcGroup,
       PairedDescriptorAliasesRejectWithoutPreparingEitherGroup) {
    ASSERT_NE(+nx_adc_group_prepare_results, nullptr);
    nx_adc_group_results_t results{{frames, 8U}, {frames, 8U}};
    const nx_adc_group_request_t before = request;
    EXPECT_EQ(nx_adc_group_prepare_results(&request, &results, 1U, 5000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&request, &before, sizeof(before)), 0);
    results.injected = {&results, sizeof(results)};
    EXPECT_EQ(nx_adc_group_prepare_results(&request, &results, 1U, 5000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&request, &before, sizeof(before)), 0);
}
TEST_F(Stm32AdcGroup, ASystemSpanIsNotAnAdmissibleDmaPayload) {
    region = {reinterpret_cast<uintptr_t>(&g_nx_stm32_system),
              sizeof(g_nx_stm32_system), NX_DMA_MEMORY_WRITE};
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    ASSERT_EQ(
        nx_adc_group_prepare(&request, &g_nx_stm32_system.rcc, 8U, 1U, 5000U),
        NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(g_nx_stm32_system.rcc, &rcc);
}
TEST_F(Stm32AdcGroup,
       RawFiniteProviderRejectsAPairedTagBeforeAcceptingStorage) {
    ASSERT_EQ(nx_stm32_adc_group_prepare(&state, &config), NX_SUCCESS);
    PrepareRequest(1U, 8U);
    request.results_kind = NX_ADC_GROUP_REGULAR_INJECTED_RESULTS;
    EXPECT_EQ(nx_stm32_adc_group_ops.submit(&state, &request),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
}
} /* namespace */

extern "C" {
#include "stm32f407_adc_group_automatic.h"
nx_result_t nx_stm32_adc_group_automatic_prepare(
    nx_stm32_adc_group_automatic_state_t*,
    const nx_stm32_adc_group_combined_config_t*) __attribute__((weak));
nx_result_t
nx_stm32_adc_group_automatic_close(nx_stm32_adc_group_automatic_state_t*)
    __attribute__((weak));
void nx_stm32_adc_group_automatic_irq(const nx_adc_group_port_t*)
    __attribute__((weak));
extern const nx_adc_group_ops_t nx_stm32_adc_group_automatic_ops
    __attribute__((weak));
}
namespace {
class AdcGroupAutomatic : public AdcGroupCombined {
  protected:
    nx_stm32_adc_group_automatic_state_t automatic{};
    nx_adc_group_port_t automatic_port{&nx_stm32_adc_group_automatic_ops,
                                       &automatic};
    nx_adc_group_results_t results{};
    nx_adc_group_request_t joint{};
    void SetUp() override {
        AdcGroupCombined::SetUp();
        selected.regular.max_service_gap_us = 0U;
        selected.min_injected_interval_us = 0U;
        block_regions[1] = {reinterpret_cast<uintptr_t>(&automatic),
                            sizeof(automatic), NX_DMA_MEMORY_WRITE};
        results = {{slots[0].data, 8U}, {injected_values, 4U}};
        nx_request_initialize(&joint.base);
    }
    void PrepareAutomatic() {
        ASSERT_NE(+nx_stm32_adc_group_automatic_prepare, nullptr);
        ASSERT_NE(+nx_stm32_adc_group_automatic_close, nullptr);
        ASSERT_NE(+nx_stm32_adc_group_automatic_irq, nullptr);
        ASSERT_NE(&nx_stm32_adc_group_automatic_ops, nullptr);
        ASSERT_EQ(nx_stm32_adc_group_automatic_prepare(&automatic, &selected),
                  NX_SUCCESS);
    }
    void StartAutomatic(nx_time_us_t deadline = 5000U) {
        ASSERT_NO_FATAL_FAILURE(PrepareAutomatic());
        ASSERT_EQ(nx_adc_group_prepare_results(&joint, &results, 1U, deadline),
                  NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_submit(&automatic_port, &joint), NX_SUCCESS);
        ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
        for (unsigned i = 0U; i < Count(); ++i) {
            ASSERT_NE(adc[i].CR1 & ADC_CR1_JAUTO, 0U);
            ASSERT_EQ(adc[i].CR2 & (ADC_CR2_CONT | ADC_CR2_JEXTEN), 0U);
        }
        ASSERT_NE(timer.CR1 & TIM_CR1_OPM, 0U);
    }
    /** Real normal DMA stores the one regular sequence before reporting TC. */
    void HardwareAutomaticRegular(bool irq = true) {
        ASSERT_NE(timer.CR1 & TIM_CR1_CEN, 0U);
        ASSERT_NE(memory.CR & DMA_SxCR_EN, 0U);
        ASSERT_EQ(memory.M0AR,
                  reinterpret_cast<uintptr_t>(results.regular.data));
        ASSERT_EQ(memory.NDTR,
                  results.regular.bytes / (Count() == 3U ? 2U : 4U));
        auto* target = reinterpret_cast<uint16_t*>(uintptr_t(memory.M0AR));
        for (unsigned rank = 0U; rank < selected.regular.adc.ranks; ++rank)
            for (unsigned i = 0U; i < Count(); ++i) {
                const uint32_t channel = (adc[i].SQR3 >> (rank * 5U)) & 31U;
                target[rank * Count() + i] =
                    static_cast<uint16_t>(0x100U * (i + 1U) + channel);
            }
        timer.CR1 &= ~uint32_t(TIM_CR1_CEN);
        memory.NDTR = 0U;
        memory.CR &= ~uint32_t(DMA_SxCR_EN);
        dma.LISR |= DMA_LISR_TCIF0;
        if (irq)
            nx_stm32_adc_group_automatic_irq(&automatic_port);
    }
    /** JAUTO follows regular conversion; it does not need JSWSTART/JEXTEN. */
    void HardwareAutomaticJeoc(unsigned completed) {
        ASSERT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
        for (unsigned i = 0U; i < completed; ++i) {
            ASSERT_NE(adc[i].CR1 & ADC_CR1_JAUTO, 0U);
            ASSERT_NE(adc[i].CR2 & ADC_CR2_ADON, 0U);
            ASSERT_EQ(adc[i].CR2 & (ADC_CR2_JSWSTART | ADC_CR2_JEXTEN), 0U);
            for (unsigned rank = 0U; rank < selected.injected_ranks; ++rank) {
                const unsigned shift =
                    (rank + 4U - selected.injected_ranks) * 5U;
                const uint32_t channel = (adc[i].JSQR >> shift) & 31U;
                *(&adc[i].JDR1 + rank) = 0x300U + 0x100U * i + channel;
            }
            adc[i].SR |= ADC_SR_JEOC;
        }
        nx_stm32_adc_group_automatic_irq(&automatic_port);
    }
    void CloseAutomatic() {
        ASSERT_EQ(nx_adc_group_stop(&automatic_port), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_adc_group_automatic_close(&automatic), NX_SUCCESS);
    }
};
TEST_F(AdcGroupAutomatic, AnArmedForeignSlaveTimerIsNeverAnIdleSource) {
    ASSERT_NE(+nx_stm32_adc_group_automatic_prepare, nullptr);
    timer.SMCR = TIM_SMCR_SMS_2 | TIM_SMCR_SMS_1;
    EXPECT_EQ(nx_stm32_adc_group_automatic_prepare(&automatic, &selected),
              NX_ERROR_BUSY);
    EXPECT_EQ(automatic.base.config, nullptr);
    EXPECT_EQ(automatic.config, nullptr);
    EXPECT_EQ(timer.SMCR, uint32_t(TIM_SMCR_SMS_2 | TIM_SMCR_SMS_1));
    EXPECT_EQ(rcc.APB2ENR & (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN |
                             RCC_APB2ENR_ADC3EN),
              0U);
}
TEST_F(AdcGroupAutomatic, BothStaticLayoutsBelongToOneFiniteOnePulseOwner) {
    ASSERT_NO_FATAL_FAILURE(PrepareAutomatic());
    nx_adc_group_info_t regular{}, injected{};
    ASSERT_EQ(nx_adc_group_info(&automatic_port, &regular), NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_injected_info(&automatic_port, &injected),
              NX_SUCCESS);
    EXPECT_EQ(regular.max_frames, 1U);
    EXPECT_EQ(injected.max_frames, 1U);
    EXPECT_EQ(regular.format.scan_stride, 8U);
    EXPECT_EQ(injected.format.scan_stride, 4U);
    EXPECT_EQ(injected.format.group, NX_ADC_INJECTED);
    EXPECT_EQ(injected.format.trigger, NX_ADC_TRIGGER_EVENT);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(adc[0].CR2 | adc[1].CR2, 0U);
    ASSERT_NO_FATAL_FAILURE(CloseAutomatic());
}
TEST_F(AdcGroupAutomatic, RegularTcCannotAbortItsPendingAutomaticInjection) {
    ASSERT_NO_FATAL_FAILURE(StartAutomatic());
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticRegular());
    EXPECT_NE(adc[0].CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&joint.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(automatic.base.active, &joint);
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticJeoc(1U));
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&joint.base), NX_REQUEST_ACTIVE);
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticJeoc(2U));
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&joint.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(joint.base.result, NX_SUCCESS);
    EXPECT_EQ(joint.base.transferred, 1U);
    EXPECT_EQ(injected_values[0], 0x304U);
    EXPECT_EQ(injected_values[1], 0x405U);
    EXPECT_EQ(memory.M0AR | memory.PAR, 0U);
    EXPECT_EQ(automatic.base.active, nullptr);
    ASSERT_NO_FATAL_FAILURE(CloseAutomatic());
}
TEST_F(AdcGroupAutomatic, ALongDmaWriteDoesNotReleaseAtJeocAlone) {
    ASSERT_NO_FATAL_FAILURE(StartAutomatic());
    timer.CR1 &= ~uint32_t(TIM_CR1_CEN);
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticJeoc(2U));
    EXPECT_NE(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&joint.base), NX_REQUEST_ACTIVE);
    memory.NDTR = 0U;
    memory.CR &= ~uint32_t(DMA_SxCR_EN);
    dma.LISR |= DMA_LISR_TCIF0;
    nx_stm32_adc_group_automatic_irq(&automatic_port);
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    EXPECT_EQ(joint.base.result, NX_SUCCESS);
    EXPECT_EQ(joint.base.transferred, 1U);
    ASSERT_NO_FATAL_FAILURE(CloseAutomatic());
}
TEST_F(AdcGroupAutomatic,
       RefusedStopRetainsTheJointDescriptorAndBothCapacities) {
    ASSERT_NO_FATAL_FAILURE(StartAutomatic(200U));
    s_hold_dma = true;
    EXPECT_EQ(nx_adc_group_cancel(&automatic_port, &joint), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_ERROR_BUSY);
    EXPECT_EQ(automatic.base.active, &joint);
    EXPECT_EQ(memory.M0AR, reinterpret_cast<uintptr_t>(results.regular.data));
    EXPECT_EQ(nx_adc_group_prepare_results(&joint, &results, 1U, 5000U),
              NX_ERROR_STATE);
    ASSERT_EQ(nx_native_clock_advance(150U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&joint.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(automatic.base.active, &joint);
    s_hold_dma = false;
    EXPECT_EQ(nx_adc_group_stop(&automatic_port), NX_SUCCESS);
    EXPECT_EQ(joint.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(joint.base.transferred, 0U);
    EXPECT_EQ(automatic.base.active, nullptr);
    ASSERT_NO_FATAL_FAILURE(CloseAutomatic());
}
TEST_F(AdcGroupAutomatic, TheSecondCapacityCannotCoverItsSelectedOwner) {
    ASSERT_NO_FATAL_FAILURE(PrepareAutomatic());
    results.injected.data = &automatic.config;
    ASSERT_EQ(nx_adc_group_prepare_results(&joint, &results, 1U, 5000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_submit(&automatic_port, &joint), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&joint.base), NX_REQUEST_READY);
    EXPECT_EQ(automatic.base.active, nullptr);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    ASSERT_NO_FATAL_FAILURE(CloseAutomatic());
}
TEST_F(AdcGroupAutomatic, OnePulseDelayIsNotAnInventedPeriodicRateBudget) {
    selected.regular.adc.period_us = 1U;
    ASSERT_NO_FATAL_FAILURE(StartAutomatic());
    EXPECT_EQ(timer.PSC, 0U);
    EXPECT_EQ(timer.ARR, 83U);
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticRegular());
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticJeoc(2U));
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    EXPECT_EQ(joint.base.result, NX_SUCCESS);
    EXPECT_EQ(joint.base.transferred, 1U);
    ASSERT_NO_FATAL_FAILURE(CloseAutomatic());
}
TEST_F(AdcGroupAutomatic, TripleJointResultsKeepTheirNativeHalfwordOrder) {
    selected.regular.adc.mode = NX_STM32_ADC_TRIPLE_REGULAR;
    selected.regular.adc.ranks = 1U;
    channels[0] = 0U;
    channels[1] = 1U;
    channels[2] = 2U;
    converters[0] = 1U;
    converters[1] = 2U;
    converters[2] = 3U;
    injected_channels[0] = 4U;
    injected_channels[1] = 5U;
    injected_channels[2] = 0U;
    injected_samples[2] = 7U;
    injected_converters[2] = 3U;
    results.regular.bytes = results.injected.bytes = 6U;
    ASSERT_NO_FATAL_FAILURE(StartAutomatic());
    EXPECT_EQ(memory.NDTR, 3U);
    EXPECT_EQ(memory.CR & (DMA_SxCR_MSIZE | DMA_SxCR_PSIZE),
              uint32_t(DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0));
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticRegular());
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticJeoc(3U));
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    EXPECT_EQ(joint.base.result, NX_SUCCESS);
    EXPECT_EQ(joint.base.transferred, 1U);
    auto* regular = static_cast<uint16_t*>(results.regular.data);
    EXPECT_EQ(regular[0], 0x100U);
    EXPECT_EQ(regular[1], 0x201U);
    EXPECT_EQ(regular[2], 0x302U);
    EXPECT_EQ(injected_values[0], 0x304U);
    EXPECT_EQ(injected_values[1], 0x405U);
    EXPECT_EQ(injected_values[2], 0x500U);
    ASSERT_NO_FATAL_FAILURE(CloseAutomatic());
}
TEST_F(AdcGroupAutomatic, ExpiryDuringDmaArmCannotStartTheOnePulse) {
    ASSERT_NO_FATAL_FAILURE(PrepareAutomatic());
    ASSERT_EQ(nx_adc_group_prepare_results(&joint, &results, 1U, 150U),
              NX_SUCCESS);
    ASSERT_EQ(nx_adc_group_submit(&automatic_port, &joint), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
    s_automatic_expire_on_arm = true;
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    EXPECT_FALSE(s_automatic_saw_late_source);
    EXPECT_EQ(nx_request_state(&joint.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(joint.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(joint.base.transferred, 0U);
    EXPECT_EQ(memory.M0AR | memory.PAR, 0U);
    ASSERT_NO_FATAL_FAILURE(CloseAutomatic());
}
TEST_F(AdcGroupAutomatic,
       RefusedOldJeocClearCannotCompleteTheNextJointRequest) {
    ASSERT_NO_FATAL_FAILURE(StartAutomatic());
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticRegular());
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticJeoc(2U));
    ASSERT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    ASSERT_EQ(joint.base.result, NX_SUCCESS);
    ASSERT_NE(adc[0].SR & ADC_SR_JEOC, 0U);
    ASSERT_EQ(nx_adc_group_prepare_results(&joint, &results, 1U, 5000U),
              NX_SUCCESS);
    /* rc_w0 refusal preserves old JEOC at the MMIO barrier; this is not a
     * W1C acknowledgement or a fabricated new conversion. */
    s_automatic_refuse_jeoc_clear = true;
    ASSERT_EQ(nx_adc_group_submit(&automatic_port, &joint), NX_SUCCESS);
    EXPECT_EQ(adc[0].CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&joint.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(joint.base.result, NX_ERROR_IO);
    EXPECT_EQ(joint.base.transferred, 0U);
    EXPECT_EQ(memory.M0AR | memory.PAR, 0U);
    s_automatic_refuse_jeoc_clear = false;
    ASSERT_NO_FATAL_FAILURE(CloseAutomatic());
}
TEST_F(AdcGroupAutomatic, ACoobservedDmaFaultInvalidatesBothResults) {
    ASSERT_NO_FATAL_FAILURE(StartAutomatic());
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticRegular(false));
    dma.LISR |= DMA_LISR_TEIF0;
    ASSERT_NO_FATAL_FAILURE(HardwareAutomaticJeoc(2U));
    EXPECT_EQ(nx_adc_group_service(&automatic_port), NX_SUCCESS);
    EXPECT_EQ(joint.base.result, NX_ERROR_IO);
    EXPECT_EQ(joint.base.transferred, 0U);
    EXPECT_EQ(injected_values[0], 0U);
    ASSERT_NO_FATAL_FAILURE(CloseAutomatic());
}
} /* namespace */

namespace {
class AdcGroupInterleaved : public AdcGroupBlocks {
  protected:
    void* sample_memory = MAP_FAILED;
    void SetUp() override {
        AdcGroupBlocks::SetUp();
        sample_memory = mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
        ASSERT_NE(sample_memory, MAP_FAILED);
        block_regions[0] = {reinterpret_cast<uintptr_t>(sample_memory), 4096U,
                            NX_DMA_MEMORY_WRITE};
        s_hardware = &block_config.adc;
        Configure(false, false);
    }
    void TearDown() override {
        if (sample_memory != MAP_FAILED) {
            EXPECT_EQ(munmap(sample_memory, 4096U), 0);
        }
        AdcGroupBlocks::TearDown();
    }
    void Configure(bool triple, bool bytes) {
        block_config.adc.mode = triple ? NX_STM32_ADC_TRIPLE_INTERLEAVED
                                       : NX_STM32_ADC_DUAL_INTERLEAVED;
        block_config.adc.ranks = 1U;
        block_config.adc.resolution = bytes ? 2U : 0U;
        block_config.adc.timer = nullptr;
        block_config.adc.period_us = block_config.adc.trigger_identity = 0U;
        const size_t capacity = (triple ? 768U : 512U) / (bytes ? 2U : 1U);
        for (size_t i = 0U; i < 6U; ++i) {
            channels[i] = 0U;
            samples[i] = 7U;
            converters[i] = static_cast<uint8_t>(i % (triple ? 3U : 2U) + 1U);
        }
        for (size_t i = 0U; i < 5U; ++i) {
            slots[i].data = static_cast<uint8_t*>(sample_memory) + i * 768U;
            slots[i].capacity = capacity;
        }
        ASSERT_EQ(nx_stream_initialize(&stream, slots, 5U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_observer_initialize(&observer, records, 5U),
                  NX_SUCCESS);
    }
    unsigned Adcs() const {
        return block_config.adc.mode == NX_STM32_ADC_TRIPLE_INTERLEAVED ? 3U
                                                                        : 2U;
    }
    bool ByteSamples() const {
        return block_config.adc.resolution >= 2U;
    }
    void ReadyInterleaved() {
        ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
        ASSERT_EQ(nx_adc_group_start_blocks(&block_port, &stream), NX_SUCCESS);
        ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
        for (unsigned i = 0U; i < Adcs(); ++i) {
            ASSERT_NE(adc[i].CR2 & ADC_CR2_ADON, 0U);
            ASSERT_NE(adc[i].CR2 & ADC_CR2_CONT, 0U);
            ASSERT_EQ(adc[i].CR2 & ADC_CR2_EXTEN, 0U);
        }
        ASSERT_NE(adc[0].CR2 & ADC_CR2_SWSTART, 0U);
        ASSERT_EQ(timer.CR1 | timer.CR2 | timer.SMCR, 0U);
    }
    /** CDR pairs preserve the documented1,2 /3,1 /2,3 cycle; this is a
     * register/ownership model, not an aperture-accurate analog simulation. */
    void HardwareBank() {
        const unsigned width = ByteSamples() ? 2U : 4U;
        const uint32_t words = static_cast<uint32_t>(slots[0].capacity / width);
        unsigned converter = 0U;
        nx_time_us_t since_service = 0U;
        adc[0].CR2 &= ~uint32_t(ADC_CR2_SWSTART);
        for (uint32_t word = 0U; word < words; ++word) {
            if (since_service + 47U > block_config.max_service_gap_us) {
                ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
                since_service = 0U;
            }
            ASSERT_EQ(nx_native_clock_advance(47U), NX_SUCCESS);
            since_service += 47U;
            const unsigned bank = (memory.CR & DMA_SxCR_CT) != 0U ? 1U : 0U;
            const uintptr_t target = bank != 0U ? memory.M1AR : memory.M0AR;
            ASSERT_NE(memory.CR & DMA_SxCR_EN, 0U);
            const uint32_t first = ++converter;
            if (converter == Adcs())
                converter = 0U;
            const uint32_t second = ++converter;
            if (converter == Adcs())
                converter = 0U;
            if (ByteSamples()) {
                common.CDR = (second * 0x11U << 8U) | first * 0x11U;
                reinterpret_cast<uint16_t*>(target)[word] =
                    static_cast<uint16_t>(common.CDR);
            } else {
                common.CDR = (second * 0x101U << 16U) | first * 0x101U;
                reinterpret_cast<uint32_t*>(target)[word] = common.CDR;
            }
            --memory.NDTR;
        }
        ASSERT_EQ(memory.NDTR, 0U);
        memory.NDTR = words;
        memory.CR ^= DMA_SxCR_CT;
        dma.LISR |= DMA_LISR_TCIF0;
        nx_stm32_adc_group_blocks_irq(&block_port);
    }
};
TEST_F(AdcGroupInterleaved,
       BothInterleaveWidthsStartEveryParticipatingConverter) {
    for (bool triple : {false, true})
        for (bool bytes : {false, true}) {
            SCOPED_TRACE(testing::Message()
                         << "triple=" << triple << ",bytes=" << bytes);
            ASSERT_NO_FATAL_FAILURE(Configure(triple, bytes));
            ASSERT_NO_FATAL_FAILURE(ReadyInterleaved());
            nx_adc_group_info_t info{};
            ASSERT_EQ(nx_adc_group_info(&block_port, &info), NX_SUCCESS);
            EXPECT_EQ(info.format.channel_count, triple ? 6U : 2U);
            EXPECT_EQ(info.format.sample_stride, bytes ? 1U : 2U);
            EXPECT_EQ(info.buffer_alignment, bytes ? 2U : 4U);
            EXPECT_EQ(info.format.trigger, NX_ADC_TRIGGER_SOFTWARE);
            EXPECT_EQ(info.physical_scan_halfcycles, 0U);
            EXPECT_EQ(memory.NDTR, slots[0].capacity / (bytes ? 2U : 4U));
            EXPECT_EQ(memory.CR & (DMA_SxCR_PSIZE | DMA_SxCR_MSIZE),
                      bytes ? uint32_t(DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0)
                            : uint32_t(DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1));
            ASSERT_NO_FATAL_FAILURE(StopClose());
        }
}
TEST_F(AdcGroupInterleaved,
       TripleBlocksKeepFullSixSampleCyclesAndObservations) {
    for (bool bytes : {false, true}) {
        ASSERT_NO_FATAL_FAILURE(Configure(true, bytes));
        ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
        ASSERT_EQ(nx_adc_group_attach_observer(&block_port, &observer),
                  NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_start_blocks(&block_port, &stream), NX_SUCCESS);
        ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
        ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
        ASSERT_NO_FATAL_FAILURE(HardwareBank());
        ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
        nx_stream_block_t block{};
        ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
        EXPECT_EQ(block.length, slots[0].capacity);
        for (size_t i = 0U; i < block.length / (bytes ? 1U : 2U); ++i) {
            const uint32_t expected =
                static_cast<uint32_t>(i % 3U + 1U) * (bytes ? 0x11U : 0x101U);
            EXPECT_EQ(bytes ? block.data[i]
                            : reinterpret_cast<const uint16_t*>(block.data)[i],
                      expected);
        }
        nx_adc_observation_t observation{};
        ASSERT_EQ(
            nx_adc_group_block_observation(&block_port, &block, &observation),
            NX_SUCCESS);
        EXPECT_EQ(observation.epoch, block.epoch);
        EXPECT_EQ(observation.sequence, 1U);
        EXPECT_EQ(observation.observed_at, nx_time_now_us());
        ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
        ASSERT_NO_FATAL_FAILURE(StopClose());
    }
}
TEST_F(AdcGroupInterleaved,
       TooSmallBanksAndNonFramesNeverAcquireProducerLoans) {
    ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
    for (auto& slot : slots)
        slot.capacity = 8U;
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 5U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_start_blocks(&block_port, &stream),
              NX_ERROR_INVALID);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    ASSERT_NO_FATAL_FAILURE(StopClose());
    ASSERT_NO_FATAL_FAILURE(Configure(true, false));
    for (auto& slot : slots)
        slot.capacity = 764U;
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 5U), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
    EXPECT_EQ(nx_adc_group_start_blocks(&block_port, &stream),
              NX_ERROR_INVALID);
    EXPECT_EQ(stream.outstanding, 0U);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}
TEST_F(AdcGroupInterleaved,
       ConsumerPauseRestartsFromIndependentWithADiscontinuity) {
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 3U), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(ReadyInterleaved());
    ASSERT_NO_FATAL_FAILURE(HardwareBank());
    ASSERT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
    nx_stream_block_t block{};
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(HardwareBank());
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_ERROR_BUSY);
    EXPECT_EQ(common.CCR & ADC_CCR_MULTI, 0U);
    EXPECT_EQ(memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(stream.outstanding, 1U);
    EXPECT_NE(stream.losses, 0U);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_ERROR_BUSY);
    EXPECT_NE(common.CCR & ADC_CCR_MULTI, 0U);
    ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
    EXPECT_NE(adc[0].CR2 & ADC_CR2_CONT, 0U);
    EXPECT_NE(adc[1].CR2 & ADC_CR2_CONT, 0U);
    ASSERT_NO_FATAL_FAILURE(HardwareBank());
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
    ASSERT_EQ(nx_stream_acquire(&stream, &block), NX_SUCCESS);
    EXPECT_NE(block.lost_blocks, 0U);
    EXPECT_NE(block.flags & NX_STREAM_BOUNDARY_LOSS, 0U);
    EXPECT_EQ(block.flags & NX_STREAM_BOUNDARY_TRIGGER, 0U);
    ASSERT_EQ(nx_stream_release(&stream, &block), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}
TEST_F(AdcGroupInterleaved, RefusedDmaGeometryCannotStartAnAcceptedSession) {
    for (bool fifo : {false, true}) {
        ASSERT_NO_FATAL_FAILURE(Configure(false, true));
        ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
        ASSERT_EQ(nx_adc_group_start_blocks(&block_port, &stream), NX_SUCCESS);
        ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
        s_interleaved_refuse_width = !fifo;
        s_interleaved_refuse_direct = fifo;
        s_interleaved_saw_refused_source = false;
        EXPECT_EQ(nx_adc_group_service(&block_port), NX_ERROR_IO);
        EXPECT_FALSE(s_interleaved_saw_refused_source);
        EXPECT_EQ(adc[0].CR2 & ADC_CR2_CONT, 0U);
        EXPECT_EQ(stream.outstanding, 2U);
        s_interleaved_refuse_width = s_interleaved_refuse_direct = false;
        ASSERT_NO_FATAL_FAILURE(StopClose());
    }
}
TEST_F(AdcGroupInterleaved, PreemptedSoftwareStartCannotHideAnUnobservedWrap) {
    ASSERT_NO_FATAL_FAILURE(PrepareBlocks());
    ASSERT_EQ(nx_adc_group_start_blocks(&block_port, &stream), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(4U), NX_SUCCESS);
    s_interleaved_jump_start = true;
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_SUCCESS);
    EXPECT_EQ(nx_adc_group_service(&block_port), NX_ERROR_IO);
    nx_stream_block_t block{};
    EXPECT_EQ(nx_stream_acquire(&stream, &block), NX_ERROR_EMPTY);
    EXPECT_EQ(adc[0].CR2 & ADC_CR2_CONT, 0U);
    ASSERT_NO_FATAL_FAILURE(StopClose());
}
} /* namespace */
