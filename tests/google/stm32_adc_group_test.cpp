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
extern "C" void nx_stm32_dma_model_disable(void* raw, uint32_t mask) {
    auto* memory = static_cast<DMA_Stream_TypeDef*>(raw);
    memory->CR &= ~(mask & ~(s_hold_dma ? uint32_t(DMA_SxCR_EN) : 0U));
}
extern "C" void nx_stm32_adc_group_model_barrier(void) {
    nx_arch_dsb();
    if (s_hardware == nullptr)
        return;
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
}
extern "C" void nx_stm32_adc_group_model_ack(void* raw, uint32_t flags) {
    auto* dma = static_cast<DMA_TypeDef*>(raw);
    dma->LIFCR = flags;
    dma->LISR &= ~flags;
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
    request.frames = &state;
    EXPECT_EQ(nx_adc_group_submit(&port, &request), NX_ERROR_INVALID);
    request.frames = frames;
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
