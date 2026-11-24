/**
 * \file            power_stage_dc_test.cpp
 * \brief           Complete DC-link cycles through real granted ADC operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_capture_audio_model.h"
extern "C" {
#include "drivers/pwm_adc/power_stage_dc.h"
}
#include <gtest/gtest.h>

namespace {
struct Hardware {
    ADC_TypeDef adc = {};
    TIM_TypeDef timer = {};
    volatile uint32_t* refused = nullptr;
    unsigned writes = 0;
    unsigned unready_sources = 0;
    nx_stm32_power_stage_dc_t* owner = nullptr;
} * hw;

class PowerStageDc : public testing::Test {
  protected:
    Hardware hardware;
    nx_stm32_power_stage_dc_t state = {};
    nx_stm32_power_stage_dc_config_t config = {};
    nx_power_stage_dc_plan_t dc = {{300, 500}, {3, 1}};
    nx_power_stage_plan_t plan = {};
    void SetUp() override {
        hw = &hardware;
        hardware.owner = &state;
        config = {{&hardware.timer, &hardware.adc, 12, 3, 0, 1000, 4, 20},
                  &state,
                  10,
                  20};
        plan.duty_ticks[0] = 200;
        plan.duty_ticks[1] = 400;
        plan.duty_ticks[2] = 600;
        plan.trigger_ticks = 300;
        plan.dc = &dc;
        hardware.timer.ARR = 999;
    }
    void ready() {
        ASSERT_EQ(nx_stm32_power_stage_dc_prepare(&config), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_power_stage_dc_stage(&config, &plan), NX_SUCCESS);
        hardware.adc.CR2 |= ADC_CR2_ADON;
        hardware.timer.CR1 |= TIM_CR1_CEN;
        hardware.timer.CNT = 10;
        ASSERT_EQ(nx_stm32_power_stage_dc_arm(&config), NX_SUCCESS);
    }
    void completed(uint32_t count, uint16_t code) {
        hardware.timer.CNT = count;
        hardware.adc.JDR1 = code;
        hardware.adc.SR = ADC_SR_JEOC | ADC_SR_JSTRT;
    }
};

TEST_F(PowerStageDc, ValidatesActualVectorsAndBothBoundedApertures) {
    EXPECT_TRUE(nx_stm32_power_stage_dc_valid(&config, &plan));
    dc.switching_vector[0] = 6;
    EXPECT_FALSE(nx_stm32_power_stage_dc_valid(&config, &plan));
    dc.switching_vector[0] = 3;
    dc.sample_ticks[0] = 210;
    plan.trigger_ticks = 210;
    EXPECT_FALSE(nx_stm32_power_stage_dc_valid(&config, &plan));
    dc.sample_ticks[0] = 380;
    plan.trigger_ticks = 380;
    EXPECT_FALSE(nx_stm32_power_stage_dc_valid(&config, &plan));
    dc.sample_ticks[0] = 300;
    plan.trigger_ticks = 300;
    plan.start_ticks[1] = 1;
    EXPECT_FALSE(nx_stm32_power_stage_dc_valid(&config, &plan));
    EXPECT_EQ(hardware.writes, 0U);
}

TEST_F(PowerStageDc, RejectsRepeatedVectorsPhaseFieldsAndInsufficientRearmGap) {
    dc.sample_ticks[1] = 350;
    dc.switching_vector[1] = 3;
    EXPECT_FALSE(nx_stm32_power_stage_dc_valid(&config, &plan));
    dc = {{300, 500}, {3, 1}};
    plan.current_phase_mask = 3;
    EXPECT_FALSE(nx_stm32_power_stage_dc_valid(&config, &plan));
    plan.current_phase_mask = 0;
    config.capture.maximum_lateness_ticks = 196;
    EXPECT_FALSE(nx_stm32_power_stage_dc_valid(&config, &plan));
    EXPECT_EQ(hardware.writes, 0U);
    config.capture.maximum_lateness_ticks = 20;
    plan.duty_ticks[0] = plan.duty_ticks[1] = plan.duty_ticks[2] = 500;
    EXPECT_FALSE(nx_stm32_power_stage_dc_valid(&config, &plan));
    plan.duty_ticks[0] = plan.duty_ticks[1] = plan.duty_ticks[2] = 0;
    EXPECT_FALSE(nx_stm32_power_stage_dc_valid(&config, &plan));
}

TEST_F(PowerStageDc, ColdPrepareUsesTheGrantedRankAndCopiesNoOwnerResources) {
    ASSERT_EQ(nx_stm32_power_stage_dc_prepare(&config), NX_SUCCESS);
    EXPECT_TRUE(state.prepared);
    EXPECT_EQ(hardware.adc.JSQR, 12U << ADC_JSQR_JSQ4_Pos);
    EXPECT_EQ(hardware.adc.CR2, 0U);
    EXPECT_EQ(hardware.timer.CCR1 | hardware.timer.CCR2 | hardware.timer.CCR3,
              0U);
    state.capture.sequence = 55;
    hardware.adc.CR2 |= ADC_CR2_ADON;
    EXPECT_EQ(nx_stm32_power_stage_dc_prepare(&config), NX_ERROR_BUSY);
    EXPECT_EQ(state.capture.sequence, 55U);
}

TEST_F(PowerStageDc, TwoConversionsPublishOnceAndRetainCopiedDescriptor) {
    ready();
    EXPECT_EQ(hardware.unready_sources, 0U);
    EXPECT_EQ(
        nx_stm32_power_stage_dc_read(
            &config, reinterpret_cast<const nx_power_stage_dc_capture_t**>(
                         &hardware.timer)),
        NX_ERROR_INVALID);
    dc = {{1, 2}, {1, 2}};
    const nx_power_stage_dc_capture_t* output = nullptr;
    EXPECT_EQ(nx_stm32_power_stage_dc_read(&config, &output), NX_ERROR_BUSY);
    completed(305, 1234);
    ASSERT_EQ(nx_stm32_power_stage_dc_read(&config, &output), NX_ERROR_BUSY);
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(hardware.timer.CCR4, 500U);
    EXPECT_EQ(hardware.adc.CR2 & ADC_CR2_JEXTEN, ADC_CR2_JEXTEN_0);
    completed(505, 2345);
    ASSERT_EQ(nx_stm32_power_stage_dc_read(&config, &output), NX_SUCCESS);
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(output->code[0], 1234U);
    EXPECT_EQ(output->code[1], 2345U);
    EXPECT_EQ(output->sample_ticks[0], 300U);
    EXPECT_EQ(output->sample_ticks[1], 500U);
    EXPECT_EQ(output->switching_vector[0], 3U);
    EXPECT_EQ(output->switching_vector[1], 1U);
    EXPECT_EQ(output->sequence, 0U);
    const unsigned writes = hardware.writes;
    EXPECT_EQ(nx_stm32_power_stage_dc_read(&config, &output), NX_ERROR_STATE);
    EXPECT_EQ(hardware.writes, writes);
}

TEST_F(PowerStageDc, OneStagedGenerationArmsOnlyOnceAndNextGenerationIsExact) {
    ready();
    EXPECT_EQ(nx_stm32_power_stage_dc_arm(&config), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_power_stage_dc_stage(&config, &plan), NX_ERROR_BUSY);
    const nx_power_stage_dc_capture_t* output = nullptr;
    completed(305, 100);
    ASSERT_EQ(nx_stm32_power_stage_dc_read(&config, &output), NX_ERROR_BUSY);
    completed(505, 200);
    ASSERT_EQ(nx_stm32_power_stage_dc_read(&config, &output), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_power_stage_dc_arm(&config), NX_ERROR_STATE);
    plan.sequence = 2;
    EXPECT_EQ(nx_stm32_power_stage_dc_stage(&config, &plan), NX_ERROR_STATE);
    plan.sequence = 1;
    ASSERT_EQ(nx_stm32_power_stage_dc_stage(&config, &plan), NX_SUCCESS);
    hardware.timer.CNT = 10;
    EXPECT_EQ(nx_stm32_power_stage_dc_arm(&config), NX_SUCCESS);
    EXPECT_EQ(hardware.timer.CCR4, 300U);
}

TEST_F(PowerStageDc, FirstPublicationMustBeInitialAndPendingStageCannotChange) {
    ASSERT_EQ(nx_stm32_power_stage_dc_prepare(&config), NX_SUCCESS);
    plan.sequence = 1;
    EXPECT_EQ(nx_stm32_power_stage_dc_stage(&config, &plan), NX_ERROR_STATE);
    plan.sequence = 0;
    ASSERT_EQ(nx_stm32_power_stage_dc_stage(&config, &plan), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_power_stage_dc_stage(&config, &plan), NX_ERROR_BUSY);
    EXPECT_EQ(state.capture.sequence, 0U);
}

TEST_F(PowerStageDc, FailedSecondRearmNeverPublishesAHalfPairOrPermitsRetry) {
    ready();
    hardware.refused = &hardware.timer.CCR4;
    completed(305, 1234);
    const nx_power_stage_dc_capture_t* output = nullptr;
    EXPECT_EQ(nx_stm32_power_stage_dc_read(&config, &output), NX_ERROR_IO);
    EXPECT_EQ(output, nullptr);
    hardware.refused = nullptr;
    EXPECT_EQ(nx_stm32_power_stage_dc_read(&config, &output), NX_ERROR_IO);
    EXPECT_EQ(nx_stm32_power_stage_dc_stage(&config, &plan), NX_ERROR_IO);
    EXPECT_EQ(hardware.adc.CR2 & ADC_CR2_JEXTEN, 0U);
}

TEST_F(PowerStageDc,
       LateFirstConversionClosesAdmissionInsteadOfPairingOldData) {
    ready();
    completed(321, 1234);
    const nx_power_stage_dc_capture_t* output = nullptr;
    EXPECT_EQ(nx_stm32_power_stage_dc_read(&config, &output), NX_ERROR_TIMEOUT);
    EXPECT_EQ(output, nullptr);
    EXPECT_EQ(hardware.adc.CR2 & ADC_CR2_JEXTEN, 0U);
}

TEST_F(PowerStageDc, FailedDisableRetainsTheGrantedStateUntilExplicitRetry) {
    ready();
    hardware.refused = &hardware.adc.CR2;
    EXPECT_EQ(nx_stm32_power_stage_dc_disable(&config), NX_ERROR_IO);
    EXPECT_TRUE(state.prepared);
    hardware.refused = nullptr;
    ASSERT_EQ(nx_stm32_power_stage_dc_disable(&config), NX_SUCCESS);
    EXPECT_FALSE(state.prepared);
    EXPECT_EQ(hardware.adc.CR2 & (ADC_CR2_ADON | ADC_CR2_JEXTEN), 0U);
    EXPECT_EQ(nx_stm32_power_stage_dc_arm(&config), NX_ERROR_STATE);
}
} /* namespace */

extern "C" uint32_t nx_av_model_read(volatile uint32_t* reg) {
    return *reg;
}

extern "C" void nx_av_model_write(volatile uint32_t* reg, uint32_t value) {
    ++hw->writes;
    if (reg == &hw->adc.CR2 && (value & ADC_CR2_JEXTEN) != 0U &&
        !hw->owner->armed) {
        ++hw->unready_sources;
    }
    if (reg == hw->refused)
        return;
    if (reg == &hw->adc.SR)
        *reg &= value;
    else
        *reg = value;
}
