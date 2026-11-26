/**
 * \file            power_stage_test.cpp
 * \brief           Real electric-drive owner and final-borrow contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../timer/f407_timer_waveform_model.h"
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
    TIM_TypeDef timer = {}, other_timer = {}, encoder = {}, core = {};
    ADC_TypeDef adc = {}, adc2 = {}, adc3 = {};
    ADC_Common_TypeDef common = {};
    GPIO_TypeDef a = {}, b = {}, c = {}, d = {}, e = {};
    uint32_t enabled[82] = {}, pending[82] = {}, active[82] = {};
    uint32_t priority[82] = {};
    nx_time_us_t now = 0;
    volatile uint32_t* refused = nullptr;
    bool isr = false, overwrite = false, refuse_priority = false;
    IRQn_Type refused_priority_vector = ADC_IRQn;
    uint32_t encoder_delay = 0U, callback_delay = 0U;
    uint32_t adc_enable_delay = 0U;
    uint32_t gated_adc_accesses = 0U, encoder_updates = 0U;
    nx_stm32_power_stage_t* break_owner = nullptr;
    nx_stm32_power_stage_t* opening_owner = nullptr;
    nx_stm32_power_stage_t* compare_fault_owner = nullptr;
    uint32_t unready_sources = 0U;
} * hw;
struct Computation {
    unsigned calls = 0;
    nx_power_stage_sample_t sample = {};
    nx_result_t result = NX_SUCCESS;
    uint32_t duty = 20;
    uint32_t trigger = 60U, start_tick = 0U;
    uint8_t phase_mask = 7U;
    uint64_t sequence_delta = 1U;
    bool dc_profile = false;
    bool foreign_dc = false;
    nx_power_stage_dc_plan_t dc_plan = {{30U, 50U}, {3U, 1U}};
    nx_power_stage_dc_capture_t dc_capture = {};
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
    if (sample->dc != nullptr)
        state->dc_capture = *sample->dc;
    *output = {};
    for (size_t i = 0U; i < 3U; ++i) {
        output->duty_ticks[i] = state->duty;
        output->start_ticks[i] = state->start_tick;
    }
    output->trigger_ticks = state->trigger;
    output->current_phase_mask = state->phase_mask;
    output->sequence = sample->sequence + state->sequence_delta;
    if (state->dc_profile) {
        output->duty_ticks[0] = 20U;
        output->duty_ticks[1] = 40U;
        output->duty_ticks[2] = 60U;
        static const nx_power_stage_dc_plan_t foreign = {{30U, 50U}, {3U, 1U}};
        output->dc = state->foreign_dc ? &foreign : &state->dc_plan;
    }
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
        config.current_count = 3U;
        config.current_phase_mask = 7U;
        config.sample_bus = true;
        config.trigger_selector = 14U << ADC_CR2_JEXTSEL_Pos;
        nx_request_initialize(&session.base);
    }
    void prepared() {
        ASSERT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_SUCCESS);
        nx_power_stage_plan_t initial = {};
        initial.duty_ticks[0] = initial.duty_ticks[1] = initial.duty_ticks[2] =
            20U;
        if (config.pwm.center_aligned) {
            for (auto& start : initial.start_ticks)
                start = config.pwm.period_ticks - 10U;
            computation.start_tick = config.pwm.period_ticks - 10U;
        }
        initial.trigger_ticks = config.trigger_ticks;
        initial.current_phase_mask = config.current_phase_mask;
        computation.trigger = config.trigger_ticks;
        computation.phase_mask = config.current_phase_mask;
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
    void overflow(nx_time_us_t now) {
        hardware.now = now;
        hardware.timer.CNT = config.pwm.period_ticks / 2U;
        hardware.timer.CR1 |= TIM_CR1_DIR;
        hardware.timer.SR |= TIM_SR_UIF;
        hardware.isr = true;
        nx_stm32_power_stage_update_irq(&owner);
        hardware.isr = false;
    }
    void irq(nx_time_us_t now = 67) {
        hardware.now = now;
        hardware.timer.CNT =
            static_cast<uint32_t>((now - 4U) % config.pwm.period_ticks);
        if (config.pwm.center_aligned) {
            hardware.timer.CNT = config.pwm.period_ticks - hardware.timer.CNT;
            hardware.timer.CR1 |= TIM_CR1_DIR;
        }
        hardware.adc.SR |= ADC_SR_JEOC;
        const uint32_t ranks =
            static_cast<uint32_t>((hardware.adc.JSQR & ADC_JSQR_JL) >>
                                  ADC_JSQR_JL_Pos) +
            1U;
        volatile uint32_t* result[4] = {&hardware.adc.JDR1, &hardware.adc.JDR2,
                                        &hardware.adc.JDR3, &hardware.adc.JDR4};
        const uint32_t offsets[4] = {hardware.adc.JOFR1, hardware.adc.JOFR2,
                                     hardware.adc.JOFR3, hardware.adc.JOFR4};
        for (uint32_t i = 0U; i < ranks; ++i) {
            if (i != 0U && (hardware.adc.CR1 & ADC_CR1_SCAN) == 0U)
                break;
            const uint32_t channel =
                (hardware.adc.JSQR >> ((4U - ranks + i) * 5U)) & 31U;
            const uint32_t value =
                config.sample_bus && channel == channels[config.current_count]
                    ? 2000U
                    : 1000U + channel * 100U;
            *result[i] = value - offsets[i];
        }
        hardware.encoder.CNT = 123;
        hardware.isr = true;
        if (config.domain != nullptr)
            nx_stm32_power_stage_domain_adc_irq(config.domain);
        else
            nx_stm32_power_stage_adc_irq(&owner);
        hardware.isr = false;
    }
};
class SharedPowerStage : public PowerStage {
  protected:
    nx_stm32_power_stage_t peer = {};
    nx_stm32_power_stage_config_t peer_config = {};
    nx_power_stage_session_t peer_session = {};
    nx_power_stage_t peer_port = {&nx_stm32_power_stage_ops, &peer};
    Computation peer_computation;
    nx_stm32_power_stage_domain_t domain = {};
    nx_stm32_power_stage_t* members[2] = {&owner, &peer};
    const nx_stm32_power_stage_config_t* selections[2] = {&config,
                                                          &peer_config};
    nx_stm32_power_stage_domain_config_t selection = {members, selections, 2U,
                                                      false, 0U};
    nx_stm32_av_pin_t peer_pwm[7] = {}, peer_analog[2] = {};
    uint16_t peer_snapshots[9] = {};
    uint8_t peer_channels[2] = {4U, 5U}, peer_times[2] = {};
    void SetUp() override {
        PowerStage::SetUp();
        config.encoder = nullptr;
        config.encoder_pins = nullptr;
        config.current_count = 2U;
        config.current_phase_mask = 3U;
        config.sample_bus = false;
        config.analog.channel_count = 2U;
        config.domain = &domain;
        peer_config = config;
        peer_config.adc_index = 1U;
        peer_config.pwm.registers = &hardware.other_timer;
        peer_config.pwm.controller = 1U;
        peer_config.pwm.irq = TIM1_BRK_TIM9_IRQn;
        peer_config.pwm.break_input = &hardware.e.IDR;
        peer_config.pwm.break_mask = 1U << 15;
        peer_config.trigger_selector = 0U;
        peer_config.trigger_ticks = 40U;
        peer_config.maximum_duty_ticks = 25U;
        peer_config.analog.adc = &hardware.adc2;
        peer_config.analog.peers[0] = &hardware.adc;
        peer_config.analog.peers[1] = &hardware.adc3;
        peer_config.analog.pins = peer_analog;
        peer_config.analog.channels = peer_channels;
        peer_config.analog.sample_times = peer_times;
        for (uint8_t i = 0U; i < 6U; ++i)
            peer_pwm[i] = {&hardware.e, &peer_snapshots[i], 4U,
                           static_cast<uint8_t>(i % 2U == 0U ? 9U + i : 7U + i),
                           1U};
        peer_pwm[6] = {&hardware.e, &peer_snapshots[6], 4U, 15U, 1U};
        peer_config.pwm.common.pins = peer_pwm;
        for (uint8_t i = 0U; i < 2U; ++i)
            peer_analog[i] = {&hardware.a, &peer_snapshots[7U + i], 0U,
                              static_cast<uint8_t>(4U + i), 0U};
        hardware.rcc.AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
        nx_request_initialize(&peer_session.base);
    }
    void both_running() {
        ASSERT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
                  NX_SUCCESS);
        ASSERT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_power_stage_prepare(&peer, &peer_config),
                  NX_SUCCESS);
        nx_power_stage_plan_t initial = {};
        initial.duty_ticks[0] = initial.duty_ticks[1] = initial.duty_ticks[2] =
            20U;
        initial.trigger_ticks = config.trigger_ticks;
        initial.current_phase_mask = config.current_phase_mask;
        computation.trigger = config.trigger_ticks;
        computation.phase_mask = config.current_phase_mask;
        ASSERT_EQ(nx_power_stage_prepare(&session, &initial, compute,
                                         &computation, sizeof(computation),
                                         20U),
                  NX_SUCCESS);
        initial.trigger_ticks = peer_config.trigger_ticks;
        initial.current_phase_mask = peer_config.current_phase_mask;
        peer_computation.trigger = peer_config.trigger_ticks;
        peer_computation.phase_mask = peer_config.current_phase_mask;
        ASSERT_EQ(nx_power_stage_prepare(&peer_session, &initial, compute,
                                         &peer_computation,
                                         sizeof(peer_computation), 20U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_power_stage_start(&port, &session), NX_SUCCESS);
        ASSERT_EQ(nx_power_stage_start(&peer_port, &peer_session), NX_SUCCESS);
        hardware.now = 4U;
        ASSERT_EQ(nx_power_stage_service(&port), NX_SUCCESS);
        hardware.opening_owner = &peer;
        ASSERT_EQ(nx_power_stage_service(&peer_port), NX_SUCCESS);
        hardware.opening_owner = nullptr;
    }
    void peer_irq(nx_time_us_t now, bool updated = false) {
        hardware.now = now;
        hardware.other_timer.CNT = 43U;
        if (updated)
            hardware.other_timer.SR |= TIM_SR_UIF;
        hardware.adc2.JDR1 = 1400U;
        hardware.adc2.JDR2 = 1500U;
        hardware.adc2.SR |= ADC_SR_JEOC;
        hardware.isr = true;
        nx_stm32_power_stage_domain_adc_irq(&domain);
        hardware.isr = false;
    }
};
TEST_F(SharedPowerStage, StoppingOneAxisPreservesTheOtherAdcAndIrq) {
    both_running();
    peer_irq(47U);
    ASSERT_EQ(peer_computation.calls, 1U);
    ASSERT_EQ(nx_power_stage_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
    EXPECT_NE(hardware.other_timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_NE(hardware.adc2.CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(hardware.enabled[ADC_IRQn], 1U);
    ASSERT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
    EXPECT_NE(hardware.rcc.APB2ENR & RCC_APB2ENR_ADC2EN, 0U);
    EXPECT_EQ(nx_stm32_power_stage_domain_close(&domain), NX_ERROR_BUSY);
    peer_irq(147U, true);
    EXPECT_EQ(peer_computation.calls, 2U);
    EXPECT_EQ(nx_power_stage_stop(&peer_port), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_power_stage_close(&peer), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_power_stage_domain_close(&domain), NX_SUCCESS);
}
TEST_F(SharedPowerStage, LateWithdrawnAxisFlagsCannotReenterItsContext) {
    both_running();
    ASSERT_EQ(nx_power_stage_stop(&port), NX_SUCCESS);
    hardware.adc.SR |= ADC_SR_JEOC;
    peer_irq(47U);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(peer_computation.calls, 1U);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
}
TEST_F(SharedPowerStage, TwoAxesCannotBorrowTheSameAlgorithmContext) {
    ASSERT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_power_stage_prepare(&peer, &peer_config), NX_SUCCESS);
    nx_power_stage_plan_t initial = {};
    initial.duty_ticks[0] = initial.duty_ticks[1] = initial.duty_ticks[2] = 20U;
    initial.trigger_ticks = config.trigger_ticks;
    initial.current_phase_mask = config.current_phase_mask;
    ASSERT_EQ(nx_power_stage_prepare(&session, &initial, compute, &computation,
                                     sizeof(computation), 20U),
              NX_SUCCESS);
    ASSERT_EQ(nx_power_stage_prepare(&peer_session, &initial, compute,
                                     &computation, sizeof(computation), 20U),
              NX_SUCCESS);
    ASSERT_EQ(nx_power_stage_start(&port, &session), NX_SUCCESS);
    EXPECT_EQ(nx_power_stage_start(&peer_port, &peer_session),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(nx_request_state(&peer_session.base), NX_REQUEST_READY);
}
TEST_F(SharedPowerStage, UnplannedCommonModeChangeInhibitsEveryAffectedAxis) {
    both_running();
    hardware.common.CCR |= ADC_CCR_MULTI_0;
    peer_irq(47U);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(peer_computation.calls, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(hardware.other_timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(SharedPowerStage, PriorityRestoreFailureCanRetryAfterGatesAreOff) {
    ASSERT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
              NX_SUCCESS);
    hardware.priority[ADC_IRQn] = 7U;
    hardware.refuse_priority = true;
    EXPECT_EQ(nx_stm32_power_stage_domain_close(&domain), NX_ERROR_IO);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
    hardware.refuse_priority = false;
    EXPECT_EQ(nx_stm32_power_stage_domain_close(&domain), NX_SUCCESS);
    EXPECT_FALSE(domain.owned);
    EXPECT_EQ(hardware.priority[ADC_IRQn], 0U);
}
TEST_F(SharedPowerStage, CaptureSourceIsEnabledOnlyAfterItsPublisherIsReady) {
    both_running();
    EXPECT_EQ(hardware.unready_sources, 0U);
    peer_irq(47U);
    EXPECT_EQ(peer_computation.calls, 1U);
}
TEST_F(SharedPowerStage,
       RefusedGateAcquireCanCloseWithoutReadingUnclockedAdcs) {
    hardware.refused = &hardware.rcc.APB2ENR;
    EXPECT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
              NX_ERROR_IO);
    EXPECT_TRUE(domain.owned);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_power_stage_domain_close(&domain), NX_SUCCESS);
    EXPECT_FALSE(domain.owned);
    EXPECT_EQ(hardware.gated_adc_accesses, 0U);
}
TEST_F(SharedPowerStage, DuplicatePhysicalAdcCannotMasqueradeAsAnotherIndex) {
    peer_config.analog = config.analog;
    EXPECT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
              NX_ERROR_INVALID);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
class PairedPowerStage : public PowerStage {
  protected:
    nx_stm32_adc_pair_config_t pair = {};
    nx_stm32_power_stage_domain_t domain = {};
    nx_stm32_power_stage_t* members[1] = {&owner};
    const nx_stm32_power_stage_config_t* selections[1] = {&config};
    nx_stm32_power_stage_domain_config_t selection = {members, selections, 1U,
                                                      false, 0U};
    void SetUp() override {
        PowerStage::SetUp();
        config.encoder = nullptr;
        config.encoder_pins = nullptr;
        config.current_count = 2U;
        config.current_phase_mask = 3U;
        config.sample_bus = false;
        config.analog.channel_count = 2U;
        config.domain = &domain;
        pair.master = &hardware.adc;
        pair.slave = &hardware.adc2;
        pair.common = &hardware.common;
        pair.channels[0] = 0U;
        pair.channels[1] = 1U;
        config.pair = &pair;
    }
    void paired_running() {
        ASSERT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
                  NX_SUCCESS);
        running();
    }
    void capture(bool slave_ready, nx_time_us_t now = 67U) {
        hardware.now = now;
        hardware.timer.CNT = static_cast<uint32_t>((now - 4U) % 100U);
        hardware.adc.SR |= ADC_SR_JEOC;
        hardware.adc.JDR1 = 1000U;
        if (slave_ready) {
            hardware.adc2.SR |= ADC_SR_JEOC;
            hardware.adc2.JDR1 = 1200U;
        }
        hardware.isr = true;
        nx_stm32_power_stage_domain_adc_irq(&domain);
        hardware.isr = false;
    }
};
TEST_F(PairedPowerStage, SimultaneousRanksReachTheOrdinaryControlCallback) {
    paired_running();
    ASSERT_TRUE(owner.running);
    EXPECT_EQ(hardware.common.CCR & ADC_CCR_MULTI, 5U);
    EXPECT_EQ(hardware.adc.JSQR & ADC_JSQR_JL, 0U);
    EXPECT_EQ(hardware.adc2.JSQR & ADC_JSQR_JL, 0U);
    EXPECT_EQ(hardware.adc2.CR1 & ADC_CR1_JEOCIE, 0U);
    capture(true);
    ASSERT_EQ(computation.calls, 1U);
    EXPECT_EQ(computation.sample.current_raw[0], 1000U);
    EXPECT_EQ(computation.sample.current_raw[1], 1200U);
    EXPECT_EQ(computation.sample.current_count, 2U);
    EXPECT_EQ(computation.sample.current_phase_mask, 3U);
    EXPECT_FALSE(computation.sample.bus_valid);
    EXPECT_EQ(owner.staged_sequence, 1U);
}
TEST_F(PairedPowerStage, MissingSlaveCompletionHasABoundedIrqRetry) {
    paired_running();
    ASSERT_TRUE(owner.running);
    capture(false);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_NE(hardware.adc.SR & ADC_SR_JEOC, 0U);
    capture(false, 90U);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(hardware.adc.CR1 & ADC_CR1_JEOCIE, 0U);
    EXPECT_NE(nx_request_state(&session.base), NX_REQUEST_SETTLED);
}
class TimeSharedPowerStage : public SharedPowerStage {
  protected:
    void SetUp() override {
        SharedPowerStage::SetUp();
        config.pwm.center_aligned = true;
        peer_config.pwm.center_aligned = true;
        peer_config.trigger_ticks = 60U;
        peer_config.adc_index = 0U;
        peer_config.analog.adc = &hardware.adc;
        peer_config.analog.peers[0] = &hardware.adc2;
        peer_config.analog.peers[1] = &hardware.adc3;
        peer_config.phase_offset_ticks = 50U;
        selection.time_shared = true;
        selection.start_tolerance_us = 2U;
    }
    void accepted(bool accept_peer = true) {
        ASSERT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
                  NX_SUCCESS);
        ASSERT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_power_stage_prepare(&peer, &peer_config),
                  NX_SUCCESS);
        nx_power_stage_plan_t initial = {};
        for (size_t i = 0U; i < 3U; ++i) {
            initial.duty_ticks[i] = 20U;
            initial.start_ticks[i] = 90U;
        }
        initial.trigger_ticks = 60U;
        initial.current_phase_mask = 3U;
        computation.trigger = peer_computation.trigger = 60U;
        computation.phase_mask = peer_computation.phase_mask = 3U;
        computation.start_tick = peer_computation.start_tick = 90U;
        ASSERT_EQ(nx_power_stage_prepare(&session, &initial, compute,
                                         &computation, sizeof(computation),
                                         200U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_power_stage_prepare(&peer_session, &initial, compute,
                                         &peer_computation,
                                         sizeof(peer_computation), 200U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_power_stage_start(&port, &session), NX_SUCCESS);
        if (accept_peer) {
            ASSERT_EQ(nx_power_stage_start(&peer_port, &peer_session),
                      NX_SUCCESS);
        }
        hardware.now = 4U;
        ASSERT_EQ(nx_power_stage_service(&port), NX_SUCCESS);
        if (accept_peer) {
            ASSERT_EQ(nx_power_stage_service(&peer_port), NX_SUCCESS);
        }
    }
    void synchronized() {
        accepted();
        ASSERT_TRUE(owner.running);
        EXPECT_FALSE(peer.running);
        overflow(54U);
        ASSERT_EQ(nx_power_stage_service(&peer_port), NX_SUCCESS);
        ASSERT_TRUE(peer.running);
        irq();
    }
    void peer_overflow() {
        hardware.now = 104U;
        hardware.other_timer.CNT = 50U;
        hardware.other_timer.CR1 |= TIM_CR1_DIR;
        hardware.other_timer.SR |= TIM_SR_UIF;
        hardware.isr = true;
        nx_stm32_power_stage_update_irq(&peer);
        hardware.isr = false;
    }
    void peer_capture() {
        hardware.now = 117U;
        hardware.other_timer.CNT = 37U;
        hardware.adc.JDR1 = 1400U;
        hardware.adc.JDR2 = 1500U;
        hardware.adc.SR |= ADC_SR_JEOC;
        hardware.isr = true;
        nx_stm32_power_stage_domain_adc_irq(&domain);
        hardware.isr = false;
    }
};
TEST_F(TimeSharedPowerStage,
       LateDomainIrqAfterGateReturnDoesNotReadTheConverter) {
    ASSERT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
              NX_SUCCESS);
    hardware.priority[ADC_IRQn] = 7U;
    hardware.refuse_priority = true;
    ASSERT_EQ(nx_stm32_power_stage_domain_close(&domain), NX_ERROR_IO);
    hardware.isr = true;
    nx_stm32_power_stage_domain_adc_irq(&domain);
    hardware.isr = false;
    EXPECT_EQ(hardware.gated_adc_accesses, 0U);
}
TEST_F(TimeSharedPowerStage, OneAcceptedMemberCannotEnergizeAnIncompleteGroup) {
    accepted(false);
    EXPECT_FALSE(owner.running);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    hardware.now = 201U;
    EXPECT_EQ(nx_power_stage_service(&port), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
}
TEST_F(TimeSharedPowerStage, ExplicitSlotOwnerSelectsChannelsAndCallback) {
    synchronized();
    ASSERT_EQ(computation.calls, 1U);
    EXPECT_EQ(peer_computation.calls, 0U);
    peer_overflow();
    EXPECT_EQ((hardware.adc.CR2 & ADC_CR2_JEXTSEL), 0U);
    EXPECT_EQ((hardware.adc.JSQR >> 10U) & 31U, 4U);
    EXPECT_EQ((hardware.adc.JSQR >> 15U) & 31U, 5U);
    peer_capture();
    EXPECT_EQ(computation.calls, 1U);
    ASSERT_EQ(peer_computation.calls, 1U);
    EXPECT_EQ(peer_computation.sample.current_raw[0], 1400U);
    EXPECT_EQ(peer_computation.sample.current_raw[1], 1500U);
    EXPECT_EQ(peer_computation.sample.sequence, 0U);
}
TEST_F(TimeSharedPowerStage, StoppingOneAxisCannotWithdrawAnotherActiveSlot) {
    synchronized();
    peer_overflow();
    ASSERT_EQ(nx_power_stage_stop(&port), NX_SUCCESS);
    EXPECT_NE(hardware.adc.CR2 & ADC_CR2_JEXTEN, 0U);
    EXPECT_NE(hardware.adc.CR2 & ADC_CR2_ADON, 0U);
    peer_capture();
    EXPECT_EQ(peer_computation.calls, 1U);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
}
TEST_F(TimeSharedPowerStage,
       UnownedResultsFaultTheGroupRatherThanGuessingAnAxis) {
    synchronized();
    hardware.adc.SR |= ADC_SR_JEOC;
    nx_stm32_power_stage_domain_adc_irq(&domain);
    EXPECT_EQ(computation.calls, 1U);
    EXPECT_EQ(peer_computation.calls, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(hardware.other_timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(TimeSharedPowerStage,
       SlotBudgetAlsoReservesBothCoreAnchorQuantizations) {
    config.compute_budget_us = 24U;
    config.commit_before_ticks = 98U;
    EXPECT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
              NX_ERROR_INVALID);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(TimeSharedPowerStage, OverlappingReservedSlotsRejectBeforeTakingClocks) {
    config.compute_budget_us = 27U;
    config.commit_before_ticks = 98U;
    EXPECT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
              NX_ERROR_INVALID);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PowerStage, ElectricalSessionDoesNotRequireAQuadratureEncoder) {
    config.encoder = nullptr;
    config.encoder_pins = nullptr;
    prepared();
    ASSERT_TRUE(owner.initialized);
    EXPECT_EQ(hardware.rcc.APB1ENR & RCC_APB1ENR_TIM4EN, 0U);
    EXPECT_EQ(hardware.encoder_updates, 0U);
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
}
TEST_F(PowerStage, TwoCurrentProfileHasNoInventedBusOrEncoderObservation) {
    config.encoder = nullptr;
    config.encoder_pins = nullptr;
    config.current_count = 2U;
    config.current_phase_mask = 3U;
    config.sample_bus = false;
    config.analog.channel_count = 2U;
    running();
    irq();
    ASSERT_EQ(computation.calls, 1U);
    EXPECT_EQ(computation.sample.current_count, 2U);
    EXPECT_EQ(computation.sample.current_phase_mask, 3U);
    EXPECT_FALSE(computation.sample.bus_valid);
    EXPECT_FALSE(computation.sample.encoder_valid);
    EXPECT_EQ(computation.sample.current_raw[2], 0U);
    EXPECT_EQ(computation.sample.bus_raw, 0U);
    EXPECT_EQ(computation.sample.plan_sequence, 0U);
}
TEST_F(PowerStage, SharedDomainRejectsMissingMemberMetadataBeforeWrites) {
    nx_stm32_power_stage_domain_t domain = {};
    const nx_stm32_power_stage_domain_config_t selection = {};
    EXPECT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
              NX_ERROR_INVALID);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PowerStage, StateExposesTheActualColdEpochBeforeStartingAQueue) {
    prepared();
    nx_power_stage_state_t state = {};
    ASSERT_EQ(nx_power_stage_state(&port, &state), NX_SUCCESS);
    EXPECT_EQ(state.clock_epoch, g_nx_stm32_system.time_epoch);
}
TEST_F(SharedPowerStage,
       SharedDomainOwnsOnlySelectedAdcGatesAndRestoresCommon) {
    hardware.common.CCR = ADC_CCR_ADCPRE_0;
    ASSERT_EQ(nx_stm32_power_stage_domain_prepare(&domain, &selection),
              NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.APB2ENR & (RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN |
                                      RCC_APB2ENR_ADC3EN),
              RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN);
    EXPECT_EQ(nx_stm32_power_stage_domain_close(&domain), NX_SUCCESS);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
    EXPECT_EQ(hardware.common.CCR, ADC_CCR_ADCPRE_0);
}
TEST_F(PowerStage, OutputAndNextCaptureUseTheSameStagedPlanIdentity) {
    running();
    computation.trigger = 70U;
    irq();
    ASSERT_EQ(computation.calls, 1U);
    EXPECT_EQ(computation.sample.plan_sequence, 0U);
    EXPECT_EQ(hardware.timer.CCR4, 70U);
    hardware.timer.SR |= TIM_SR_UIF;
    irq(177U);
    ASSERT_EQ(computation.calls, 2U);
    EXPECT_EQ(computation.sample.plan_sequence, 1U);
    EXPECT_EQ(computation.sample.trigger_ticks, 70U);
}
TEST_F(PowerStage, WrongGenerationNeverStagesACompleteButStalePlan) {
    running();
    computation.sequence_delta = 2U;
    computation.duty = 30U;
    irq();
    EXPECT_EQ(hardware.timer.CCR1, 20U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, UnsupportedPhaseShiftIsRejectedRatherThanIgnored) {
    running();
    computation.start_tick = 1U;
    irq();
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, CenterCarrierUsesFullPeriodAndDownSlopeCapture) {
    config.pwm.center_aligned = true;
    running();
    ASSERT_TRUE(owner.running);
    EXPECT_EQ(hardware.timer.ARR, 50U);
    EXPECT_EQ(hardware.timer.RCR, 0U);
    EXPECT_EQ(hardware.timer.CCR1, 10U);
    EXPECT_EQ(hardware.timer.CCR4, 40U);
    EXPECT_EQ(hardware.timer.CR1 & TIM_CR1_CMS, TIM_CR1_CMS_0);
    EXPECT_EQ(hardware.adc.CR2 & ADC_CR2_JEXTEN, 0U);
    overflow(54U);
    EXPECT_NE(hardware.adc.CR2 & ADC_CR2_JEXTEN, 0U);
    irq();
    ASSERT_EQ(computation.calls, 1U);
    EXPECT_EQ(computation.sample.planned_apply_at, 104U);
    EXPECT_EQ(hardware.timer.CCR1, 10U);
    EXPECT_EQ(hardware.timer.CCR4, 40U);
    EXPECT_EQ(hardware.adc.CR2 & ADC_CR2_JEXTEN, 0U);
    overflow(154U);
    irq(167U);
    EXPECT_EQ(computation.calls, 2U);
}
TEST_F(PowerStage, CenterCarrierRejectsAnUpSlopeCapture) {
    config.pwm.center_aligned = true;
    running();
    ASSERT_TRUE(owner.running);
    overflow(54U);
    hardware.now = 67U;
    hardware.timer.CNT = 37U;
    hardware.timer.CR1 &= ~static_cast<uint32_t>(TIM_CR1_DIR);
    hardware.adc.SR |= ADC_SR_JEOC;
    nx_stm32_power_stage_adc_irq(&owner);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, CenterUpdatePublisherMustJoinBeforeSessionSettlement) {
    config.pwm.center_aligned = true;
    running();
    ASSERT_TRUE(owner.running);
    hardware.active[TIM8_UP_TIM13_IRQn] = 1U;
    EXPECT_EQ(nx_power_stage_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_QUARANTINED);
    hardware.active[TIM8_UP_TIM13_IRQn] = 0U;
    EXPECT_EQ(nx_power_stage_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
}
TEST_F(PowerStage, CenterCannotArmAnotherCycleWhilePreviousCaptureIsMissing) {
    config.pwm.center_aligned = true;
    running();
    ASSERT_TRUE(owner.running);
    overflow(54U);
    overflow(154U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(computation.calls, 0U);
}
TEST_F(PowerStage, SelectedPhasesChangeRealRanksAndTheLogicalCaptureLayout) {
    running();
    computation.phase_mask = 5U;
    irq();
    ASSERT_EQ(computation.calls, 1U);
    ASSERT_EQ((hardware.adc.JSQR & ADC_JSQR_JL) >> ADC_JSQR_JL_Pos, 2U);
    EXPECT_EQ((hardware.adc.JSQR >> 5U) & 31U, 0U);
    EXPECT_EQ((hardware.adc.JSQR >> 10U) & 31U, 2U);
    EXPECT_EQ((hardware.adc.JSQR >> 15U) & 31U, 3U);
    hardware.timer.SR |= TIM_SR_UIF;
    irq(167U);
    ASSERT_EQ(computation.calls, 2U);
    EXPECT_EQ(computation.sample.current_count, 2U);
    EXPECT_EQ(computation.sample.current_phase_mask, 5U);
    EXPECT_EQ(computation.sample.current_raw[0], 1000U);
    EXPECT_EQ(computation.sample.current_raw[1], 0U);
    EXPECT_EQ(computation.sample.current_raw[2], 1200U);
    EXPECT_EQ(computation.sample.bus_raw, 2000U);
    EXPECT_EQ(computation.sample.plan_sequence, 1U);
}
TEST_F(PowerStage, APlanCannotSelectAnUnwiredCurrentPhase) {
    config.current_count = 2U;
    config.current_phase_mask = 3U;
    config.sample_bus = false;
    config.analog.channel_count = 2U;
    running();
    computation.phase_mask = 5U;
    irq();
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ((hardware.adc.JSQR & ADC_JSQR_JL) >> ADC_JSQR_JL_Pos, 1U);
}
TEST_F(PowerStage, TimingFactsLetConsumersUseTheExactSelectedBudgets) {
    prepared();
    nx_power_stage_state_t state = {};
    ASSERT_EQ(nx_power_stage_state(&port, &state), NX_SUCCESS);
    EXPECT_EQ(state.deadtime_ticks, 1U);
    EXPECT_EQ(state.settling_ticks, config.settling_ticks);
    EXPECT_GT(state.aperture_ticks, 0U);
    EXPECT_LE(state.aperture_ticks, state.conversion_ticks);
    EXPECT_EQ(state.irq_ticks, 5U);
    EXPECT_EQ(state.compute_ticks, 10U);
    EXPECT_EQ(state.rearm_ticks, 4U);
    EXPECT_EQ(state.commit_before_ticks, 95U);
}
TEST_F(PowerStage, CenterRefusesAnAlreadyOwnedUpdateVectorPeer) {
    config.pwm.center_aligned = true;
    hardware.rcc.APB1ENR |= RCC_APB1ENR_TIM13EN;
    EXPECT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_ERROR_BUSY);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(PowerStage, CenterPriorityFailureRetainsColdOwnerUntilRestored) {
    config.pwm.center_aligned = true;
    running();
    ASSERT_TRUE(owner.running);
    hardware.refused_priority_vector = TIM8_UP_TIM13_IRQn;
    hardware.refuse_priority = true;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_ERROR_IO);
    EXPECT_TRUE(owner.owned);
    hardware.refuse_priority = false;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.priority[TIM8_UP_TIM13_IRQn], 0U);
}
TEST_F(PowerStage, ConcurrentTripCannotPublishAPlanAfterItsFinalWriteFence) {
    running();
    computation.duty = 30U;
    hardware.compare_fault_owner = &owner;
    irq();
    EXPECT_EQ(owner.staged_sequence, 0U);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(PowerStage, WarmupStartsAfterTheActualConverterEnableWrite) {
    prepared();
    hardware.adc_enable_delay = 3U;
    ASSERT_EQ(nx_power_stage_start(&port, &session), NX_SUCCESS);
    EXPECT_EQ(hardware.now, 3U);
    hardware.now = 4U;
    EXPECT_EQ(nx_power_stage_service(&port), NX_SUCCESS);
    EXPECT_FALSE(owner.running);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    hardware.now = 7U;
    EXPECT_EQ(nx_power_stage_service(&port), NX_SUCCESS);
    EXPECT_TRUE(owner.running);
}
class DcPowerStage : public PowerStage {
  protected:
    union CaptureStorage {
        nx_stm32_power_stage_dc_t capture;
        nx_power_stage_state_t query;
    } dc_storage = {};
    nx_stm32_power_stage_dc_config_t dc_config = {};
    void SetUp() override {
        PowerStage::SetUp();
        config.current_count = 0U;
        config.current_phase_mask = 0U;
        config.sample_bus = false;
        config.analog.channel_count = 1U;
        config.pwm.immediate_trigger = true;
        config.trigger_ticks = 30U;
        config.maximum_duty_ticks = 70U;
        dc_config.capture = {&hardware.timer,         &hardware.adc, 0U, 0U,
                             config.trigger_selector, 100U,          4U, 6U};
        dc_config.state = &dc_storage.capture;
        dc_config.settling_ticks = 3U;
        dc_config.aperture_ticks = 1U;
        config.dc = &dc_config;
        computation.dc_profile = true;
        computation.phase_mask = 0U;
        computation.trigger = 30U;
    }
    void dc_prepared() {
        ASSERT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_SUCCESS);
        nx_power_stage_plan_t initial = {};
        initial.duty_ticks[0] = 20U;
        initial.duty_ticks[1] = 40U;
        initial.duty_ticks[2] = 60U;
        initial.trigger_ticks = 30U;
        initial.dc = &computation.dc_plan;
        ASSERT_EQ(nx_power_stage_prepare(&session, &initial, compute,
                                         &computation, sizeof(computation),
                                         20U),
                  NX_SUCCESS);
    }
    void dc_running() {
        dc_prepared();
        ASSERT_EQ(nx_power_stage_start(&port, &session), NX_SUCCESS);
        hardware.now = 4U;
        ASSERT_EQ(nx_power_stage_service(&port), NX_SUCCESS);
    }
    void dc_irq(nx_time_us_t now, uint16_t code) {
        hardware.now = now;
        hardware.timer.CNT = static_cast<uint32_t>((now - 4U) % 100U);
        hardware.adc.JDR1 = code;
        hardware.adc.SR |= ADC_SR_JEOC | ADC_SR_JSTRT;
        hardware.isr = true;
        nx_stm32_power_stage_adc_irq(&owner);
        hardware.isr = false;
    }
};
TEST_F(DcPowerStage,
       TwoActiveVectorCapturesComputeExactlyOnceAndRearmAtUpdate) {
    dc_running();
    ASSERT_TRUE(owner.running);
    EXPECT_EQ(hardware.timer.CCMR2 & TIM_CCMR2_OC4PE, 0U);
    dc_irq(34U, 2100U);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(hardware.timer.CCR4, 50U);
    dc_irq(54U, 1900U);
    ASSERT_EQ(computation.calls, 1U);
    EXPECT_EQ(computation.sample.current_count, 0U);
    EXPECT_FALSE(computation.sample.bus_valid);
    EXPECT_EQ(computation.dc_capture.code[0], 2100U);
    EXPECT_EQ(computation.dc_capture.code[1], 1900U);
    EXPECT_EQ(computation.dc_capture.sequence, 0U);
    EXPECT_EQ(computation.sample.scan_span_us, 21U);
    EXPECT_EQ(hardware.adc.CR2 & ADC_CR2_JEXTEN, 0U);
    hardware.now = 104U;
    hardware.timer.CNT = 0U;
    hardware.timer.SR |= TIM_SR_UIF;
    hardware.isr = true;
    nx_stm32_power_stage_update_irq(&owner);
    hardware.isr = false;
    EXPECT_NE(hardware.adc.CR2 & ADC_CR2_JEXTEN, 0U);
    dc_irq(134U, 2101U);
    dc_irq(154U, 1899U);
    EXPECT_EQ(computation.calls, 2U);
    EXPECT_EQ(computation.dc_capture.sequence, 1U);
    ASSERT_EQ(nx_power_stage_stop(&port), NX_SUCCESS);
    dc_irq(155U, 2000U);
    EXPECT_EQ(computation.calls, 2U);
}
TEST_F(DcPowerStage, EqualDutyInitialPlanCannotSilentlyInventExcitation) {
    dc_prepared();
    session.initial.duty_ticks[0] = 40U;
    session.initial.duty_ticks[2] = 40U;
    EXPECT_EQ(nx_power_stage_start(&port, &session), NX_ERROR_INVALID);
    EXPECT_EQ(owner.active, nullptr);
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
}
TEST_F(DcPowerStage,
       MissingSecondObservationFailsBeforeAnotherCarrierCanBorrowIt) {
    dc_running();
    dc_irq(34U, 2100U);
    hardware.now = 64U;
    EXPECT_EQ(nx_power_stage_service(&port), NX_ERROR_TIMEOUT);
    EXPECT_EQ(computation.calls, 0U);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(hardware.adc.CR2 & (ADC_CR2_JEXTEN | ADC_CR2_ADON), 0U);
}
TEST_F(DcPowerStage,
       SelectedCaptureStorageCannotBeOverwrittenThroughStateOutput) {
    ASSERT_EQ(nx_stm32_power_stage_prepare(&owner, &config), NX_SUCCESS);
    EXPECT_EQ(nx_power_stage_state(&port, &dc_storage.query), NX_ERROR_INVALID);
}

TEST_F(DcPowerStage,
       CallbackDescriptorOutsideBorrowedContextFaultsWithoutPublishing) {
    dc_running();
    computation.foreign_dc = true;
    dc_irq(34U, 2100U);
    dc_irq(54U, 1900U);
    EXPECT_EQ(owner.staged_sequence, 0U);
    EXPECT_EQ(owner.fault, static_cast<uint32_t>(NX_ERROR_INVALID));
    EXPECT_EQ(hardware.timer.BDTR & TIM_BDTR_MOE, 0U);
    EXPECT_EQ(nx_power_stage_service(&port), NX_ERROR_INVALID);
}
TEST_F(DcPowerStage, CaptureStateCannotAliasTheCarrierOwnerOrImmutableConfig) {
    dc_config.state = reinterpret_cast<nx_stm32_power_stage_dc_t*>(&owner);
    EXPECT_EQ(nx_stm32_power_stage_admit(&owner, &config), NX_ERROR_INVALID);
    dc_config.state = reinterpret_cast<nx_stm32_power_stage_dc_t*>(&config);
    EXPECT_EQ(nx_stm32_power_stage_admit(&owner, &config), NX_ERROR_INVALID);
    EXPECT_EQ(hardware.rcc.APB2ENR, 0U);
}
TEST_F(DcPowerStage, FailedTimerInhibitStillAttemptsConverterWithdrawal) {
    dc_running();
    hardware.refused = &hardware.timer.BDTR;
    EXPECT_EQ(nx_power_stage_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(hardware.adc.CR2 & (ADC_CR2_JEXTEN | ADC_CR2_ADON), 0U);
    EXPECT_NE(owner.active, nullptr);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_power_stage_stop(&port), NX_SUCCESS);
}
TEST_F(DcPowerStage,
       UpdatePriorityReceiptRestoresAndRetriesWithoutLosingOwner) {
    hardware.priority[TIM8_UP_TIM13_IRQn] = 9U;
    dc_running();
    ASSERT_EQ(nx_power_stage_stop(&port), NX_SUCCESS);
    hardware.refuse_priority = true;
    hardware.refused_priority_vector = TIM8_UP_TIM13_IRQn;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_ERROR_IO);
    EXPECT_TRUE(owner.owned);
    hardware.refuse_priority = false;
    EXPECT_EQ(nx_stm32_power_stage_close(&owner), NX_SUCCESS);
    EXPECT_EQ(hardware.priority[TIM8_UP_TIM13_IRQn], 9U);
}
TEST_F(PowerStage, DcPlanDescriptorMustBeInsideTheDeclaredAlgorithmLoan) {
    nx_power_stage_dc_plan_t outside = {};
    outside.sample_ticks[0] = 10U;
    outside.sample_ticks[1] = 40U;
    outside.switching_vector[0] = 4U;
    outside.switching_vector[1] = 6U;
    nx_power_stage_plan_t initial = {};
    initial.dc = &outside;
    EXPECT_EQ(nx_power_stage_prepare(&session, &initial, compute, &computation,
                                     sizeof(computation), 20U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&session.base), NX_REQUEST_READY);
}
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
    const uint32_t previous = *reg;
    if (reg == &hw->adc2.CR2 && (value & ADC_CR2_JEXTEN) != 0U &&
        hw->opening_owner != nullptr &&
        (!hw->opening_owner->running || hw->opening_owner->cycle_anchor != 4U))
        ++hw->unready_sources;
    if (reg == &hw->timer.SR || reg == &hw->other_timer.SR ||
        reg == &hw->adc.SR || reg == &hw->adc2.SR || reg == &hw->adc3.SR ||
        reg == &hw->encoder.SR)
        *reg &= value;
    else
        *reg = value;
    if ((reg == &hw->adc.CR2 || reg == &hw->adc2.CR2) &&
        (previous & ADC_CR2_ADON) == 0U && (value & ADC_CR2_ADON) != 0U)
        hw->now += hw->adc_enable_delay;
    if (reg == &hw->timer.CCR2 && hw->compare_fault_owner != nullptr) {
        hw->a.IDR |= 1U << 6U;
        auto* owner = hw->compare_fault_owner;
        hw->compare_fault_owner = nullptr;
        nx_stm32_power_stage_break_irq(owner);
    }
    if (reg == &hw->timer.EGR && (value & TIM_EGR_UG) != 0)
        hw->timer.SR |= TIM_SR_UIF;
    if (reg == &hw->other_timer.EGR && (value & TIM_EGR_UG) != 0U)
        hw->other_timer.SR |= TIM_SR_UIF;
    if (reg == &hw->encoder.EGR && (value & TIM_EGR_UG) != 0U) {
        hw->encoder.SR |= TIM_SR_UIF;
        ++hw->encoder_updates;
    }
    for (auto* gpio : {&hw->a, &hw->b, &hw->c, &hw->d, &hw->e})
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
    if (!hw->refuse_priority || irq != hw->refused_priority_vector)
        hw->priority[irq] = value;
}
extern "C" uint32_t nx_av_model_irq_priority_read(IRQn_Type irq) {
    return hw->priority[irq];
}
