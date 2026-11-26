/**
 * \file            single_shunt_test.cpp
 * \brief           Production DC-link capture leaves and rejected rearm paths
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../audio_video/f407_capture_audio_model.h"
extern "C" {
#include "drivers/pwm_adc/single_shunt.h"
}
#include <gtest/gtest.h>

namespace {
struct Registers {
    ADC_TypeDef adc = {};
    TIM_TypeDef timer = {};
    volatile uint32_t* refused = nullptr;
    bool inject_conversion = false;
    bool expire_on_compare = false;
    bool start_on_close = false;
    unsigned writes = 0;
} * model;

class SingleShunt : public testing::Test {
  protected:
    Registers registers;
    nx_stm32_single_shunt_config_t config = {};
    void SetUp() override {
        model = &registers;
        config = {&registers.timer, &registers.adc, 12, 3, 0, 1000, 4, 20};
        registers.timer.ARR = 999;
    }
    void warm() {
        ASSERT_EQ(nx_stm32_single_shunt_prepare(&config), NX_SUCCESS);
        registers.adc.CR2 |= ADC_CR2_ADON;
        registers.timer.CR1 |= TIM_CR1_CEN;
        registers.timer.CNT = 10;
    }
    void completed(uint32_t count = 605) {
        registers.adc.SR = ADC_SR_JEOC | ADC_SR_JSTRT | ADC_SR_EOC;
        registers.adc.JDR1 = 1234;
        registers.timer.CNT = count;
    }
};

TEST_F(SingleShunt, InvalidProfileHasNoRegisterEffects) {
    EXPECT_TRUE(nx_stm32_single_shunt_valid(&config));
    config.channel = 16;
    EXPECT_FALSE(nx_stm32_single_shunt_valid(&config));
    config.channel = 12;
    config.rearm_guard_ticks = 0;
    EXPECT_FALSE(nx_stm32_single_shunt_valid(&config));
    EXPECT_EQ(nx_stm32_single_shunt_prepare(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(registers.writes, 0U);
}

TEST_F(SingleShunt,
       PrepareConfiguresOneRightAlignedInjectedRankWithSourcesClosed) {
    registers.adc.CR1 = ADC_CR1_RES | ADC_CR1_JEOCIE;
    ASSERT_EQ(nx_stm32_single_shunt_prepare(&config), NX_SUCCESS);
    EXPECT_EQ(registers.adc.JSQR, 12U << ADC_JSQR_JSQ4_Pos);
    EXPECT_EQ((registers.adc.SMPR1 >> 6U) & 7U, 3U);
    EXPECT_EQ(registers.adc.CR1, 0U);
    EXPECT_EQ(registers.adc.CR2, 0U);
    EXPECT_EQ(registers.adc.JOFR1 | registers.adc.JOFR2 | registers.adc.JOFR3 |
                  registers.adc.JOFR4,
              0U);
    EXPECT_EQ(
        registers.timer.CCR1 | registers.timer.CCR2 | registers.timer.CCR3, 0U);
}

TEST_F(SingleShunt, LiveConverterAndUnsupportedCarrierHaveNoPrepareEffects) {
    registers.adc.CR2 = ADC_CR2_ADON;
    EXPECT_EQ(nx_stm32_single_shunt_prepare(&config), NX_ERROR_BUSY);
    registers.adc.CR2 = 0;
    registers.timer.CR1 = TIM_CR1_CMS_0;
    EXPECT_EQ(nx_stm32_single_shunt_prepare(&config), NX_ERROR_STATE);
    registers.timer.CR1 = TIM_CR1_DIR;
    EXPECT_EQ(nx_stm32_single_shunt_prepare(&config), NX_ERROR_STATE);
    registers.timer.CR1 = 0;
    registers.timer.CCMR2 = TIM_CCMR2_OC4PE;
    EXPECT_EQ(nx_stm32_single_shunt_prepare(&config), NX_ERROR_STATE);
    EXPECT_EQ(registers.writes, 0U);
}

TEST_F(SingleShunt, ArmWritesImmediateFutureCompareAndOnlySamplingSources) {
    warm();
    registers.timer.CCR1 = 101;
    registers.timer.CCR2 = 202;
    registers.timer.CCR3 = 303;
    ASSERT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_SUCCESS);
    EXPECT_EQ(registers.timer.CCR4, 600U);
    EXPECT_EQ(registers.adc.CR1 & ADC_CR1_JEOCIE, ADC_CR1_JEOCIE);
    EXPECT_EQ(registers.adc.CR2 & ADC_CR2_JEXTEN, ADC_CR2_JEXTEN_0);
    EXPECT_EQ(registers.timer.CCR1, 101U);
    EXPECT_EQ(registers.timer.CCR2, 202U);
    EXPECT_EQ(registers.timer.CCR3, 303U);
}

TEST_F(SingleShunt, PendingOrElapsedCompareCannotBeSilentlyRearmed) {
    warm();
    const unsigned writes = registers.writes;
    registers.timer.CNT = 596;
    EXPECT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_ERROR_TIMEOUT);
    registers.timer.CNT = 10;
    registers.adc.SR = ADC_SR_JEOC;
    EXPECT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_ERROR_BUSY);
    EXPECT_EQ(registers.adc.SR, ADC_SR_JEOC);
    EXPECT_EQ(registers.writes, writes);
}

TEST_F(SingleShunt, ConversionRacingSourceWithdrawalCannotBeOverwritten) {
    warm();
    registers.start_on_close = true;
    EXPECT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_ERROR_BUSY);
    EXPECT_EQ(registers.timer.CCR4, 0U);
    EXPECT_EQ(registers.adc.SR, ADC_SR_JSTRT);
    EXPECT_EQ(registers.adc.CR2 & ADC_CR2_JEXTEN, 0U);
}

TEST_F(SingleShunt, ExpiringCompareDuringRearmLeavesSourceClosed) {
    warm();
    registers.expire_on_compare = true;
    EXPECT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_ERROR_TIMEOUT);
    EXPECT_EQ(registers.adc.CR2 & ADC_CR2_JEXTEN, 0U);
    EXPECT_EQ(registers.adc.CR1 & ADC_CR1_JEOCIE, 0U);
}

TEST_F(SingleShunt, MissingCompletionRetainsFlagsAndCallerOutput) {
    warm();
    ASSERT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_SUCCESS);
    registers.adc.SR = ADC_SR_JSTRT;
    const unsigned writes = registers.writes;
    uint16_t code = 77;
    EXPECT_EQ(nx_stm32_single_shunt_read(&config, 600, &code), NX_ERROR_BUSY);
    EXPECT_EQ(code, 77);
    EXPECT_EQ(registers.adc.SR, ADC_SR_JSTRT);
    EXPECT_EQ(registers.writes, writes);
}

TEST_F(SingleShunt,
       CompletedCodeClosesSamplingButKeepsConverterWarmForSecondArm) {
    warm();
    ASSERT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_SUCCESS);
    completed();
    uint16_t code = 77;
    ASSERT_EQ(nx_stm32_single_shunt_read(&config, 600, &code), NX_SUCCESS);
    EXPECT_EQ(code, 1234);
    EXPECT_EQ(registers.adc.CR2 & ADC_CR2_ADON, ADC_CR2_ADON);
    EXPECT_EQ(registers.adc.CR2 & ADC_CR2_JEXTEN, 0U);
    EXPECT_EQ(registers.adc.CR1 & ADC_CR1_JEOCIE, 0U);
    EXPECT_EQ(registers.adc.SR, ADC_SR_EOC);
    ASSERT_EQ(nx_stm32_single_shunt_arm(&config, 800), NX_SUCCESS);
    EXPECT_EQ(registers.timer.CCR4, 800U);
}

TEST_F(SingleShunt, LateOrWrappedCompletionCannotPublishAStaleCode) {
    warm();
    ASSERT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_SUCCESS);
    uint16_t code = 77;
    completed(621);
    EXPECT_EQ(nx_stm32_single_shunt_read(&config, 600, &code),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(code, 77);
    EXPECT_EQ(registers.adc.CR2 & ADC_CR2_JEXTEN, 0U);
    EXPECT_EQ(registers.adc.CR1 & ADC_CR1_JEOCIE, 0U);
    completed(5);
    EXPECT_EQ(nx_stm32_single_shunt_read(&config, 600, &code),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(code, 77);
}

TEST_F(SingleShunt, NewConversionDuringReadAndIllegalCodesRetainCallerOutput) {
    warm();
    ASSERT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_SUCCESS);
    completed();
    registers.inject_conversion = true;
    uint16_t code = 77;
    EXPECT_EQ(nx_stm32_single_shunt_read(&config, 600, &code),
              NX_ERROR_OVERFLOW);
    EXPECT_EQ(code, 77);
    completed();
    registers.adc.JDR1 = 4096;
    EXPECT_EQ(nx_stm32_single_shunt_read(&config, 600, &code),
              NX_ERROR_INVALID);
    EXPECT_EQ(code, 77);
}

TEST_F(SingleShunt, FailedCompareReadbackRetainsTheOwnersCleanupDuty) {
    warm();
    registers.refused = &registers.timer.CCR4;
    EXPECT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_ERROR_IO);
    EXPECT_EQ(registers.adc.CR2 & ADC_CR2_JEXTEN, 0U);
}

TEST_F(SingleShunt, WithdrawalAttemptsAllSourcesAfterOneWriteFails) {
    warm();
    ASSERT_EQ(nx_stm32_single_shunt_arm(&config, 600), NX_SUCCESS);
    registers.refused = &registers.adc.CR2;
    EXPECT_EQ(nx_stm32_single_shunt_disable(&config), NX_ERROR_IO);
    EXPECT_EQ(registers.adc.CR1 & ADC_CR1_JEOCIE, 0U);
    EXPECT_EQ(registers.adc.SR, 0U);
}
} /* namespace */

extern "C" uint32_t nx_av_model_read(volatile uint32_t* reg) {
    const uint32_t value = *reg;
    if (reg == &model->adc.JDR1 && model->inject_conversion) {
        model->adc.SR |= ADC_SR_JSTRT;
        model->inject_conversion = false;
    }
    return value;
}

extern "C" void nx_av_model_write(volatile uint32_t* reg, uint32_t value) {
    ++model->writes;
    if (reg == model->refused)
        return;
    if (reg == &model->adc.SR)
        *reg &= value;
    else
        *reg = value;
    if (reg == &model->adc.CR1 && model->start_on_close) {
        model->adc.SR |= ADC_SR_JSTRT;
        model->start_on_close = false;
    }
    if (reg == &model->timer.CCR4 && model->expire_on_compare)
        model->timer.CNT = value;
}
