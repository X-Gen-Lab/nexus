/**
 * \file            power_stage_test.cpp
 * \brief           Real electric-drive owner and final-borrow contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_timer_waveform_model.h"
extern "C" {
#include "drivers/pwm_adc/power_stage.h"
#include "nexus/io/power_stage.h"
nx_stm32_system_t g_nx_stm32_system;
}
#include <cstdlib>
#include <gtest/gtest.h>

namespace {
struct Hardware {
    RCC_TypeDef rcc = {};
    TIM_TypeDef timer = {}, encoder = {}, core = {};
    ADC_TypeDef adc = {}, adc2 = {}, adc3 = {};
    ADC_Common_TypeDef common = {};
    GPIO_TypeDef a = {}, b = {}, c = {}, d = {};
    uint32_t enabled[82] = {}, pending[82] = {}, active[82] = {};
    uint32_t priority[82] = {};
    nx_time_us_t now = 0;
    volatile uint32_t* refused = nullptr;
    bool isr = false, overwrite = false, refuse_priority = false;
    uint32_t encoder_delay = 0U, callback_delay = 0U;
    uint32_t gated_adc_accesses = 0U, encoder_updates = 0U;
    nx_stm32_power_stage_t* break_owner = nullptr;
} * hw;
struct Computation {
    unsigned calls = 0;
    nx_power_stage_sample_t sample = {};
    nx_result_t result = NX_SUCCESS;
    uint32_t duty = 20;
};
nx_result_t compute(void* context, const nx_power_stage_sample_t* sample,
                    nx_power_stage_plan_t* output) {
    auto* state = static_cast<Computation*>(context);
    ++state->calls;
    hw->now += hw->callback_delay;
    if (hw->break_owner != nullptr) {
        hw->timer.SR |= TIM_SR_BIF;
        nx_stm32_power_stage_break_irq(hw->break_owner);
    }
    state->sample = *sample;
    for (auto& duty : output->duty_ticks)
        duty = state->duty;
    return state->result;
}
class PowerStage : public testing::Test {
  protected:
    Hardware hardware;
    Computation computation;
    nx_stm32_power_stage_t owner = {};
    nx_stm32_power_stage_config_t config = {};
    nx_power_stage_session_t session = {};
    nx_power_stage_t port = {&nx_stm32_power_stage_ops, &owner};
    nx_stm32_av_pin_t pwm[7] = {}, analog[4] = {}, encoder[2] = {};
    uint16_t snapshots[13] = {};
    uint8_t channels[4] = {0, 1, 2, 3}, times[4] = {};
    void SetUp() override {
        hw = &hardware;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &hardware.rcc;
        g_nx_stm32_system.timer = &hardware.core;
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.core_hz = NEXUS_CORE_HZ;
        g_nx_stm32_system.time_epoch = 1;
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
        hardware.rcc.AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                               RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;
        pwm[0] = {&hardware.c, &snapshots[0], 2, 6, 3};
        pwm[1] = {&hardware.a, &snapshots[1], 0, 7, 3};
        pwm[2] = {&hardware.c, &snapshots[2], 2, 7, 3};
        pwm[3] = {&hardware.b, &snapshots[3], 1, 0, 3};
        pwm[4] = {&hardware.c, &snapshots[4], 2, 8, 3};
        pwm[5] = {&hardware.b, &snapshots[5], 1, 1, 3};
        pwm[6] = {&hardware.a, &snapshots[6], 0, 6, 3};
        for (uint8_t i = 0; i != 4; ++i)
            analog[i] = {&hardware.a, &snapshots[7 + i], 0, i, 0};
        encoder[0] = {&hardware.d, &snapshots[11], 3, 12, 2};
        encoder[1] = {&hardware.d, &snapshots[12], 3, 13, 2};
        config.pwm.common = {pwm, 7, nullptr, 0, 8, 5};
        config.pwm.registers = &hardware.timer;
        config.pwm.break_input = &hardware.a.IDR;
        config.pwm.break_mask = 1U << 6;
        config.pwm.timer_hz = NEXUS_APB2_TIMER_HZ;
        config.pwm.tick_hz = 1000000;
        config.pwm.period_ticks = 100;
        config.pwm.deadtime_ticks = 84;
        config.pwm.irq = TIM8_BRK_TIM12_IRQn;
        config.pwm.kind = NX_STM32_WAVE_GROUP;
        config.pwm.controller = 8;
        config.pwm.channels = 7;
        config.pwm.master_mode = 7;
        config.pwm.complementary = true;
        config.pwm.internal_trigger = true;
        config.pwm.break_active_high = true;
        config.analog = {&hardware.adc,
                         {&hardware.adc2, &hardware.adc3},
                         &hardware.common,
                         analog,
                         channels,
                         times,
                         4,
                         NEXUS_ADC_HZ,
                         3300};
        config.encoder = &hardware.encoder;
        config.encoder_pins = encoder;
        config.trigger_ticks = 60;
        config.minimum_duty_ticks = 2;
        config.maximum_duty_ticks = 50;
        config.settling_ticks = 2;
        config.commit_before_ticks = 95;
        config.compute_budget_us = 10;
        config.max_irq_delay_us = 5;
        nx_request_initialize(&session.base);
    }
    void prepared() {
        ASSERT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_SUCCESS);
        const nx_power_stage_plan_t initial = {{20, 20, 20}};
        ASSERT_EQ(nx_power_stage_prepare(&session, &initial, compute,
                                         &computation, sizeof(computation), 20),
                  NX_SUCCESS);
    }
    void running() {
        prepared();
        ASSERT_EQ(nx_power_stage_start(&port, &session), NX_SUCCESS);
        hardware.now = 4;
        ASSERT_EQ(nx_power_stage_service(&port), NX_SUCCESS);
    }
    void irq(nx_time_us_t now = 67) {
        hardware.now = now;
        hardware.timer.CNT = static_cast<uint32_t>(now - 4);
        hardware.adc.SR |= ADC_SR_JEOC;
        hardware.adc.JDR1 = 1000U - hardware.adc.JOFR1;
        if ((hardware.adc.CR1 & ADC_CR1_SCAN) != 0U &&
            ((hardware.adc.JSQR & ADC_JSQR_JL) >> ADC_JSQR_JL_Pos) == 3U) {
            hardware.adc.JDR2 = 1100U - hardware.adc.JOFR2;
            hardware.adc.JDR3 = 1200U - hardware.adc.JOFR3;
            hardware.adc.JDR4 = 2000U - hardware.adc.JOFR4;
        }
        hardware.encoder.CNT = 123;
        hardware.isr = true;
        nx_stm32_power_stage_adc_irq(&owner);
        hardware.isr = false;
    }
};
TEST_F(PowerStage, PreparationKeepsEveryOutputAndConverterInactive) {
    prepared();
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(hardware.adc.CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
}
TEST_F(PowerStage, ImpossibleLowSideWindowRejectsBeforeHardwareWrites) {
    config.maximum_duty_ticks = 59;
    EXPECT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_ERROR_INVALID);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PowerStage, RunningSessionDoesNotExpireWithItsStartupDeadline) {
    running();
    irq();
    ASSERT_EQ(computation.calls, 1U);
    EXPECT_EQ(computation.sample.current_raw[0], 1000);
    EXPECT_EQ(computation.sample.current_raw[2], 1200);
    EXPECT_EQ(computation.sample.bus_raw, 2000);
    EXPECT_EQ(computation.sample.encoder_count, 123U);
    EXPECT_EQ(computation.sample.sequence, 0U);
    EXPECT_EQ(computation.sample.planned_apply_at, 104U);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(hardware.timer.CCR1, 20U);
    EXPECT_EQ(nx_power_stage_service(&port), NX_SUCCESS);
}
TEST_F(PowerStage, BadPlanInhibitsOutputWithoutPublishingAMixedGroup) {
    running();
    computation.duty = 51;
    irq();
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(hardware.timer.CCR1, 20U);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_ACTIVE);
    (void)nx_power_stage_service(&port);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
}
TEST_F(PowerStage, ActiveIrqRetainsSessionUntilRealJoin) {
    running();
    hardware.active[ADC_IRQn] = 1;
    EXPECT_EQ(nx_power_stage_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    hardware.active[ADC_IRQn] = 0;
    EXPECT_EQ(nx_power_stage_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
    const auto calls = computation.calls;
    irq();
    EXPECT_EQ(computation.calls, calls);
}
TEST_F(PowerStage, CallbackErrorAndOverwrittenScanNeverCommitOutput) {
    running();
    hardware.overwrite = true;
    irq();
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, BreakInhibitsButDoesNotReleaseBorrowInInterrupt) {
    running();
    hardware.timer.SR |= TIM_SR_BIF;
    nx_stm32_power_stage_break_irq(&owner);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_ACTIVE);
    (void)nx_power_stage_service(&port);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
}
TEST_F(PowerStage, PartialShadowFailureLeavesFaultAndRequiresJoinedClose) {
    running();
    computation.duty = 30;
    hardware.refused = &hardware.timer.CCR2;
    irq();
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_NE(nx_request_state(&session.base), NX_REQUEST_SETTLED);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
}
TEST_F(PowerStage, InvalidAdcCodeCannotEnterTheControlCallback) {
    running();
    hardware.now = 67;
    hardware.timer.CNT = 63;
    hardware.adc.SR = ADC_SR_JEOC;
    hardware.adc.JDR1 = 4096;
    nx_stm32_power_stage_adc_irq(&owner);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, AngleReadOutsideItsObservationBracketIsRejected) {
    running();
    hardware.encoder_delay = 2;
    irq();
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, CallbackFaultPreventsAnyNewShadowWrite) {
    running();
    computation.duty = 30;
    hardware.break_owner = &owner;
    irq();
    EXPECT_EQ(hardware.timer.CCR1, 20U);
    EXPECT_EQ(hardware.timer.CCR2, 20U);
    EXPECT_EQ(hardware.timer.CCR3, 20U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, CallbackOverrunCannotCommitAnOutput) {
    running();
    hardware.callback_delay = 11;
    computation.duty = 30;
    irq();
    EXPECT_EQ(hardware.timer.CCR1, 20U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, AWholeLostPeriodCannotBeHiddenByModuloCounter) {
    running();
    hardware.now = 167;
    hardware.timer.CNT = 63;
    hardware.timer.SR = TIM_SR_UIF;
    hardware.adc.SR = ADC_SR_JEOC;
    nx_stm32_power_stage_adc_irq(&owner);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, EpochLossNeverRestartsAndOnlyAnActualJoinReleasesBorrow) {
    running();
    hardware.active[ADC_IRQn] = 1;
    ++g_nx_stm32_system.time_epoch;
    EXPECT_EQ(nx_power_stage_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_QUARANTINED);
    --g_nx_stm32_system.time_epoch;
    EXPECT_EQ(nx_power_stage_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_QUARANTINED);
    hardware.active[ADC_IRQn] = 0;
    EXPECT_EQ(nx_power_stage_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(session.base.result, NX_ERROR_STATE);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, DirectStopAfterEpochLossStillInhibitsLiveHardware) {
    running();
    ++g_nx_stm32_system.time_epoch;
    (void)nx_power_stage_stop(&port);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(hardware.adc.CR2 & (ADC_CR2_ADON | ADC_CR2_JEXTEN), 0U);
}
TEST_F(PowerStage, MissingAdcGateDoesNotPreventLiveTimerSuppression) {
    running();
    hardware.rcc.APB2ENR &= ~static_cast<uint32_t>(RCC_APB2ENR_ADC1EN);
    EXPECT_NE(nx_power_stage_stop(&port), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_QUARANTINED);
}
TEST_F(PowerStage, ObservedForeignEncoderStateIsNeverClearedByColdClose) {
    hardware.encoder.CR1 = TIM_CR1_CEN;
    hardware.encoder.DIER = TIM_DIER_UIE;
    EXPECT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.encoder.CR1, TIM_CR1_CEN);
    EXPECT_EQ(hardware.encoder.DIER, TIM_DIER_UIE);
    EXPECT_EQ(hardware.rcc.APB1ENR & RCC_APB1ENR_TIM4EN, 0U);
}
TEST_F(PowerStage, StartupDeadlineIsSeparateFromTheWholeSessionDeadline) {
    prepared();
    EXPECT_EQ(session.base.deadline, NX_DEADLINE_NEVER);
    hardware.now = 19;
    ASSERT_EQ(nx_power_stage_start(&port, &session), NX_SUCCESS);
    hardware.now = 20;
    (void)nx_power_stage_service(&port);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(session.base.result, NX_ERROR_TIMEOUT);
}
TEST_F(PowerStage, AliasInTheWholeContextExtentRejectsBeforeAdmission) {
    prepared();
    session.context = &owner;
    session.context_bytes = sizeof(owner);
    EXPECT_EQ(nx_power_stage_start(&port, &session), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_READY);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(PowerStage, PartialAnalogAndEncoderPrepareEffectsCloseInOrder) {
    hardware.refused = &hardware.encoder.ARR;
    EXPECT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_ERROR_IO);
    EXPECT_TRUE(owner.encoder_owned);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.APB1ENR & RCC_APB1ENR_TIM4EN, 0U);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PowerStage, ZeroTimerClockIsRejectedWithoutUndefinedArithmetic) {
    config.pwm.timer_hz = 0;
    EXPECT_EXIT(
        {
            const auto result = nx_stm32_power_stage_admit(&owner, &config);
            std::_Exit(result == NX_ERROR_INVALID ? 0 : 1);
        },
        testing::ExitedWithCode(0), "");
}
TEST_F(PowerStage, ColdCloseRetriesAfterAnalogRestoreFailsPastTimerRelease) {
    running();
    hardware.refused = &hardware.common.CCR;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_ERROR_IO);
    EXPECT_FALSE(owner.pwm.owned);
    EXPECT_TRUE(owner.owned);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
    EXPECT_FALSE(owner.owned);
}
TEST_F(PowerStage, ColdCloseRetriesFinalIrqReceiptAfterAllGatesAreOff) {
    running();
    hardware.refuse_priority = true;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_ERROR_IO);
    EXPECT_FALSE(owner.pwm.owned);
    EXPECT_FALSE(owner.encoder_owned);
    hardware.refuse_priority = false;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.priority[ADC_IRQn], 0U);
}
TEST_F(PowerStage, CallbackContextCannotBorrowTheLiveImmutableFace) {
    prepared();
    session.context = &port;
    session.context_bytes = sizeof(port);
    EXPECT_EQ(nx_power_stage_start(&port, &session), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_READY);
}
TEST_F(PowerStage, CallbackContextCannotBorrowItsReadonlyOperations) {
    prepared();
    session.context = const_cast<nx_power_stage_ops_t*>(port.ops);
    session.context_bytes = sizeof(*port.ops);
    EXPECT_EQ(nx_power_stage_start(&port, &session), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_READY);
}
TEST_F(PowerStage, LateIrqNeverReadsAnUnclockedAdc) {
    running();
    hardware.rcc.APB2ENR &= ~static_cast<uint32_t>(RCC_APB2ENR_ADC1EN);
    hardware.adc.SR = ADC_SR_JEOC;
    nx_stm32_power_stage_adc_irq(&owner);
    EXPECT_EQ(hardware.gated_adc_accesses, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_NE(nx_request_state(&session.base), NX_REQUEST_SETTLED);
}
TEST_F(PowerStage, PartialColdCloseRetainsUnclockedAnalogReceipt) {
    hardware.encoder.CR1 = TIM_CR1_CEN;
    ASSERT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_ERROR_BUSY);
    hardware.rcc.APB2ENR &= ~static_cast<uint32_t>(RCC_APB2ENR_ADC1EN);
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_ERROR_STATE);
    EXPECT_EQ(hardware.gated_adc_accesses, 0U);
    EXPECT_TRUE(owner.owned);
    hardware.rcc.APB2ENR |= RCC_APB2ENR_ADC1EN;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
}
TEST_F(PowerStage, EncoderPrescalerIsLatchedBeforeInputsCanStart) {
    hardware.encoder.PSC = 31;
    prepared();
    EXPECT_EQ(hardware.encoder_updates, 1U);
    EXPECT_EQ(hardware.encoder.PSC, 0U);
    EXPECT_EQ(hardware.encoder.SR, 0U);
    EXPECT_EQ(hardware.encoder.CR2, 0U);
    EXPECT_EQ(hardware.encoder.CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(PowerStage, RefusedScanEnableCannotPublishAFourRankProvider) {
    hardware.refused = &hardware.adc.CR1;
    EXPECT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_ERROR_IO);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CEN, 0U);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
}
TEST_F(PowerStage, PriorInjectedOffsetsCannotBiasTheDeclaredRawScan) {
    hardware.adc.JOFR2 = 37;
    hardware.adc.JOFR4 = 400;
    running();
    irq();
    ASSERT_EQ(computation.calls, 1U);
    EXPECT_EQ(computation.sample.current_raw[1], 1100);
    EXPECT_EQ(computation.sample.bus_raw, 2000);
}
TEST_F(PowerStage, RefusedInjectedOffsetResetRetainsColdCleanupReceipt) {
    hardware.adc.JOFR4 = 19;
    hardware.refused = &hardware.adc.JOFR4;
    EXPECT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_ERROR_IO);
    EXPECT_TRUE(owner.owned);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
}
} /* namespace */
extern "C" uint32_t nx_av_model_read(volatile uint32_t* reg) {
    if (reinterpret_cast<uintptr_t>(reg) >=
            reinterpret_cast<uintptr_t>(&hw->adc) &&
        reinterpret_cast<uintptr_t>(reg) <
            reinterpret_cast<uintptr_t>(&hw->adc) + sizeof(hw->adc) &&
        (hw->rcc.APB2ENR & RCC_APB2ENR_ADC1EN) == 0U)
        ++hw->gated_adc_accesses;
    const auto value = *reg;
    if (reg == &hw->encoder.CNT)
        hw->now += hw->encoder_delay;
    if (reg == &hw->adc.JDR1 && hw->overwrite)
        hw->adc.SR |= ADC_SR_JEOC;
    return value;
}
extern "C" void nx_av_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reinterpret_cast<uintptr_t>(reg) >=
            reinterpret_cast<uintptr_t>(&hw->adc) &&
        reinterpret_cast<uintptr_t>(reg) <
            reinterpret_cast<uintptr_t>(&hw->adc) + sizeof(hw->adc) &&
        (hw->rcc.APB2ENR & RCC_APB2ENR_ADC1EN) == 0U)
        ++hw->gated_adc_accesses;
    if (reg == hw->refused)
        return;
    if (reg == &hw->timer.SR || reg == &hw->adc.SR || reg == &hw->encoder.SR)
        *reg &= value;
    else
        *reg = value;
    if (reg == &hw->timer.EGR && (value & TIM_EGR_UG) != 0)
        hw->timer.SR |= TIM_SR_UIF;
    if (reg == &hw->encoder.EGR && (value & TIM_EGR_UG) != 0U) {
        hw->encoder.SR |= TIM_SR_UIF;
        ++hw->encoder_updates;
    }
    for (auto* gpio : {&hw->a, &hw->b, &hw->c, &hw->d})
        if (reg == &gpio->BSRR)
            gpio->ODR = (gpio->ODR | (value & 0xffffU)) & ~(value >> 16U);
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
    return hw->isr;
}
extern "C" bool nx_av_model_masked(void) {
    return false;
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
    hw->enabled[irq] = 1;
}
extern "C" void nx_av_model_irq_disable(IRQn_Type irq) {
    hw->enabled[irq] = 0;
}
extern "C" void nx_av_model_irq_clear(IRQn_Type irq) {
    hw->pending[irq] = 0;
}
extern "C" void nx_av_model_irq_priority(IRQn_Type irq, uint32_t value) {
    if (!hw->refuse_priority || irq != ADC_IRQn)
        hw->priority[irq] = value;
}
extern "C" uint32_t nx_av_model_irq_priority_read(IRQn_Type irq) {
    return hw->priority[irq];
}
