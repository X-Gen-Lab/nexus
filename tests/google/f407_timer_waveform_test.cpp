/**
 * \file            f407_timer_waveform_test.cpp
 * \brief           Real coherent timer updates and native DMA at MMIO leaves
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_timer_waveform_model.h"
extern "C" {
#include "stm32f407_timer_waveform.h"
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" {
nx_stm32_system_t g_nx_stm32_system;
}
namespace {
struct Hardware {
    RCC_TypeDef rcc = {};
    TIM_TypeDef timer = {}, core = {}, master = {};
    GPIO_TypeDef gpio = {};
    DMA_TypeDef dma = {};
    DMA_Stream_TypeDef stream = {};
    uint32_t enabled[82] = {}, pending[82] = {}, active[82] = {};
    uint32_t priority[82] = {};
    uint32_t active_ccr[4] = {};
    uint32_t masks[3] = {};
    nx_time_us_t now = 0U;
    volatile uint32_t* refused = nullptr;
    uint32_t refused_mask = UINT32_MAX;
    unsigned writes = 0U, compare_writes = 0U, shared_resets = 0U;
    bool inject_update = false, partial_active = false;
    bool break_on_compare = false, hold_irq = false;
    bool complete_on_enable = false, hold_update_clear = false;
    bool error_on_dma_stop = false;
    unsigned unavailable_writes = 0U;
    unsigned nvic_writes = 0U, advanced_resets = 0U;
    unsigned available_channels = 4U;
    bool has_master = true, has_slave = true;
    uint8_t* dma_memory = nullptr;
    size_t dma_capacity = 0U, dma_position = 0U;
    uint8_t dma_lane = 2U;
    unsigned enable_attempts = 0U;
    bool privileged = true, in_isr = false;
    void update() {
        if ((timer.CR1 & TIM_CR1_UDIS) != 0U)
            return;
        const uint32_t previous[4] = {active_ccr[0], active_ccr[1],
                                      active_ccr[2], active_ccr[3]};
        active_ccr[0] = timer.CCR1;
        active_ccr[1] = timer.CCR2;
        active_ccr[2] = timer.CCR3;
        active_ccr[3] = timer.CCR4;
        unsigned changed = 0U;
        for (unsigned i = 0U; i < 4U; ++i)
            changed += previous[i] != active_ccr[i] ? 1U : 0U;
        if (changed != 0U && changed != 4U)
            partial_active = true;
        timer.SR |= TIM_SR_UIF;
        timer.CNT = 0U;
        if ((timer.CR1 & TIM_CR1_OPM) != 0U)
            timer.CR1 &= ~static_cast<uint32_t>(TIM_CR1_CEN);
        if ((timer.DIER & TIM_DIER_UDE) != 0U &&
            (stream.CR & DMA_SxCR_EN) != 0U && stream.NDTR != 0U) {
            const unsigned bits =
                static_cast<unsigned>((stream.CR & DMA_SxCR_MSIZE) >> 13U);
            const size_t width = size_t{1U} << bits;
            const unsigned count = ((timer.DCR >> 8U) & 31U) + 1U;
            const unsigned first = timer.DCR & 31U;
            for (unsigned i = 0U; i < count && stream.NDTR != 0U; ++i) {
                const uint32_t address =
                    stream.M0AR + static_cast<uint32_t>(dma_position);
                const uint32_t start = static_cast<uint32_t>(
                    reinterpret_cast<uintptr_t>(dma_memory));
                const size_t offset = address - start;
                if (offset > dma_capacity || width > dma_capacity - offset)
                    break;
                uint32_t value = 0U;
                std::memcpy(&value, dma_memory + offset, width);
                auto* target = reinterpret_cast<volatile uint32_t*>(
                    reinterpret_cast<uint8_t*>(&timer) + (first + i) * 4U);
                *target = value;
                dma_position += width;
                --stream.NDTR;
            }
            if (stream.NDTR == 0U) {
                stream.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
                dma_flags(1U << 5U);
            }
        }
    }
    void dma_flags(uint32_t flags) {
        const unsigned shifts[4] = {0U, 6U, 16U, 22U};
        if (dma_lane < 4U)
            dma.LISR |= flags << shifts[dma_lane & 3U];
        else
            dma.HISR |= flags << shifts[dma_lane & 3U];
    }
    void trigger(bool high) {
        const uint32_t mode = timer.SMCR & TIM_SMCR_SMS;
        if (high && mode == 6U)
            timer.CR1 |= TIM_CR1_CEN;
        if (high && mode == 4U) {
            timer.CNT = 0U;
            update();
        }
    }
} * hw;

class TimerWaveform : public testing::Test {
  protected:
    Hardware hardware;
    nx_stm32_waveform_state_t owner = {};
    nx_stm32_waveform_config_t config = {};
    nx_pwm_group_t group = {&nx_stm32_pwm_group_ops, &owner};
    nx_stm32_av_pin_t pins[8] = {};
    uint16_t snapshots[8] = {};
    alignas(8) uint32_t pool[128] = {};
    nx_dma_memory_region_t region = {};
    uint32_t duties[4] = {100U, 200U, 300U, 400U};
    nx_pwm_group_frame_t frame = {duties, 4U, 0U};
    nx_timer_pulse_t pulse = {&nx_stm32_timer_pulse_ops, &owner};
    nx_timer_pulse_request_t pulse_request = {};
    nx_timer_sequence_t sequence = {&nx_stm32_timer_sequence_ops, &owner};
    nx_timer_sequence_request_t sequence_request = {};
    nx_stream_slot_t slot = {};
    nx_stream_t stream = {};
    nx_frame_stream_t source = {};
    nx_frame_dma_loan_t loan = {};
    void SetUp() override {
        hw = &hardware;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &hardware.rcc;
        g_nx_stm32_system.timer = &hardware.core;
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.timer_error = NX_SUCCESS;
        g_nx_stm32_system.core_hz = NEXUS_CORE_HZ;
        g_nx_stm32_system.time_epoch = 1U;
        hardware.core.CR1 = TIM_CR1_CEN;
        hardware.core.DIER = TIM_DIER_UIE;
        hardware.core.PSC = 83U;
        hardware.core.ARR = UINT32_MAX;
        hardware.rcc.PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        hardware.rcc.CFGR =
            NEXUS_CLOCK_CFGR | RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
        hardware.rcc.CR =
            RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        hardware.rcc.APB1ENR = RCC_APB1ENR_TIM2EN;
        hardware.rcc.AHB1ENR =
            RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMA2EN;
        for (unsigned i = 0U; i < 8U; ++i)
            pins[i] = {&hardware.gpio, &snapshots[i], 0U,
                       static_cast<uint8_t>(i), 2U};
        region = {reinterpret_cast<uintptr_t>(pool), sizeof(pool),
                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        config.common = {pins, 4U, &region, 1U, 32U, 5U};
        config.registers = &hardware.timer;
        config.controller = 3U;
        config.kind = NX_STM32_WAVE_GROUP;
        config.channels = 15U;
        config.timer_hz = NEXUS_APB1_TIMER_HZ;
        config.tick_hz = 1000000U;
        config.period_ticks = 1000U;
        config.irq = TIM3_IRQn;
        config.break_input = &hardware.gpio.IDR;
        config.break_mask = 1U << 12U;
        config.break_active_high = true;
        nx_request_initialize(&pulse_request.base);
        ASSERT_EQ(nx_request_prepare(&pulse_request.base, NX_DEADLINE_NEVER),
                  NX_SUCCESS);
        pulse_request.width_ticks = duties;
        pulse_request.channel_count = 4U;
        hardware.dma_memory = reinterpret_cast<uint8_t*>(pool);
        hardware.dma_capacity = sizeof(pool);
    }
    void prepare() {
        ASSERT_EQ(nx_stm32_waveform_prepare(&owner, &config), NX_SUCCESS);
    }
    void advanced() {
        config.controller = 1U;
        config.channels = 7U;
        config.timer_hz = NEXUS_APB2_TIMER_HZ;
        config.irq = TIM1_BRK_TIM9_IRQn;
        config.complementary = true;
        config.internal_trigger = true;
        config.common.pin_count = 7U;
        config.deadtime_ticks = 80U;
        pins[6].pin = 12U;
        for (auto& pin : pins)
            pin.af = 1U;
        frame.channel_count = 3U;
        frame.trigger_ticks = 500U;
    }
    void sequence_source(size_t rows = 3U) {
        config.kind = NX_STM32_WAVE_SEQUENCE;
        config.burst_budget_hclk_cycles = 128U;
        hardware.dma_lane = config.controller == 1U ? 5U : 2U;
        config.dma = {&hardware.dma, &hardware.stream,
                      config.controller == 1U ? DMA2_Stream5_IRQn
                                              : DMA1_Stream2_IRQn,
                      hardware.dma_lane,
                      static_cast<uint8_t>(config.controller == 1U ? 6U : 5U)};
        const size_t width = config.controller == 1U ? 4U : 2U;
        slot.data = reinterpret_cast<uint8_t*>(pool);
        slot.capacity = sizeof(pool);
        ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
        nx_frame_plane_t plane = {0U, 6U * width, 6U * width, rows};
        nx_frame_layout_t layout;
        ASSERT_EQ(nx_frame_layout_planar(&plane, 1U, &layout), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_initialize(&source, &stream, &layout),
                  NX_SUCCESS);
        nx_stream_fill_t fill;
        ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
        for (size_t row = 0U; row < rows; ++row)
            for (size_t item = 0U; item < 6U; ++item) {
                uint32_t value = item == 0U
                                     ? 999U + 10U * static_cast<uint32_t>(row)
                                 : item == 1U ? 0U
                                 : (config.channels & (1U << (item - 2U))) != 0U
                                     ? 100U * static_cast<uint32_t>(item - 1U) +
                                           static_cast<uint32_t>(row)
                                     : 0U;
                std::memcpy(fill.data + (row * 6U + item) * width, &value,
                            width);
            }
        ASSERT_EQ(nx_frame_stream_publish(&source, &fill, 0U), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_acquire(&source, &loan), NX_SUCCESS);
        nx_request_initialize(&sequence_request.base);
        ASSERT_EQ(nx_request_prepare(&sequence_request.base, NX_DEADLINE_NEVER),
                  NX_SUCCESS);
        sequence_request.source = &loan;
    }
    void sequence_complete() {
        for (size_t row = 0U; row < source.layout.planes[0].rows; ++row)
            hardware.update();
        nx_stm32_waveform_dma_irq(&owner);
    }
    void sequence_tail() {
        EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_BUSY);
        hardware.update();
        EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_BUSY);
        hardware.update();
    }
    void coexist(uint8_t primary, uint8_t peer) {
        advanced();
        config.controller = primary;
        config.irq = primary == 1U ? TIM1_BRK_TIM9_IRQn : TIM8_BRK_TIM12_IRQn;
        config.channels = 1U;
        config.complementary = false;
        config.internal_trigger = false;
        config.common.pin_count = 2U;
        pins[0].pin = 8U;
        pins[1].pin = 12U;
        prepare();
        ASSERT_EQ(nx_pwm_group_start(&group), NX_SUCCESS);
        nx_stm32_waveform_state_t peer_owner = {};
        nx_stm32_waveform_config_t peer_config = config;
        peer_config.registers = &hardware.master;
        peer_config.controller = peer;
        peer_config.timer_hz =
            peer == 9U ? NEXUS_APB2_TIMER_HZ : NEXUS_APB1_TIMER_HZ;
        peer_config.channels = 3U;
        peer_config.deadtime_ticks = 0U;
        peer_config.irq = static_cast<IRQn_Type>(-1);
        peer_config.common.pins = &pins[2];
        pins[2].pin = 2U;
        pins[3].pin = 3U;
        const unsigned nvic_before = hardware.nvic_writes;
        const uint32_t cr1 = hardware.timer.CR1, ccer = hardware.timer.CCER;
        const uint32_t dier = hardware.timer.DIER, bdtr = hardware.timer.BDTR;
        nx_pwm_group_t peer_face = {&nx_stm32_pwm_group_ops, &peer_owner};
        ASSERT_EQ(nx_stm32_waveform_prepare(&peer_owner, &peer_config),
                  NX_SUCCESS);
        ASSERT_EQ(nx_pwm_group_start(&peer_face), NX_SUCCESS);
        ASSERT_NE(hardware.master.CR1 & TIM_CR1_CEN, 0U);
        ASSERT_EQ(nx_pwm_group_stop(&peer_face), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_waveform_close(&peer_owner), NX_SUCCESS);
        EXPECT_EQ(hardware.nvic_writes, nvic_before);
        EXPECT_EQ(hardware.advanced_resets, 0U);
        EXPECT_EQ(hardware.timer.CR1, cr1);
        EXPECT_EQ(hardware.timer.CCER, ccer);
        EXPECT_EQ(hardware.timer.DIER, dier);
        EXPECT_EQ(hardware.timer.BDTR, bdtr);
        EXPECT_NE(hardware.enabled[config.irq], 0U);
        EXPECT_EQ(nx_pwm_group_stop(&group), NX_SUCCESS);
        EXPECT_EQ(nx_stm32_waveform_close(&owner), NX_SUCCESS);
    }
};

TEST_F(TimerWaveform, PlatformPrepareDoesNotStartAnyCounterOutputOrDma) {
    prepare();
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(hardware.timer.CCER, 0U);
    EXPECT_EQ(hardware.timer.DIER, 0U);
    EXPECT_EQ(hardware.stream.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(hardware.timer.PSC, 83U);
    EXPECT_EQ(hardware.timer.ARR, 999U);
}
TEST_F(TimerWaveform, UdisPreventsAnyPartialGroupFromBecomingActive) {
    prepare();
    ASSERT_EQ(nx_pwm_group_start(&group), NX_SUCCESS);
    hardware.inject_update = true;
    EXPECT_EQ(nx_pwm_group_commit(&group, &frame), NX_SUCCESS);
    EXPECT_FALSE(hardware.partial_active);
    for (auto compare : hardware.active_ccr)
        EXPECT_EQ(compare, 0U);
    hardware.update();
    for (unsigned i = 0U; i < 4U; ++i)
        EXPECT_EQ(hardware.active_ccr[i], duties[i]);
    EXPECT_NE(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(TimerWaveform, FailedUpdateMaskMakesNoShadowWrites) {
    prepare();
    hardware.refused = &hardware.timer.CR1;
    hardware.refused_mask = TIM_CR1_UDIS;
    unsigned writes = hardware.compare_writes;
    EXPECT_EQ(nx_pwm_group_commit(&group, &frame), NX_ERROR_IO);
    EXPECT_EQ(hardware.compare_writes, writes);
    EXPECT_EQ(hardware.timer.CCER, 0U);
}
TEST_F(TimerWaveform, FailedPartialWriteStopsBeforeAllowingAnotherUpdate) {
    advanced();
    prepare();
    ASSERT_EQ(nx_pwm_group_start(&group), NX_SUCCESS);
    hardware.refused = &hardware.timer.CCR2;
    EXPECT_EQ(nx_pwm_group_commit(&group, &frame), NX_ERROR_IO);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(hardware.timer.CCER, 0U);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    nx_pwm_group_state_t state;
    ASSERT_EQ(nx_pwm_group_state(&group, &state), NX_SUCCESS);
    EXPECT_TRUE(state.fault);
    EXPECT_NE(nx_pwm_group_start(&group), NX_SUCCESS);
    EXPECT_NE(nx_pwm_group_clear_fault(&group), NX_SUCCESS);
    hardware.refused = nullptr;
    ASSERT_EQ(nx_pwm_group_commit(&group, &frame), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_group_clear_fault(&group), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(TimerWaveform, BreakDuringAGroupCommitCannotAutomaticallyRestartOutput) {
    advanced();
    prepare();
    ASSERT_EQ(nx_pwm_group_start(&group), NX_SUCCESS);
    hardware.break_on_compare = true;
    EXPECT_EQ(nx_pwm_group_commit(&group, &frame), NX_ERROR_IO);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(hardware.timer.CCER, 0U);
    hardware.gpio.IDR = 0U;
    EXPECT_NE(nx_pwm_group_start(&group), NX_SUCCESS);
}
TEST_F(TimerWaveform, Tim5CpuGroupKeepsTheFullThirtyTwoBitCompare) {
    config.controller = 5U;
    config.irq = TIM5_IRQn;
    config.period_ticks = 100000U;
    duties[0] = 70000U;
    duties[1] = 0U;
    duties[2] = 100000U;
    prepare();
    EXPECT_EQ(nx_pwm_group_commit(&group, &frame), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.ARR, 99999U);
    EXPECT_EQ(hardware.timer.CCR1, 70000U);
    EXPECT_EQ(hardware.timer.CCR2, 0U);
    EXPECT_EQ(hardware.timer.CCR3, 100000U);
}
TEST_F(TimerWaveform, CoreTimerAndFakeDmaCapabilitiesRejectWithoutEffects) {
    config.controller = 2U;
    unsigned writes = hardware.writes;
    EXPECT_EQ(nx_stm32_waveform_prepare(&owner, &config), NX_ERROR_PERMISSION);
    EXPECT_EQ(hardware.writes, writes);
    config.controller = 9U;
    config.kind = NX_STM32_WAVE_SEQUENCE;
    config.channels = 3U;
    EXPECT_NE(nx_stm32_waveform_prepare(&owner, &config), NX_SUCCESS);
    EXPECT_EQ(hardware.writes, writes);
}
TEST_F(TimerWaveform, ContextRejectionKeepsEveryIncomingMaskAndAllShadows) {
    prepare();
    unsigned writes = hardware.writes;
    for (unsigned context = 0U; context < 5U; ++context) {
        hardware.privileged = context != 0U;
        hardware.in_isr = context == 1U;
        hardware.masks[0] = context == 2U ? 1U : 0U;
        hardware.masks[1] = context == 3U ? 32U : 0U;
        hardware.masks[2] = context == 4U ? 1U : 0U;
        uint32_t before[3];
        std::memcpy(before, hardware.masks, sizeof(before));
        EXPECT_EQ(nx_pwm_group_commit(&group, &frame), NX_ERROR_CONTEXT);
        EXPECT_EQ(std::memcmp(before, hardware.masks, sizeof(before)), 0);
        EXPECT_EQ(hardware.writes, writes);
    }
}
TEST_F(TimerWaveform, OnePulseUsesPwmTwoAndJoinsAfterHardwareCounterStop) {
    config.kind = NX_STM32_WAVE_PULSE;
    prepare();
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    EXPECT_NE(hardware.timer.CR1 & TIM_CR1_OPM, 0U);
    EXPECT_EQ(hardware.timer.CCMR1 & 0x7070U, 0x7070U);
    EXPECT_EQ(hardware.timer.CCR1, 900U);
    EXPECT_EQ(hardware.timer.CCR4, 600U);
    EXPECT_NE(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    hardware.update();
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_NE(nx_request_state(&pulse_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&pulse_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(pulse_request.base.transferred, 1U);
    EXPECT_EQ(hardware.timer.CCER, 0U);
    EXPECT_EQ(owner.active, nullptr);
}
TEST_F(TimerWaveform, CancelledPulseRetainsItsControlUntilCounterReadback) {
    config.kind = NX_STM32_WAVE_PULSE;
    prepare();
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    hardware.refused = &hardware.timer.CR1;
    hardware.refused_mask = TIM_CR1_CEN;
    EXPECT_EQ(pulse.ops->cancel(pulse.context, &pulse_request), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&pulse_request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(owner.active, &pulse_request);
    hardware.refused = nullptr;
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_request_state(&pulse_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(pulse_request.base.transferred, 0U);
}
TEST_F(TimerWaveform, FirstObservedPulseCompletionAtDeadlineReportsTimeout) {
    config.kind = NX_STM32_WAVE_PULSE;
    prepare();
    ASSERT_EQ(nx_request_prepare(&pulse_request.base, 10U), NX_SUCCESS);
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    hardware.update();
    hardware.now = 10U;
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_ERROR_TIMEOUT);
    EXPECT_EQ(pulse_request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(pulse_request.base.transferred, 0U);
}
TEST_F(TimerWaveform, PulseClockHistoryLossCannotReleaseARecoveredOldControl) {
    config.kind = NX_STM32_WAVE_PULSE;
    prepare();
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    hardware.rcc.APB1RSTR |= RCC_APB1RSTR_TIM3RST;
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_ERROR_STATE);
    hardware.rcc.APB1RSTR &= ~static_cast<uint32_t>(RCC_APB1RSTR_TIM3RST);
    hardware.update();
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&pulse_request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(owner.active, &pulse_request);
}
TEST_F(TimerWaveform,
       SettledPulseControlsCanBeProtectedBeforeLateIrqsAndClose) {
    config.kind = NX_STM32_WAVE_PULSE;
    prepare();
    struct Controls {
        nx_timer_pulse_request_t request;
        uint32_t widths[4];
    };
    void* page = mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(page, MAP_FAILED);
    auto* controls = ::new (page) Controls{};
    for (unsigned i = 0U; i < 4U; ++i)
        controls->widths[i] = duties[i];
    nx_request_initialize(&controls->request.base);
    ASSERT_EQ(nx_request_prepare(&controls->request.base, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    controls->request.width_ticks = controls->widths;
    controls->request.channel_count = 4U;
    ASSERT_EQ(pulse.ops->submit(pulse.context, &controls->request), NX_SUCCESS);
    hardware.update();
    ASSERT_EQ(pulse.ops->service(pulse.context), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&controls->request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(mprotect(page, 4096U, PROT_NONE), 0);
    nx_stm32_waveform_irq(&owner);
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_waveform_close(&owner), NX_SUCCESS);
    EXPECT_EQ(munmap(page, 4096U), 0);
}
TEST_F(TimerWaveform, FastPulseMayFinishBeforeTheCounterEnableReadback) {
    config.kind = NX_STM32_WAVE_PULSE;
    config.period_ticks = 2U;
    for (auto& width : duties)
        width = 1U;
    prepare();
    hardware.complete_on_enable = true;
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_SUCCESS);
    EXPECT_EQ(pulse_request.base.result, NX_SUCCESS);
    EXPECT_EQ(pulse_request.base.transferred, 1U);
}
TEST_F(TimerWaveform, RefusedUpdateClearCannotAuthorizeAFalseCompletedPulse) {
    config.kind = NX_STM32_WAVE_PULSE;
    prepare();
    hardware.hold_update_clear = true;
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    (void)pulse.ops->service(pulse.context);
    EXPECT_EQ(hardware.enable_attempts, 0U);
    EXPECT_EQ(nx_request_state(&pulse_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(pulse_request.base.result, NX_ERROR_IO);
    EXPECT_EQ(pulse_request.base.transferred, 0U);
}
TEST_F(TimerWaveform, CompletedPulseKeepsItsBorrowUntilBreakVectorReallyJoins) {
    advanced();
    config.kind = NX_STM32_WAVE_PULSE;
    config.complementary = false;
    config.internal_trigger = false;
    config.common.pin_count = 4U;
    pins[3].pin = 12U;
    pulse_request.channel_count = 3U;
    prepare();
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    hardware.update();
    hardware.hold_irq = true;
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&pulse_request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(owner.active, &pulse_request);
    hardware.hold_irq = false;
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&pulse_request.base), NX_REQUEST_SETTLED);
}
TEST_F(TimerWaveform, NativeDmaAdvancesEveryRowWithoutPerRowCpuService) {
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    EXPECT_EQ(hardware.timer.DCR, 11U | (5U << 8U));
    EXPECT_EQ(hardware.stream.CR & (DMA_SxCR_PSIZE | DMA_SxCR_MSIZE),
              DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0);
    EXPECT_EQ(hardware.stream.PAR,
              static_cast<uint32_t>(
                  reinterpret_cast<uintptr_t>(&hardware.timer.DMAR)));
    sequence_complete();
    EXPECT_EQ(hardware.stream.NDTR, 0U);
    EXPECT_EQ(hardware.timer.CCR1, 102U);
    EXPECT_EQ(hardware.timer.CCR4, 402U);
    EXPECT_NE(nx_request_state(&sequence_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    sequence_tail();
    EXPECT_EQ(hardware.active_ccr[0], 102U);
    EXPECT_EQ(hardware.active_ccr[3], 402U);
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_SUCCESS);
    EXPECT_EQ(sequence_request.base.transferred, 36U);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(hardware.stream.M0AR | hardware.stream.M1AR | hardware.stream.PAR,
              0U);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(hardware.timer.DIER, 0U);
}
TEST_F(TimerWaveform, AdvancedDmarUsesNativeWordsWithAnExplicitRcrZero) {
    advanced();
    config.complementary = false;
    config.internal_trigger = false;
    config.common.pin_count = 4U;
    pins[3].pin = 12U;
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    EXPECT_EQ(hardware.stream.CR & (DMA_SxCR_PSIZE | DMA_SxCR_MSIZE),
              DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1);
    sequence_complete();
    EXPECT_EQ(hardware.timer.RCR, 0U);
    EXPECT_EQ(hardware.timer.CCR4, 0U);
    sequence_tail();
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_SUCCESS);
    EXPECT_EQ(sequence_request.base.transferred, 72U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(TimerWaveform, CounterZeroWithoutRealTcCannotReleaseSequenceStorage) {
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    hardware.stream.NDTR = 0U;
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_BUSY);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(owner.active, &sequence_request);
}
TEST_F(TimerWaveform, FailedDmaStopKeepsTheCompleteSequenceLoan) {
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    hardware.refused = &hardware.stream.CR;
    hardware.refused_mask = DMA_SxCR_EN;
    EXPECT_EQ(sequence.ops->cancel(sequence.context, &sequence_request),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&sequence_request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(nx_stm32_waveform_close(&owner), NX_ERROR_BUSY);
    EXPECT_TRUE(owner.owned);
    hardware.refused = nullptr;
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_CANCELLED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(sequence_request.base.transferred, 0U);
}
TEST_F(TimerWaveform, CompletedSequenceStillJoinsTheActualDmaIrqAndAddress) {
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    sequence_complete();
    hardware.hold_irq = true;
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_BUSY);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    hardware.hold_irq = false;
    sequence_tail();
    hardware.refused = &hardware.stream.M0AR;
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_BUSY);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    hardware.refused = nullptr;
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_SUCCESS);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
}
TEST_F(TimerWaveform, SequenceDmaErrorReportsNoGuaranteedBytesOrSharedReset) {
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    hardware.dma_flags(1U << 3U);
    nx_stm32_waveform_dma_irq(&owner);
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_IO);
    EXPECT_EQ(sequence_request.base.transferred, 0U);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(hardware.shared_resets, 0U);
}
TEST_F(TimerWaveform, DmaResetHistoryRetainsTheLoanAfterRegistersAreRestored) {
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    hardware.rcc.AHB1RSTR |= RCC_AHB1RSTR_DMA1RST;
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_STATE);
    hardware.rcc.AHB1RSTR = 0U;
    EXPECT_EQ(sequence.ops->stop(sequence.context), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_waveform_close(&owner), NX_ERROR_BUSY);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(owner.active, &sequence_request);
}
TEST_F(TimerWaveform, SequenceStateRejectsAnOutputInUnusedLoanCapacity) {
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    auto* output = reinterpret_cast<nx_timer_sequence_state_t*>(pool + 64U);
    const uint32_t sentinel = 0xdeadbeefU;
    std::memcpy(output, &sentinel, sizeof(sentinel));
    EXPECT_EQ(sequence.ops->state(sequence.context, output), NX_ERROR_INVALID);
    uint32_t observed;
    std::memcpy(&observed, output, sizeof(observed));
    EXPECT_EQ(observed, sentinel);
}
TEST_F(TimerWaveform, InvalidSequencePaddingRejectsBeforeAnyHardwareEffects) {
    sequence_source();
    prepare();
    reinterpret_cast<uint16_t*>(pool)[1] = 1U;
    const unsigned writes = hardware.writes;
    EXPECT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_ERROR_INVALID);
    EXPECT_EQ(hardware.writes, writes);
    EXPECT_EQ(nx_request_state(&sequence_request.base), NX_REQUEST_READY);
    EXPECT_EQ(owner.active, nullptr);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
}
TEST_F(TimerWaveform, SequenceDeadlineIncludesTheWholeFinalIrqJoin) {
    sequence_source();
    prepare();
    ASSERT_EQ(nx_request_prepare(&sequence_request.base, 10000U), NX_SUCCESS);
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    sequence_complete();
    sequence_tail();
    hardware.now = 9999U;
    hardware.pending[config.dma.irq] = 1U;
    hardware.hold_irq = true;
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_BUSY);
    hardware.now = 10000U;
    hardware.hold_irq = false;
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_TIMEOUT);
    EXPECT_EQ(sequence_request.base.transferred, 0U);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
}
TEST_F(TimerWaveform, ASettledSequenceCanAcceptAnotherWholeLoan) {
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    sequence_complete();
    sequence_tail();
    ASSERT_EQ(sequence.ops->service(sequence.context), NX_SUCCESS);
    nx_stream_fill_t fill;
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_publish(&source, &fill, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_acquire(&source, &loan), NX_SUCCESS);
    ASSERT_EQ(nx_request_prepare(&sequence_request.base, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    EXPECT_EQ(hardware.stream.NDTR, 18U);
}
TEST_F(TimerWaveform, ALastBusErrorDuringDmaDisableIsObservedBeforeRelease) {
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    sequence_complete();
    hardware.error_on_dma_stop = true;
    nx_result_t result = sequence.ops->service(sequence.context);
    if (result == NX_ERROR_BUSY) {
        hardware.update();
        (void)sequence.ops->service(sequence.context);
        hardware.update();
        result = sequence.ops->service(sequence.context);
    }
    EXPECT_EQ(result, NX_ERROR_IO);
    EXPECT_EQ(sequence_request.base.result, NX_ERROR_IO);
    EXPECT_EQ(sequence_request.base.transferred, 0U);
}
TEST_F(TimerWaveform, SettledSequenceDropsControlBeforeAnyLateIrqOrClose) {
    sequence_source();
    void* page = mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(page, MAP_FAILED);
    auto* request = static_cast<nx_timer_sequence_request_t*>(page);
    auto* protected_loan = reinterpret_cast<nx_frame_dma_loan_t*>(
        static_cast<uint8_t*>(page) + 256U);
    const nx_dma_drain_facts_t idle = {1U, 1U, true, true, true};
    ASSERT_EQ(nx_frame_dma_release(&source, &loan, &idle), NX_SUCCESS);
    nx_stream_fill_t fill;
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_publish(&source, &fill, 0U), NX_SUCCESS);
    *protected_loan = {};
    ASSERT_EQ(nx_frame_stream_acquire(&source, protected_loan), NX_SUCCESS);
    *request = sequence_request;
    request->source = protected_loan;
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, request), NX_SUCCESS);
    sequence_complete();
    sequence_tail();
    ASSERT_EQ(sequence.ops->service(sequence.context), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&request->base), NX_REQUEST_SETTLED);
    ASSERT_EQ(mprotect(page, 4096U, PROT_NONE), 0);
    nx_stm32_waveform_dma_irq(&owner);
    nx_stm32_waveform_irq(&owner);
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_waveform_close(&owner), NX_SUCCESS);
    EXPECT_EQ(munmap(page, 4096U), 0);
}
TEST_F(TimerWaveform, AForeignTimerFoundAfterGateAcquisitionIsNotReconfigured) {
    hardware.timer.CR1 = TIM_CR1_CEN;
    hardware.timer.CCER = 0x1111U;
    EXPECT_EQ(nx_stm32_waveform_prepare(&owner, &config), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_waveform_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.CR1, TIM_CR1_CEN);
    EXPECT_EQ(hardware.timer.CCER, 0x1111U);
}
TEST_F(TimerWaveform, TriggerSlaveArmsWithoutCpuStartAndWaitsForSelectedItr) {
    config.kind = NX_STM32_WAVE_PULSE;
    config.sync_mode = 6U;
    config.itr = 3U;
    prepare();
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.SMCR & (TIM_SMCR_SMS | TIM_SMCR_TS),
              6U | (3U << 4U));
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(hardware.enable_attempts, 0U);
    nx_timer_pulse_state_t state;
    ASSERT_EQ(pulse.ops->state(pulse.context, &state), NX_SUCCESS);
    EXPECT_TRUE(state.armed);
    EXPECT_FALSE(state.running);
    hardware.trigger(true);
    EXPECT_NE(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    hardware.update();
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.SMCR, 0U);
    hardware.trigger(true);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(TimerWaveform, EnableMasterStartsArmedSlaveOnlyAtCounterEnable) {
    config.kind = NX_STM32_WAVE_PULSE;
    config.sync_mode = 6U;
    config.itr = 3U;
    prepare();
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    nx_stm32_waveform_state_t master_owner = {};
    nx_stm32_waveform_config_t master_config = config;
    master_config.registers = &hardware.master;
    master_config.kind = NX_STM32_WAVE_GROUP;
    master_config.controller = 4U;
    master_config.channels = 1U;
    master_config.common.pins = &pins[4];
    master_config.common.pin_count = 1U;
    master_config.irq = TIM4_IRQn;
    master_config.master_mode = 1U;
    master_config.sync_mode = 0U;
    master_config.itr = 0U;
    nx_pwm_group_t master = {&nx_stm32_pwm_group_ops, &master_owner};
    ASSERT_EQ(nx_stm32_waveform_prepare(&master_owner, &master_config),
              NX_SUCCESS);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    ASSERT_EQ(nx_pwm_group_start(&master), NX_SUCCESS);
    EXPECT_EQ(hardware.master.CR2 & TIM_CR2_MMS, 1U << 4U);
    EXPECT_NE(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    hardware.update();
    ASSERT_EQ(pulse.ops->service(pulse.context), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_waveform_close(&master_owner), NX_SUCCESS);
}
TEST_F(TimerWaveform, ResetAndGatedSlavesKeepTheirActualSmsAndItrFields) {
    config.sync_mode = 4U;
    config.itr = 2U;
    prepare();
    ASSERT_EQ(nx_pwm_group_start(&group), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.SMCR, 4U | (2U << 4U));
    hardware.timer.CNT = 500U;
    hardware.trigger(true);
    EXPECT_EQ(hardware.timer.CNT, 0U);
    EXPECT_EQ(nx_pwm_group_stop(&group), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.SMCR, 0U);
    EXPECT_EQ(nx_stm32_waveform_close(&owner), NX_SUCCESS);
    hardware.timer = {};
    config.sync_mode = 5U;
    config.itr = 1U;
    prepare();
    ASSERT_EQ(nx_pwm_group_start(&group), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.SMCR, 5U | (1U << 4U));
    EXPECT_NE(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(TimerWaveform, UpdateAndOc4RefMastersPublishOnlyTheSelectedMms) {
    advanced();
    config.master_mode = 7U;
    prepare();
    ASSERT_EQ(nx_pwm_group_start(&group), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.CR2 & TIM_CR2_MMS, 7U << 4U);
    EXPECT_EQ(nx_pwm_group_stop(&group), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.CR2 & TIM_CR2_MMS, 1U << 4U);
    EXPECT_EQ(nx_stm32_waveform_close(&owner), NX_SUCCESS);
    hardware.timer = {};
    config.master_mode = 2U;
    prepare();
    ASSERT_EQ(nx_pwm_group_start(&group), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.CR2 & TIM_CR2_MMS, 2U << 4U);
}
TEST_F(TimerWaveform, FailedChannelEnableCannotStartACounterOrTriggerPeers) {
    prepare();
    hardware.refused = &hardware.timer.CCER;
    EXPECT_EQ(nx_pwm_group_start(&group), NX_ERROR_IO);
    EXPECT_EQ(hardware.enable_attempts, 0U);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(TimerWaveform, UnalignedGroupArgumentsRejectBeforeTypedLoadsOrWrites) {
    prepare();
    alignas(8) uint8_t storage[sizeof(frame) + 8U] = {};
    std::memcpy(storage + 1U, &frame, sizeof(frame));
    const unsigned writes = hardware.writes;
    EXPECT_EQ(
        nx_pwm_group_commit(
            &group, reinterpret_cast<nx_pwm_group_frame_t*>(storage + 1U)),
        NX_ERROR_INVALID);
    EXPECT_EQ(hardware.writes, writes);
    alignas(8) uint8_t output[sizeof(nx_pwm_group_state_t) + 8U] = {};
    EXPECT_EQ(nx_pwm_group_state(
                  &group, reinterpret_cast<nx_pwm_group_state_t*>(output + 1U)),
              NX_ERROR_INVALID);
    EXPECT_EQ(output[1], 0U);
}
TEST_F(TimerWaveform, AdvancedOwnerRejectsAnUnrelatedBreakVectorBeforeEffects) {
    advanced();
    config.irq = TIM3_IRQn;
    const unsigned writes = hardware.writes;
    EXPECT_EQ(nx_stm32_waveform_prepare(&owner, &config), NX_ERROR_INVALID);
    EXPECT_EQ(hardware.writes, writes);
}
TEST_F(TimerWaveform, UnsupportedSmsAndMmsValuesRejectBeforeEffects) {
    config.sync_mode = 1U;
    const unsigned writes = hardware.writes;
    EXPECT_EQ(nx_stm32_waveform_prepare(&owner, &config), NX_ERROR_INVALID);
    EXPECT_EQ(hardware.writes, writes);
    owner = {};
    hardware.timer = {};
    hardware.rcc.APB1ENR = RCC_APB1ENR_TIM2EN;
    config.sync_mode = 0U;
    config.master_mode = 3U;
    EXPECT_EQ(nx_stm32_waveform_prepare(&owner, &config), NX_ERROR_INVALID);
    EXPECT_EQ(hardware.writes, writes);
}
TEST_F(TimerWaveform, Tim9TwoChannelGroupAvoidsAbsentMasterAndDmaRegisters) {
    config.controller = 9U;
    config.channels = 3U;
    config.common.pin_count = 2U;
    config.timer_hz = NEXUS_APB2_TIMER_HZ;
    config.irq = static_cast<IRQn_Type>(-1);
    hardware.available_channels = 2U;
    hardware.has_master = false;
    frame.channel_count = 2U;
    prepare();
    ASSERT_EQ(nx_pwm_group_commit(&group, &frame), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_group_start(&group), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_group_stop(&group), NX_SUCCESS);
    EXPECT_EQ(hardware.unavailable_writes, 0U);
}
TEST_F(TimerWaveform, Tim10SingleChannelPulseAvoidsAbsentSlaveRegisters) {
    config.kind = NX_STM32_WAVE_PULSE;
    config.controller = 10U;
    config.channels = 1U;
    config.common.pin_count = 1U;
    config.timer_hz = NEXUS_APB2_TIMER_HZ;
    config.irq = static_cast<IRQn_Type>(-1);
    hardware.available_channels = 1U;
    hardware.has_master = false;
    hardware.has_slave = false;
    pulse_request.channel_count = 1U;
    prepare();
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    hardware.update();
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_SUCCESS);
    EXPECT_EQ(hardware.unavailable_writes, 0U);
}
TEST_F(TimerWaveform, TriggerArmFailureRetainsBorrowUntilSmsReallyWithdraws) {
    config.kind = NX_STM32_WAVE_PULSE;
    config.sync_mode = 6U;
    prepare();
    ASSERT_EQ(pulse.ops->submit(pulse.context, &pulse_request), NX_SUCCESS);
    hardware.refused = &hardware.timer.SMCR;
    EXPECT_EQ(pulse.ops->stop(pulse.context), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&pulse_request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(owner.active, &pulse_request);
    hardware.refused = nullptr;
    EXPECT_EQ(pulse.ops->service(pulse.context), NX_ERROR_CANCELLED);
    EXPECT_EQ(hardware.timer.SMCR, 0U);
}
TEST_F(TimerWaveform, SequenceEpochChangeCannotReleaseAnOldDomainLoan) {
    sequence_source();
    prepare();
    ASSERT_EQ(sequence.ops->submit(sequence.context, &sequence_request),
              NX_SUCCESS);
    ++g_nx_stm32_system.time_epoch;
    EXPECT_EQ(sequence.ops->service(sequence.context), NX_ERROR_STATE);
    --g_nx_stm32_system.time_epoch;
    EXPECT_EQ(sequence.ops->stop(sequence.context), NX_ERROR_BUSY);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
}
TEST_F(TimerWaveform, Tim9PeerNeverTouchesTheRunningTim1OrItsSharedNvic) {
    coexist(1U, 9U);
}
TEST_F(TimerWaveform, Tim12PeerNeverTouchesTheRunningTim8OrItsSharedNvic) {
    coexist(8U, 12U);
}
} /* namespace */

