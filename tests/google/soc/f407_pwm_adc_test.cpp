/**
 * \file            f407_pwm_adc_test.cpp
 * \brief           Production coupled PWM/ADC ownership at physical leaves
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_timer_waveform_model.h"
extern "C" {
#include "drivers/pwm_adc/instance.h"
#include "drivers/pwm_adc/loop.h"
nx_stm32_system_t g_nx_stm32_system;
}
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
struct Hardware {
    RCC_TypeDef rcc = {};
    TIM_TypeDef timer = {}, core = {};
    ADC_TypeDef adc = {}, adc2 = {}, adc3 = {};
    ADC_Common_TypeDef common = {};
    GPIO_TypeDef output = {}, analog = {};
    DMA_TypeDef dma = {};
    DMA_Stream_TypeDef memory = {};
    uint32_t enabled[82] = {}, pending[82] = {}, active[82] = {};
    uint32_t priority[82] = {};
    nx_time_us_t now = 0U;
    volatile uint32_t* refused = nullptr;
    uint32_t refuse_mask = UINT32_MAX;
    size_t writes = 0U;
    bool in_isr = false, masked = false;
    bool inject_jeoc_on_read = false;
    uint32_t shadow_delay = 0U;
} * hw;

class PwmAdc : public testing::Test {
  protected:
    Hardware hardware;
    nx_stm32_pwm_adc_state_t owner = {};
    nx_stm32_pwm_adc_config_t config = {};
    nx_pwm_adc_t face = {&nx_stm32_pwm_adc_ops, &owner};
    nx_pwm_adc_request_t request = {};
    nx_stm32_av_pin_t pwm_pins[4] = {}, analog_pins[1] = {};
    uint16_t snapshots[5] = {};
    uint8_t channels[1] = {0U}, sample_times[1] = {3U};
    uint32_t duties[3] = {100U, 200U, 300U};
    nx_pwm_group_frame_t frame = {duties, 3U, 500U};
    nx_dma_memory_region_t region = {};
    uint16_t* samples = nullptr;
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
        hardware.core.PSC = NEXUS_APB1_TIMER_HZ / 1000000U - 1U;
        hardware.core.ARR = UINT32_MAX;
        hardware.rcc.PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        hardware.rcc.CFGR =
            NEXUS_CLOCK_CFGR | RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
        hardware.rcc.CR =
            RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        hardware.rcc.APB1ENR = RCC_APB1ENR_TIM2EN;
        hardware.rcc.AHB1ENR =
            RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_DMA2EN;
        for (uint8_t i = 0U; i < 3U; ++i)
            pwm_pins[i] = {&hardware.output, &snapshots[i], 2U,
                           static_cast<uint8_t>(6U + i), 3U};
        pwm_pins[3] = {&hardware.analog, &snapshots[3], 0U, 6U, 3U};
        analog_pins[0] = {&hardware.analog, &snapshots[4], 0U, 0U, 0U};
        samples = static_cast<uint16_t*>(
            mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0));
        ASSERT_NE(samples, MAP_FAILED);
        region = {reinterpret_cast<uintptr_t>(samples), 4096U,
                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        config.pwm.common = {pwm_pins, 4U, &region, 1U, 8U, 5U};
        config.pwm.registers = &hardware.timer;
        config.pwm.break_input = &hardware.analog.IDR;
        config.pwm.break_mask = 1U << 6U;
        config.pwm.timer_hz = NEXUS_APB2_TIMER_HZ;
        config.pwm.tick_hz = 1000000U;
        config.pwm.period_ticks = 1000U;
        config.pwm.irq = TIM8_BRK_TIM12_IRQn;
        config.pwm.kind = NX_STM32_WAVE_GROUP;
        config.pwm.controller = 8U;
        config.pwm.channels = 7U;
        config.pwm.master_mode = 7U;
        config.pwm.internal_trigger = true;
        config.pwm.break_active_high = true;
        config.dma = {&hardware.dma, &hardware.memory, DMA2_Stream4_IRQn, 4U,
                      0U};
        config.analog.adc = &hardware.adc;
        config.analog.peers[0] = &hardware.adc2;
        config.analog.peers[1] = &hardware.adc3;
        config.analog.common = &hardware.common;
        config.analog.pins = analog_pins;
        config.analog.channels = channels;
        config.analog.sample_times = sample_times;
        config.analog.channel_count = 1U;
        config.analog.clock_hz = NEXUS_ADC_HZ;
        config.analog.reference_mv = 3300U;
        config.trigger_ticks = 500U;
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        if (samples != nullptr && samples != MAP_FAILED)
            munmap(samples, 4096U);
    }
    void prepared_request(nx_time_us_t deadline = 100U) {
        ASSERT_EQ(nx_stm32_pwm_adc_prepare(&owner, &config), NX_SUCCESS);
        ASSERT_EQ(nx_pwm_adc_prepare(&request, samples, 16U, &frame, deadline),
                  NX_SUCCESS);
    }
    void running_request() {
        prepared_request();
        ASSERT_EQ(nx_pwm_adc_submit(&face, &request), NX_SUCCESS);
        hardware.now = 4U;
        ASSERT_EQ(nx_pwm_adc_service(&face), NX_SUCCESS);
        ASSERT_NE(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    }
};

TEST_F(PwmAdc, ColdPreparationNeverStartsOutputConversionOrDma) {
    ASSERT_EQ(nx_stm32_pwm_adc_prepare(&owner, &config), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(hardware.adc.CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(hardware.memory.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(nx_stm32_pwm_adc_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PwmAdc, UnusedWindowDoesNotRewriteUnborrowedDmaAddresses) {
    hardware.memory.PAR = 0x1111U;
    hardware.memory.M0AR = 0x2222U;
    hardware.memory.M1AR = 0x3333U;
    ASSERT_EQ(nx_stm32_pwm_adc_prepare(&owner, &config), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_pwm_adc_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.memory.PAR, 0x1111U);
    EXPECT_EQ(hardware.memory.M0AR, 0x2222U);
    EXPECT_EQ(hardware.memory.M1AR, 0x3333U);
}
TEST_F(PwmAdc, PartialCommonClockPreparationRetainsOnlyAcquiredEffects) {
    hardware.refused = &hardware.common.CCR;
    EXPECT_EQ(nx_stm32_pwm_adc_prepare(&owner, &config), NX_ERROR_IO);
    EXPECT_TRUE(owner.owned);
    EXPECT_NE(owner.analog.clocks, 0U);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_pwm_adc_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PwmAdc, ForeignAdcClockRejectsBeforeAnyWrite) {
    hardware.rcc.APB2ENR |= RCC_APB2ENR_ADC2EN;
    EXPECT_EQ(nx_stm32_pwm_adc_prepare(&owner, &config), NX_ERROR_BUSY);
    EXPECT_EQ(hardware.writes, 0U);
}
TEST_F(PwmAdc, FailedRankWriteClosesTheActualPartialAnalogReceipt) {
    hardware.refused = &hardware.adc.SQR1;
    hardware.adc.SQR1 = ADC_SQR1_L;
    EXPECT_EQ(nx_stm32_pwm_adc_prepare(&owner, &config), NX_ERROR_IO);
    EXPECT_NE(owner.analog.config, nullptr);
    EXPECT_FALSE(owner.pwm.owned);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_pwm_adc_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PwmAdc, FailedTimerPreparationClosesBothRealColdReceipts) {
    hardware.refused = &hardware.timer.ARR;
    EXPECT_EQ(nx_stm32_pwm_adc_prepare(&owner, &config), NX_ERROR_IO);
    EXPECT_TRUE(owner.pwm.owned);
    EXPECT_NE(owner.analog.config, nullptr);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_pwm_adc_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PwmAdc, PlatformPreflightDoesNotRequireUnacquiredGpioClocks) {
    hardware.rcc.AHB1ENR &=
        ~static_cast<uint32_t>(RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN);
    EXPECT_EQ(nx_stm32_pwm_adc_admit(&owner, &config), NX_SUCCESS);
    EXPECT_EQ(hardware.writes, 0U);
    EXPECT_EQ(nx_stm32_pwm_adc_prepare(&owner, &config), NX_ERROR_STATE);
    EXPECT_EQ(hardware.writes, 0U);
}
TEST_F(PwmAdc, IrqCannotPrepareTheThreadOwnedWindow) {
    hardware.in_isr = true;
    EXPECT_EQ(nx_stm32_pwm_adc_prepare(&owner, &config), NX_ERROR_CONTEXT);
    EXPECT_EQ(hardware.writes, 0U);
}
TEST_F(PwmAdc, StartsOnlyAfterWarmupAndSettlesAfterAllSourcesDetach) {
    prepared_request();
    ASSERT_EQ(nx_pwm_adc_submit(&face, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    hardware.now = 4U;
    ASSERT_EQ(nx_pwm_adc_service(&face), NX_SUCCESS);
    EXPECT_NE(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ((hardware.adc.CR2 & ADC_CR2_EXTSEL) >> ADC_CR2_EXTSEL_Pos, 14U);
    hardware.memory.NDTR = 0U;
    hardware.memory.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    hardware.dma.HISR = 1U << 5U;
    nx_stm32_pwm_adc_dma_irq(&owner);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_pwm_adc_service(&face), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 16U);
    EXPECT_EQ(owner.active, nullptr);
    EXPECT_EQ(hardware.memory.M0AR | hardware.memory.M1AR | hardware.memory.PAR,
              0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PwmAdc, BadDutyRejectsWithoutAdmissionOrHardwareEffects) {
    prepared_request();
    duties[0] = 1001U;
    const size_t before = hardware.writes;
    EXPECT_EQ(nx_pwm_adc_submit(&face, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(hardware.writes, before);
}
TEST_F(PwmAdc, ExpiredDeadlineDoesNotArmAnything) {
    prepared_request(2U);
    hardware.now = 2U;
    const size_t before = hardware.writes;
    EXPECT_EQ(nx_pwm_adc_submit(&face, &request), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(hardware.writes, before);
}
TEST_F(PwmAdc, CancellationRetainsBorrowUntilMemoryStopActuallySucceeds) {
    running_request();
    hardware.refused = &hardware.memory.CR;
    hardware.refuse_mask = DMA_SxCR_EN;
    ASSERT_EQ(nx_pwm_adc_cancel(&face, &request), NX_SUCCESS);
    EXPECT_EQ(nx_pwm_adc_service(&face), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(owner.active, &request);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_pwm_adc_service(&face), NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(PwmAdc, ActiveDmaVectorPreventsSettlementUntilJoin) {
    running_request();
    hardware.active[DMA2_Stream4_IRQn] = 1U;
    ASSERT_EQ(nx_pwm_adc_cancel(&face, &request), NX_SUCCESS);
    EXPECT_EQ(nx_pwm_adc_service(&face), NX_ERROR_BUSY);
    EXPECT_EQ(owner.active, &request);
    hardware.active[DMA2_Stream4_IRQn] = 0U;
    EXPECT_EQ(nx_pwm_adc_service(&face), NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(PwmAdc, CompletionObservedAtDeadlineCannotBecomeSuccess) {
    running_request();
    hardware.now = 100U;
    hardware.memory.NDTR = 0U;
    hardware.memory.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    hardware.dma.HISR = 1U << 5U;
    nx_stm32_pwm_adc_dma_irq(&owner);
    EXPECT_EQ(nx_pwm_adc_service(&face), NX_ERROR_TIMEOUT);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(PwmAdc, BreakWithdrawsOutputButDoesNotReleaseTheRequestInIrq) {
    running_request();
    hardware.timer.SR |= TIM_SR_BIF;
    hardware.analog.IDR |= 1U << 6U;
    nx_stm32_pwm_adc_break_irq(&owner);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_pwm_adc_service(&face), NX_ERROR_IO);
    EXPECT_TRUE(owner.fault);
}
TEST_F(PwmAdc, LateIrqAfterSettlementCannotTouchCallerStorage) {
    running_request();
    ASSERT_EQ(nx_pwm_adc_stop(&face), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    const auto result = request.base.result;
    hardware.dma.HISR = 1U << 5U;
    nx_stm32_pwm_adc_dma_irq(&owner);
    EXPECT_EQ(request.base.result, result);
    EXPECT_EQ(owner.active, nullptr);
}
TEST_F(PwmAdc, LostEpochCannotBeRepairedByRestoringTheOldNumber) {
    running_request();
    ++g_nx_stm32_system.time_epoch;
    EXPECT_EQ(nx_pwm_adc_service(&face), NX_ERROR_STATE);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(owner.active, &request);
    --g_nx_stm32_system.time_epoch;
    EXPECT_NE(nx_pwm_adc_stop(&face), NX_SUCCESS);
    EXPECT_EQ(owner.active, &request);
}
TEST_F(PwmAdc, LostTimerGateCannotBeRepairedByRestoringItsRegisterValue) {
    running_request();
    hardware.rcc.APB2ENR &= ~static_cast<uint32_t>(RCC_APB2ENR_TIM8EN);
    EXPECT_EQ(nx_pwm_adc_service(&face), NX_ERROR_STATE);
    EXPECT_EQ(owner.active, &request);
    hardware.rcc.APB2ENR |= RCC_APB2ENR_TIM8EN;
    EXPECT_NE(nx_pwm_adc_stop(&face), NX_SUCCESS);
    EXPECT_TRUE(owner.history_lost);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
}
TEST_F(PwmAdc, TimerResetHistorySurvivesTheResetBitBeingCleared) {
    running_request();
    hardware.rcc.APB2RSTR |= RCC_APB2RSTR_TIM8RST;
    EXPECT_EQ(nx_pwm_adc_service(&face), NX_ERROR_STATE);
    hardware.rcc.APB2RSTR &= ~static_cast<uint32_t>(RCC_APB2RSTR_TIM8RST);
    EXPECT_NE(nx_pwm_adc_stop(&face), NX_SUCCESS);
    EXPECT_TRUE(owner.history_lost);
    EXPECT_EQ(owner.active, &request);
}

TEST_F(PwmAdc, StateOutputCannotOverwriteTheLiveDmaLoan) {
    running_request();
    EXPECT_EQ(
        nx_pwm_adc_state(&face, reinterpret_cast<nx_pwm_adc_state_t*>(samples)),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_pwm_adc_stop(&face), NX_SUCCESS);
}
TEST_F(PwmAdc, ColdCloseAllowsTheSameStaticWindowOwnerToReopen) {
    running_request();
    ASSERT_EQ(nx_pwm_adc_stop(&face), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_pwm_adc_close(&owner), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_pwm_adc_prepare(&owner, &config), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_adc_prepare(&request, samples, 16U, &frame, 200U),
              NX_SUCCESS);
    EXPECT_EQ(nx_pwm_adc_submit(&face, &request), NX_SUCCESS);
    EXPECT_EQ(nx_pwm_adc_stop(&face), NX_SUCCESS);
}

class PwmAdcLoop : public PwmAdc {
  protected:
    nx_stm32_pwm_adc_loop_state_t loop = {};
    nx_stm32_pwm_adc_loop_config_t loop_config = {};
    nx_pwm_adc_loop_t loop_face = {&nx_stm32_pwm_adc_loop_ops, &loop};
    nx_pwm_adc_loop_session_t session = {};
    struct Compute {
        Hardware* hardware;
        uint64_t sequence;
        size_t calls;
        uint32_t elapsed;
        uint32_t next_second;
        bool invalid_duty;
        nx_result_t result;
    } compute = {};
    uint16_t scan[1] = {};
    uint32_t outputs[3] = {5U, 10U, 15U};
    static nx_result_t step(void* context, const uint16_t* values, size_t count,
                            uint64_t sequence, uint32_t* output,
                            size_t output_count) {
        auto* state = static_cast<Compute*>(context);
        EXPECT_TRUE(state->hardware->in_isr);
        EXPECT_EQ(count, 1U);
        EXPECT_EQ(output_count, 3U);
        state->sequence = sequence;
        ++state->calls;
        output[0] = static_cast<uint32_t>(values[0]) % 20U;
        output[1] = state->next_second;
        output[2] = state->invalid_duty ? 51U : 15U;
        state->hardware->now += state->elapsed;
        state->hardware->timer.CNT =
            (state->hardware->timer.CNT + state->elapsed) % 50U;
        return state->result;
    }
    void SetUp() override {
        PwmAdc::SetUp();
        loop_config.pwm = config.pwm;
        loop_config.pwm.period_ticks = 50U;
        loop_config.analog = config.analog;
        loop_config.trigger_ticks = 10U;
        loop_config.commit_before_ticks = 40U;
        loop_config.compute_budget_ticks = 3U;
        loop_config.max_irq_delay_us = 2U;
        compute.hardware = &hardware;
        compute.next_second = 10U;
        nx_request_initialize(&session.base);
    }
    void start_loop(uint16_t* target = nullptr, nx_time_us_t deadline = 1000U) {
        ASSERT_EQ(nx_stm32_pwm_adc_loop_prepare(&loop, &loop_config),
                  NX_SUCCESS);
        ASSERT_EQ(
            nx_pwm_adc_loop_prepare(&session, target == nullptr ? scan : target,
                                    1U, outputs, 3U, step, &compute, deadline),
            NX_SUCCESS);
        ASSERT_EQ(nx_pwm_adc_loop_start(&loop_face, &session), NX_SUCCESS);
        hardware.now = 4U;
        ASSERT_EQ(nx_pwm_adc_loop_service(&loop_face), NX_SUCCESS);
    }
    void conversion(nx_time_us_t at = 18U, uint32_t counter = 14U) {
        hardware.now = at;
        hardware.timer.CNT = counter;
        hardware.adc.JDR1 = 123U;
        hardware.adc.SR |= ADC_SR_JEOC;
        hardware.in_isr = true;
        nx_stm32_pwm_adc_loop_adc_irq(&loop);
        hardware.in_isr = false;
    }
};

TEST_F(PwmAdcLoop, InjectedIrqComputesAndStagesOneCompleteGroup) {
    start_loop();
    conversion();
    EXPECT_EQ(compute.calls, 1U);
    EXPECT_EQ(compute.sequence, 0U);
    EXPECT_EQ(hardware.timer.CCR1, 3U);
    EXPECT_EQ(hardware.timer.CCR2, 10U);
    EXPECT_EQ(hardware.timer.CCR3, 15U);
    EXPECT_NE(nx_request_state(&session.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_pwm_adc_loop_stop(&loop_face), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
}
TEST_F(PwmAdcLoop, OrdinaryServiceBetweenCyclesDoesNotExpireTheNextAnchor) {
    start_loop();
    conversion();
    hardware.now = 19U;
    EXPECT_EQ(nx_pwm_adc_loop_service(&loop_face), NX_SUCCESS);
    hardware.timer.SR |= TIM_SR_UIF;
    conversion(68U, 14U);
    EXPECT_EQ(compute.calls, 2U);
    EXPECT_EQ(compute.sequence, 1U);
    EXPECT_EQ(nx_pwm_adc_loop_stop(&loop_face), NX_SUCCESS);
}
TEST_F(PwmAdcLoop, ModuloCounterCannotHideALostWholePeriod) {
    start_loop();
    conversion(68U, 14U);
    EXPECT_EQ(compute.calls, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(nx_pwm_adc_loop_service(&loop_face), NX_ERROR_TIMEOUT);
}
TEST_F(PwmAdcLoop, FullPeriodCallbackOverrunNeverCommitsANewGroup) {
    start_loop();
    compute.elapsed = 50U;
    conversion();
    EXPECT_EQ(compute.calls, 1U);
    EXPECT_EQ(hardware.timer.CCR1, 5U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(nx_pwm_adc_loop_service(&loop_face), NX_ERROR_TIMEOUT);
}
TEST_F(PwmAdcLoop, CallbackDeadlineExpiryCannotPublishAnotherDutyGroup) {
    start_loop(nullptr, 19U);
    compute.elapsed = 2U;
    conversion();
    EXPECT_EQ(hardware.timer.CCR1, 5U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(loop.completed_cycles, 0U);
    EXPECT_EQ(nx_pwm_adc_loop_service(&loop_face), NX_ERROR_TIMEOUT);
}
TEST_F(PwmAdcLoop, DeadlineDuringShadowWritesKeepsTheNewGroupBlocked) {
    start_loop(nullptr, 19U);
    hardware.shadow_delay = 2U;
    conversion();
    EXPECT_NE(hardware.timer.CR1 & TIM_CR1_UDIS, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(loop.completed_cycles, 0U);
    EXPECT_EQ(nx_pwm_adc_loop_service(&loop_face), NX_ERROR_TIMEOUT);
}
TEST_F(PwmAdcLoop, JeocReappearingDuringCaptureRejectsOverwrittenData) {
    start_loop();
    hardware.inject_jeoc_on_read = true;
    conversion();
    EXPECT_EQ(compute.calls, 0U);
    EXPECT_EQ(nx_pwm_adc_loop_service(&loop_face), NX_ERROR_OVERFLOW);
}
TEST_F(PwmAdcLoop, InvalidCallbackDutyCannotPublishAPartialGroup) {
    start_loop();
    compute.invalid_duty = true;
    conversion();
    EXPECT_EQ(hardware.timer.CCR1, 5U);
    EXPECT_EQ(nx_pwm_adc_loop_service(&loop_face), NX_ERROR_INVALID);
}
TEST_F(PwmAdcLoop, RefusedShadowWriteRemovesOutputAndRetainsFault) {
    start_loop();
    hardware.refused = &hardware.timer.CCR2;
    hardware.refuse_mask = UINT32_MAX;
    compute.next_second = 11U;
    conversion();
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(nx_pwm_adc_loop_service(&loop_face), NX_ERROR_IO);
}
TEST_F(PwmAdcLoop, ActiveCallbackVectorBlocksSessionSettlement) {
    start_loop();
    hardware.active[ADC_IRQn] = 1U;
    EXPECT_EQ(nx_pwm_adc_loop_stop(&loop_face), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_QUARANTINED);
    hardware.active[ADC_IRQn] = 0U;
    EXPECT_EQ(nx_pwm_adc_loop_stop(&loop_face), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
}
TEST_F(PwmAdcLoop, SequenceKeepsItsMeaningBeyondUint32) {
    start_loop();
    loop.completed_cycles = static_cast<uint64_t>(UINT32_MAX) + 1U;
    conversion();
    EXPECT_EQ(compute.sequence, static_cast<uint64_t>(UINT32_MAX) + 1U);
    nx_pwm_adc_loop_state_t observed = {};
    ASSERT_EQ(nx_pwm_adc_loop_state(&loop_face, &observed), NX_SUCCESS);
    EXPECT_EQ(observed.completed_cycles,
              static_cast<uint64_t>(UINT32_MAX) + 2U);
}
TEST_F(PwmAdcLoop, StopPreventsLateIrqFromCallingBorrowedContext) {
    start_loop();
    ASSERT_EQ(nx_pwm_adc_loop_stop(&loop_face), NX_SUCCESS);
    conversion();
    EXPECT_EQ(compute.calls, 0U);
    EXPECT_EQ(loop.active, nullptr);
}
TEST_F(PwmAdcLoop, StateOutputCannotOverwriteAnAdmittedSampleBuffer) {
    start_loop(samples);
    EXPECT_EQ(
        nx_pwm_adc_loop_state(
            &loop_face, reinterpret_cast<nx_pwm_adc_loop_state_t*>(samples)),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_pwm_adc_loop_stop(&loop_face), NX_SUCCESS);
}
TEST_F(PwmAdcLoop, HandBuiltContextCannotAliasAnAdmittedSample) {
    ASSERT_EQ(nx_stm32_pwm_adc_loop_prepare(&loop, &loop_config), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_adc_loop_prepare(&session, scan, 1U, outputs, 3U, step,
                                      &compute, 1000U),
              NX_SUCCESS);
    session.context = scan;
    nx_result_t result = nx_pwm_adc_loop_start(&loop_face, &session);
    EXPECT_EQ(result, NX_ERROR_INVALID);
    if (result == NX_SUCCESS) {
        EXPECT_EQ(nx_pwm_adc_loop_stop(&loop_face), NX_SUCCESS);
    }
}
TEST_F(PwmAdcLoop, FailedInjectedRankWritePreservesAnalogCloseResponsibility) {
    hardware.adc.JSQR = 1U;
    hardware.refused = &hardware.adc.JSQR;
    EXPECT_EQ(nx_stm32_pwm_adc_loop_prepare(&loop, &loop_config), NX_ERROR_IO);
    EXPECT_NE(loop.analog.config, nullptr);
    EXPECT_FALSE(loop.pwm.owned);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_pwm_adc_loop_close(&loop), NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PwmAdcLoop, FailedTimerPreparationPreservesBothColdReceipts) {
    hardware.refused = &hardware.timer.ARR;
    EXPECT_EQ(nx_stm32_pwm_adc_loop_prepare(&loop, &loop_config), NX_ERROR_IO);
    EXPECT_TRUE(loop.pwm.owned);
    EXPECT_NE(loop.analog.config, nullptr);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_pwm_adc_loop_close(&loop), NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PwmAdcLoop, ColdCloseAllowsTheSameStaticLoopOwnerToReopen) {
    start_loop();
    ASSERT_EQ(nx_pwm_adc_loop_stop(&loop_face), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_pwm_adc_loop_close(&loop), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_pwm_adc_loop_prepare(&loop, &loop_config), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_adc_loop_prepare(&session, scan, 1U, outputs, 3U, step,
                                      &compute, 2000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_pwm_adc_loop_start(&loop_face, &session), NX_SUCCESS);
    EXPECT_EQ(nx_pwm_adc_loop_stop(&loop_face), NX_SUCCESS);
}
} /* namespace */

