/**
 * \file            stm32_adc_cold_factory_test.cpp
 * \brief           Actual multi-bank ADC factory and retained cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32_adc_cold_fixture.h"

using AdcColdFactory = AdcColdModel;

TEST_F(AdcColdFactory, LookupIsPureAndStopRestoresAllThreeBanks) {
    const auto* port = nx_factory_adc(static_cast<nx_adc_id_t>(0));
    ASSERT_NE(nullptr, port);
    EXPECT_EQ(0U, starts);
    Restored();
    Start();
    nx_adc_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_info(port, &info));
    EXPECT_EQ(16U, info.channel_count);
    EXPECT_EQ(NX_TEST_ADC_BITS, info.resolution_bits);
    EXPECT_EQ((12U - NX_TEST_ADC_BITS) / 2U,
              (ADC1->CR1 & ADC_CR1_RES) >> ADC_CR1_RES_Pos);
    EXPECT_EQ(0xffffU, GPIOA->MODER);
    EXPECT_EQ(0xfU, GPIOB->MODER);
    EXPECT_EQ(0xfffU, GPIOC->MODER);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, starts);
    EXPECT_EQ(1U, stops);
    Restored();
}

TEST_F(AdcColdFactory, RefusedAdcShutdownKeepsEveryPadAndTheSystemAlive) {
    Start();
    ADC1->CR2 = ADC_CR2_ADON;
    refused = &ADC1->CR2;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    EXPECT_EQ(0xffffU, GPIOA->MODER);
    EXPECT_EQ(0xfU, GPIOB->MODER);
    EXPECT_EQ(0xfffU, GPIOC->MODER);
    EXPECT_EQ(0U, stops);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, stops);
    Restored();
}

TEST_F(AdcColdFactory, RefusedBankRestoreRetainsClocksAndRetriesRemainingPads) {
    Start();
    refuse_restore = true;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    EXPECT_EQ(0U, stops);
    EXPECT_EQ(3U, GPIOB->MODER & 3U);
    EXPECT_EQ(0xffffU, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOC->MODER);
    EXPECT_EQ(7U, RCC->AHB1ENR & 7U);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    refuse_restore = false;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, stops);
    Restored();
}

TEST_F(AdcColdFactory, RefusedShotPowerdownRetainsOnlyDeviceCleanup) {
    Start();
    const auto* port = nx_factory_adc(static_cast<nx_adc_id_t>(0));
    ASSERT_NE(nullptr, port);
    refused = &ADC1->CR2;
    refuse_powerdown = true;
    uint16_t samples[16]{};
    size_t count{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(port, samples, 16U, now + 10000U, &count));
    EXPECT_EQ(16U, count);
    EXPECT_NE(0U, ADC1->CR2 & ADC_CR2_ADON);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_adc_port_sample(port, samples, 16U, now + 10000U, &count));
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    refused = nullptr;
    refuse_powerdown = false;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    Restored();
}

TEST_F(AdcColdFactory, UnprivilegedSamplingDoesNotAccessTheConverter) {
    Start();
    const auto* port = nx_factory_adc(static_cast<nx_adc_id_t>(0));
    ASSERT_NE(nullptr, port);
    const uint32_t control1 = ADC1->CR1;
    const uint32_t control2 = ADC1->CR2;
    uint16_t samples[16]{0xBEEFU};
    size_t count = 99U;
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_adc_port_sample(port, samples, 16U, now + 10000U, &count));
    privileged = true;
    EXPECT_EQ(0xBEEFU, samples[0]);
    EXPECT_EQ(0U, count);
    EXPECT_EQ(control1, ADC1->CR1);
    EXPECT_EQ(control2, ADC1->CR2);
}

TEST_F(AdcColdFactory, UnselectedConvertersCannotAcquirePadsOrClocks) {
    const uint8_t channel[]{0U};
    const uint8_t samples[]{7U};
    for (auto* converter : {ADC2, ADC3}) {
        const nx_stm32_adc_config_t config{channel, samples, 1U,
                                           3300U,   0U,      converter};
        nx_stm32_adc_state_t state{};
        EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_adc_prepare(&state, &config));
        EXPECT_EQ(0U, state.cold_effects);
        EXPECT_EQ(NX_SUCCESS, nx_stm32_adc_close(&state));
        EXPECT_EQ(0U, starts);
        Restored();
    }
}
