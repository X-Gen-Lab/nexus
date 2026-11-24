/**
 * \file            adc_pair_test.cpp
 * \brief           Production dual-injected ADC operations at MMIO leaves
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_capture_audio_model.h"
extern "C" {
#include "drivers/pwm_adc/adc_pair.h"
}
#include <gtest/gtest.h>

namespace {
struct Registers {
    ADC_TypeDef master = {}, slave = {};
    ADC_Common_TypeDef common = {};
    volatile uint32_t* refused = nullptr;
    bool inject_conversion = false;
    unsigned writes = 0;
} * model;

class AdcPair : public testing::Test {
  protected:
    Registers registers;
    nx_stm32_adc_pair_config_t config = {};
    void SetUp() override {
        model = &registers;
        config = {&registers.master,
                  &registers.slave,
                  &registers.common,
                  {5, 12},
                  {3, 3}};
        registers.common.CCR =
            (ADC_CCR_MULTI_2 | ADC_CCR_MULTI_0) | ADC_CCR_ADCPRE_0;
    }
    void enable() {
        ASSERT_EQ(nx_stm32_adc_pair_prepare(&config), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_adc_pair_enable(&config, (ADC_CR2_JEXTSEL_3 |
                                                     ADC_CR2_JEXTSEL_2 |
                                                     ADC_CR2_JEXTSEL_1)),
                  NX_SUCCESS);
    }
};

TEST_F(AdcPair, StaticProfileRejectsAliasingAndUnequalSampleTimesWithoutMMIO) {
    EXPECT_TRUE(nx_stm32_adc_pair_valid(&config));
    config.slave = config.master;
    EXPECT_FALSE(nx_stm32_adc_pair_valid(&config));
    config.slave = &registers.slave;
    config.sample_times[1] = 4;
    EXPECT_FALSE(nx_stm32_adc_pair_valid(&config));
    EXPECT_EQ(registers.writes, 0U);
}

TEST_F(AdcPair, PrepareUsesRightAlignedSingleInjectedRanksAndPreservesCommon) {
    const auto common = registers.common.CCR;
    registers.master.CR1 = registers.slave.CR1 = ADC_CR1_RES;
    ASSERT_EQ(nx_stm32_adc_pair_prepare(&config), NX_SUCCESS);
    EXPECT_EQ(registers.master.JSQR, 5U << ADC_JSQR_JSQ4_Pos);
    EXPECT_EQ(registers.slave.JSQR, 12U << ADC_JSQR_JSQ4_Pos);
    EXPECT_EQ((registers.master.SMPR2 >> 15U) & 7U, 3U);
    EXPECT_EQ((registers.slave.SMPR1 >> 6U) & 7U, 3U);
    EXPECT_EQ(registers.master.CR1 & ADC_CR1_RES, 0U);
    EXPECT_EQ(registers.slave.CR1 & ADC_CR1_RES, 0U);
    EXPECT_EQ(registers.common.CCR, common);
}

TEST_F(AdcPair, PrepareRejectsLiveConverterWithoutWritingAnyRegisters) {
    registers.slave.CR2 = ADC_CR2_ADON;
    EXPECT_EQ(nx_stm32_adc_pair_prepare(&config), NX_ERROR_BUSY);
    EXPECT_EQ(registers.writes, 0U);
}

TEST_F(AdcPair, WrongCommonModeDoesNotGetSilentlyReconfigured) {
    registers.common.CCR = 0U;
    EXPECT_EQ(nx_stm32_adc_pair_prepare(&config), NX_ERROR_STATE);
    EXPECT_EQ(registers.common.CCR, 0U);
    EXPECT_EQ(registers.writes, 0U);
}

TEST_F(AdcPair, EnableLeavesBothPublisherPathsClosedUntilOwnerIsReady) {
    enable();
    EXPECT_EQ(registers.master.CR2 & ADC_CR2_ADON, ADC_CR2_ADON);
    EXPECT_EQ(registers.slave.CR2 & ADC_CR2_ADON, ADC_CR2_ADON);
    EXPECT_EQ(registers.master.CR2 & ADC_CR2_JEXTSEL,
              static_cast<uint32_t>(
                  (ADC_CR2_JEXTSEL_3 | ADC_CR2_JEXTSEL_2 | ADC_CR2_JEXTSEL_1)));
    EXPECT_EQ(registers.slave.CR2 & ADC_CR2_JEXTSEL, 0U);
    EXPECT_EQ((registers.master.CR2 | registers.slave.CR2) & ADC_CR2_JEXTEN,
              0U);
    EXPECT_EQ((registers.master.CR1 | registers.slave.CR1) & ADC_CR1_JEOCIE,
              0U);
}

TEST_F(AdcPair, HalfReadyPairRetainsFlagsAndCallerOutput) {
    enable();
    registers.master.SR = ADC_SR_JEOC | ADC_SR_JSTRT;
    registers.master.JDR1 = 123;
    uint16_t out[2] = {77, 88};
    EXPECT_EQ(nx_stm32_adc_pair_read(&config, out), NX_ERROR_BUSY);
    EXPECT_EQ(out[0], 77U);
    EXPECT_EQ(out[1], 88U);
    EXPECT_EQ(registers.master.SR, ADC_SR_JEOC | ADC_SR_JSTRT);
}

TEST_F(AdcPair, CompletePairAcknowledgesBothAndPreservesRegularFlags) {
    enable();
    registers.master.SR = ADC_SR_JEOC | ADC_SR_JSTRT | ADC_SR_EOC;
    registers.slave.SR = ADC_SR_JEOC | ADC_SR_JSTRT;
    registers.master.JDR1 = 123;
    registers.slave.JDR1 = 456;
    uint16_t out[2] = {};
    ASSERT_EQ(nx_stm32_adc_pair_read(&config, out), NX_SUCCESS);
    EXPECT_EQ(out[0], 123U);
    EXPECT_EQ(out[1], 456U);
    EXPECT_EQ(registers.master.SR, ADC_SR_EOC);
    EXPECT_EQ(registers.slave.SR, 0U);
}

TEST_F(AdcPair, NewConversionDuringReadCannotPublishMixedSamples) {
    enable();
    registers.master.SR = registers.slave.SR = ADC_SR_JEOC | ADC_SR_JSTRT;
    registers.master.JDR1 = 123;
    registers.slave.JDR1 = 456;
    registers.inject_conversion = true;
    uint16_t out[2] = {77, 88};
    EXPECT_EQ(nx_stm32_adc_pair_read(&config, out), NX_ERROR_OVERFLOW);
    EXPECT_EQ(out[0], 77U);
    EXPECT_EQ(out[1], 88U);
}

TEST_F(AdcPair, OverrunAndIllegalCodeNeverModifyCallerOutput) {
    enable();
    registers.master.SR = ADC_SR_JEOC | ADC_SR_OVR;
    registers.slave.SR = ADC_SR_JEOC;
    uint16_t out[2] = {77, 88};
    EXPECT_EQ(nx_stm32_adc_pair_read(&config, out), NX_ERROR_OVERFLOW);
    registers.master.SR = ADC_SR_JEOC;
    registers.master.JDR1 = 4096;
    EXPECT_EQ(nx_stm32_adc_pair_read(&config, out), NX_ERROR_INVALID);
    EXPECT_EQ(out[0], 77U);
    EXPECT_EQ(out[1], 88U);
}

TEST_F(AdcPair, FailedReadbackRetainsCallerCleanupResponsibility) {
    registers.refused = &registers.slave.JSQR;
    EXPECT_EQ(nx_stm32_adc_pair_prepare(&config), NX_ERROR_IO);
    EXPECT_EQ(registers.master.JSQR, 5U << ADC_JSQR_JSQ4_Pos);
    EXPECT_EQ(registers.slave.JSQR, 0U);
}

TEST_F(AdcPair, WithdrawalAttemptsBothConvertersAfterOneWriteFails) {
    enable();
    registers.master.CR1 |= ADC_CR1_JEOCIE;
    registers.slave.CR1 |= ADC_CR1_JEOCIE;
    registers.refused = &registers.master.CR2;
    EXPECT_EQ(nx_stm32_adc_pair_disable(&config), NX_ERROR_IO);
    EXPECT_EQ(registers.slave.CR2 & ADC_CR2_ADON, 0U);
    EXPECT_EQ(registers.slave.CR1 & ADC_CR1_JEOCIE, 0U);
}

TEST_F(AdcPair, WithdrawalStillSuppressesAfterCommonModeBecomesInvalid) {
    enable();
    registers.common.CCR = 0U;
    ASSERT_EQ(nx_stm32_adc_pair_disable(&config), NX_SUCCESS);
    EXPECT_EQ((registers.master.CR2 | registers.slave.CR2) & ADC_CR2_ADON, 0U);
    EXPECT_EQ(registers.common.CCR, 0U);
}
} /* namespace */

extern "C" uint32_t nx_av_model_read(volatile uint32_t* reg) {
    const uint32_t value = *reg;
    if (reg == &model->master.JDR1 && model->inject_conversion) {
        model->master.SR |= ADC_SR_JSTRT;
        model->slave.SR |= ADC_SR_JSTRT;
        model->inject_conversion = false;
    }
    return value;
}

extern "C" void nx_av_model_write(volatile uint32_t* reg, uint32_t value) {
    ++model->writes;
    if (reg == model->refused)
        return;
    if (reg == &model->master.SR || reg == &model->slave.SR)
        *reg &= value;
    else
        *reg = value;
}