extern "C" uint32_t nx_av_model_read(volatile uint32_t* reg) {
    uint32_t value = *reg;
    if (reg == &hw->adc.JDR1 && hw->inject_jeoc_on_read)
        hw->adc.SR |= ADC_SR_JEOC;
    return value;
}
extern "C" void nx_av_model_write(volatile uint32_t* reg, uint32_t value) {
    ++hw->writes;
    if (reg == hw->refused)
        value = (value & ~hw->refuse_mask) | (*reg & hw->refuse_mask);
    if (reg == &hw->timer.SR || reg == &hw->adc.SR)
        *reg &= value;
    else if (reg == &hw->dma.HIFCR)
        hw->dma.HISR &= ~value;
    else if (reg == &hw->dma.LIFCR)
        hw->dma.LISR &= ~value;
    else
        *reg = value;
    if (reg == &hw->timer.CCR2) {
        hw->now += hw->shadow_delay;
        hw->timer.CNT = (hw->timer.CNT + hw->shadow_delay) % 50U;
    }
    if (reg == &hw->output.BSRR)
        hw->output.ODR = (hw->output.ODR | (value & 0xffffU)) & ~(value >> 16U);
    if (reg == &hw->analog.BSRR)
        hw->analog.ODR = (hw->analog.ODR | (value & 0xffffU)) & ~(value >> 16U);
    if (reg == &hw->timer.EGR && (value & TIM_EGR_UG) != 0U)
        hw->timer.SR |= TIM_SR_UIF;
}
extern "C" void nx_av_model_poll(void) {
}
extern "C" nx_time_us_t nx_av_model_now(void) {
    return hw->now;
}
extern "C" bool nx_av_model_privileged(void) {
    return true;
}
extern "C" bool nx_av_model_in_isr(void) {
    return hw->in_isr;
}
extern "C" bool nx_av_model_masked(void) {
    return hw->masked;
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
    hw->enabled[irq] = 1U;
}
extern "C" void nx_av_model_irq_disable(IRQn_Type irq) {
    hw->enabled[irq] = 0U;
}
extern "C" void nx_av_model_irq_clear(IRQn_Type irq) {
    hw->pending[irq] = 0U;
}
extern "C" void nx_av_model_irq_priority(IRQn_Type irq, uint32_t priority) {
    hw->priority[irq] = priority;
}
extern "C" uint32_t nx_av_model_irq_priority_read(IRQn_Type irq) {
    return hw->priority[irq];
}