extern "C" uint32_t nx_av_model_read(volatile uint32_t* reg) {
    return *reg;
}
extern "C" void nx_av_model_write(volatile uint32_t* reg, uint32_t value) {
    ++hw->writes;
    uint32_t previous = *reg;
    if ((!hw->has_master && (reg == &hw->timer.CR2 || reg == &hw->timer.CCMR2 ||
                             reg == &hw->timer.DCR || reg == &hw->timer.RCR ||
                             reg == &hw->timer.BDTR)) ||
        (!hw->has_slave && reg == &hw->timer.SMCR) ||
        (hw->available_channels < 4U && reg == &hw->timer.CCR4) ||
        (hw->available_channels < 3U && reg == &hw->timer.CCR3) ||
        (hw->available_channels < 2U && reg == &hw->timer.CCR2))
        ++hw->unavailable_writes;
    if (reg == hw->refused)
        value = (value & ~hw->refused_mask) | (*reg & hw->refused_mask);
    if (reg == &hw->timer.SR) {
        if (hw->hold_update_clear)
            value |= TIM_SR_UIF;
        hw->timer.SR &= value;
    } else if (reg == &hw->dma.LIFCR)
        hw->dma.LISR &= ~value;
    else if (reg == &hw->dma.HIFCR)
        hw->dma.HISR &= ~value;
    else
        *reg = value;
    if (reg == &hw->stream.CR && (value & DMA_SxCR_EN) == 0U)
        hw->dma_position = 0U;
    if (reg == &hw->stream.CR && (value & DMA_SxCR_EN) == 0U &&
        hw->error_on_dma_stop)
        hw->dma_flags(1U << 3U);
    if (reg == &hw->master.EGR && (value & TIM_EGR_UG) != 0U &&
        (hw->master.CR2 & TIM_CR2_MMS) == 0U)
        hw->trigger(true);
    if (reg == &hw->master.CR1 && (previous & TIM_CR1_CEN) == 0U &&
        (value & TIM_CR1_CEN) != 0U &&
        (hw->master.CR2 & TIM_CR2_MMS) == (1U << 4U))
        hw->trigger(true);
    if (reg == &hw->timer.CCR1 || reg == &hw->timer.CCR2 ||
        reg == &hw->timer.CCR3 || reg == &hw->timer.CCR4) {
        ++hw->compare_writes;
        if (hw->inject_update)
            hw->update();
        if (hw->break_on_compare) {
            hw->gpio.IDR |= 1U << 12U;
            hw->timer.SR |= TIM_SR_BIF;
            hw->timer.BDTR &= ~static_cast<uint32_t>(TIM_BDTR_MOE);
        }
    }
    if (reg == &hw->gpio.BSRR)
        hw->gpio.ODR = (hw->gpio.ODR | (value & 0xffffU)) & ~(value >> 16U);
    if (reg == &hw->timer.EGR && (value & TIM_EGR_UG) != 0U)
        hw->update();
    if (reg == &hw->timer.CR1 && (value & TIM_CR1_CEN) != 0U) {
        ++hw->enable_attempts;
        if (hw->complete_on_enable)
            hw->update();
    }
    if (reg == &hw->rcc.AHB1RSTR &&
        (value & (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U)
        ++hw->shared_resets;
    if (reg == &hw->rcc.APB2RSTR &&
        (value & (RCC_APB2RSTR_TIM1RST | RCC_APB2RSTR_TIM8RST)) != 0U)
        ++hw->advanced_resets;
}
extern "C" void nx_av_model_poll(void) {
}
extern "C" nx_time_us_t nx_av_model_now(void) {
    return hw->now;
}
extern "C" bool nx_av_model_privileged(void) {
    return hw->privileged;
}
extern "C" bool nx_av_model_in_isr(void) {
    return hw->in_isr;
}
extern "C" bool nx_av_model_masked(void) {
    return (hw->masks[0] | hw->masks[1] | hw->masks[2]) != 0U;
}
extern "C" uint32_t nx_av_model_irq_active(IRQn_Type irq) {
    return hw->active[irq];
}
extern "C" uint32_t nx_av_model_irq_pending(IRQn_Type irq) {
    return hw->pending[irq];
}
extern "C" uint32_t nx_av_model_irq_enabled(IRQn_Type irq) {
    return hw->enabled[irq];
}
extern "C" void nx_av_model_irq_enable(IRQn_Type irq) {
    ++hw->nvic_writes;
    hw->enabled[irq] = 1U;
}
extern "C" void nx_av_model_irq_disable(IRQn_Type irq) {
    ++hw->nvic_writes;
    if (!hw->hold_irq)
        hw->enabled[irq] = 0U;
}
extern "C" void nx_av_model_irq_clear(IRQn_Type irq) {
    ++hw->nvic_writes;
    if (!hw->hold_irq)
        hw->pending[irq] = 0U;
}
extern "C" void nx_av_model_irq_priority(IRQn_Type irq, uint32_t priority) {
    ++hw->nvic_writes;
    hw->priority[irq] = priority;
}
extern "C" uint32_t nx_av_model_irq_priority_read(IRQn_Type irq) {
    return hw->priority[irq];
}
