/**
 * \file            h743_peripheral_factory_test.cpp
 * \brief           Actual seven-class H743 static factory and pin cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_peripheral_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32h743_provider.h"
#include <gtest/gtest.h>
class H743PeripheralFactory : public testing::Test {
  protected:
    void SetUp() override {
        nx_h743_model_reset();
        g_h743_model.pwm = TIM3;
        g_h743_model.spi = SPI4;
        g_h743_model.adc = ADC1;
        ADC1->CR = ADC2->CR = ADC_CR_DEEPPWD;
        ADC1->CFGR = ADC_CFGR_JQDIS;
        FLASH->OPTSR_CUR = FLASH_OPTSR_IWDG1_SW;
        IWDG1->WINR = IWDG1->RLR = 4095U;
    }
    void TearDown() override {
        g_h743_model.ignore_write = nullptr;
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
};
TEST_F(H743PeripheralFactory, ColdTypedLookupHasNoHardwareEffects) {
    EXPECT_NE(nullptr, nx_factory_spi(NX_SPI_ID_BUS4));
    EXPECT_NE(nullptr, nx_factory_i2c(NX_I2C_ID_BUS1));
    EXPECT_NE(nullptr, nx_factory_adc(NX_ADC_ID_SENSOR));
    EXPECT_NE(nullptr, nx_factory_pwm(NX_PWM_ID_WAVE3));
    EXPECT_NE(nullptr, nx_factory_flash(NX_FLASH_ID_STORAGE));
    EXPECT_NE(nullptr, nx_factory_exti(NX_EXTI_ID_EDGE0));
    EXPECT_NE(nullptr, nx_factory_watchdog(NX_WATCHDOG_ID_WATCHDOG0));
    EXPECT_EQ(0U, g_h743_model.reads);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743PeripheralFactory, AllConcreteProvidersAndEndpointPinsAreAssembled) {
    auto result = nx_platform_start();
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_TRUE(g_h743_model.adc_calibrated);
    EXPECT_EQ(0x70f22727U, I2C1->TIMINGR);
    const auto* i2c = static_cast<const nx_i2c_v2_state_t*>(
        nx_factory_i2c(NX_I2C_ID_BUS1)->context);
    EXPECT_TRUE(i2c->ignore_master_berr);
    EXPECT_TRUE(i2c->receive_reload_one);
    EXPECT_EQ(3U, (GPIOB->OTYPER >> 8U) & 3U);
    EXPECT_EQ(0x44U, GPIOB->AFR[1] & 255U);
    EXPECT_EQ(1U, (GPIOE->MODER >> 22U) & 3U);
    EXPECT_NE(0U, GPIOE->ODR & (1U << 11U));
    EXPECT_TRUE(nx_spi_endpoint_on_port(nx_device_device4,
                                        nx_factory_spi(NX_SPI_ID_BUS4)));
    nx_adc_info_t info{};
    EXPECT_EQ(NX_SUCCESS,
              nx_adc_port_info(nx_factory_adc(NX_ADC_ID_SENSOR), &info));
    EXPECT_EQ(16U, info.resolution_bits);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(0U, GPIOE->MODER);
    EXPECT_EQ(0U, GPIOE->ODR);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_SPI4EN);
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_ADC12EN);
}
TEST_F(H743PeripheralFactory,
       FlashCapabilityMatchesActualGeometryAndInputCopy) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    const auto* geometry =
        nx_flash_port_geometry(nx_factory_flash(NX_FLASH_ID_STORAGE));
    const auto* capability = nx_factory_flash_capability(NX_FLASH_ID_STORAGE);
    ASSERT_NE(nullptr, geometry);
    ASSERT_NE(nullptr, capability);
    EXPECT_EQ(geometry->program_unit, capability->unit_bytes);
    EXPECT_EQ(1U, capability->buffer_alignment);
}
TEST_F(H743PeripheralFactory, EndpointCsBehindDisabledClockIsNotAdopted) {
    GPIOE->MODER = 2U << 22U;
    auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, g_h743_model.gpio_writes);
    EXPECT_EQ(2U, (GPIOE->MODER >> 22U) & 3U);
}
TEST_F(H743PeripheralFactory,
       FailedFinalPinRestorationRetainsItsClockAndRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    g_h743_model.ignore_write = &GPIOE->MODER;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_NE(0U, RCC->AHB4ENR & RCC_AHB4ENR_GPIOEEN);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCC->AHB4ENR & RCC_AHB4ENR_GPIOEEN);
}
TEST_F(H743PeripheralFactory, I2cForeignKernelRejectsBeforePrivatePinEffects) {
    g_nx_stm32h743_system.started = true;
    g_nx_stm32h743_system.core_hz = 64000000U;
    RCC->APB1LENR = RCC_APB1LENR_I2C1EN;
    for (uint32_t choice = 1U; choice < 4U; ++choice) {
        RCC->D2CCIP2R = choice << RCC_D2CCIP2R_I2C123SEL_Pos;
        uint32_t writes = g_h743_model.writes;
        EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_i2c_pins_prepare());
        EXPECT_EQ(writes, g_h743_model.writes);
    }
    g_nx_stm32h743_system.started = false;
}
