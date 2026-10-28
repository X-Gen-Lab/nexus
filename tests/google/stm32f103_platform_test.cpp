/**
 * \file            stm32f103_platform_test.cpp
 * \brief           Real F103 complete cold resource admission and retry
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "stm32f103_provider.h"
#include <gtest/gtest.h>
class F103Platform : public testing::Test {
  protected:
    nx_stm32f103_platform_config_t config{32U, 0U, 0U};
    nx_stm32f103_platform_resources_t resources{};
    void SetUp() override {
        nx_f103_model_reset();
        g_nx_stm32f103_system.started = true;
        g_nx_stm32f103_system.core_hz = 8000000U;
    }
    void TearDown() override {
        g_f103_model.ignore_write = nullptr;
        EXPECT_EQ(NX_SUCCESS,
                  nx_stm32f103_platform_resources_release(&resources));
        if (nx_arch_irq_is_masked())
            nx_arch_irq_restore(nx_arch_irq_state_t{0U});
    }
};
TEST_F(F103Platform, ForeignPoweredPinAndLockedPinRejectBeforeWrites) {
    g_nx_stm32f103_system.started = false;
    g_f103_model.rcc.APB2ENR = RCC_APB2ENR_IOPAEN;
    g_f103_model.gpio.CRL =
        (g_f103_model.gpio.CRL & ~(15U << 20U)) | (3U << 20U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_platform_preflight(&config));
    EXPECT_EQ(0U, g_f103_model.writes);
    g_f103_model.gpio.CRL = 0x44444444U;
    g_f103_model.gpio.LCKR = 32U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_platform_preflight(&config));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Platform, SelectedVectorPendingAndActiveRejectWithoutAdoption) {
    g_nx_stm32f103_system.started = false;
    config.pins = 12U;
    config.peripherals =
        NX_STM32F103_PLATFORM_UART | NX_STM32F103_PLATFORM_DMA_TX;
    g_f103_model.irq_pending[DMA1_Channel7_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_platform_preflight(&config));
    g_f103_model.irq_pending[DMA1_Channel7_IRQn] = false;
    g_f103_model.irq_active[USART2_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_platform_preflight(&config));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Platform, ExtiAndAdcDividerAdmissionRemainExact) {
    g_nx_stm32f103_system.started = false;
    config.pins = 3U;
    config.exti_lines = 2U;
    g_f103_model.exti.PR = 2U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_platform_preflight(&config));
    g_f103_model.exti.PR = 0U;
    config.peripherals = NX_STM32F103_PLATFORM_ADC;
    g_f103_model.rcc.CFGR = RCC_CFGR_ADCPRE_0;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_platform_preflight(&config));
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Platform, ReleaseRestoresOnlySelectedModesLatchAndAddedGates) {
    g_f103_model.rcc.APB2ENR = RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
    g_f103_model.gpio.ODR = 33U;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32f103_platform_resources_acquire(&resources, &config));
    g_f103_model.gpio.CRL =
        (g_f103_model.gpio.CRL & ~((15U << 20U) | (15U << 16U))) | (2U << 20U) |
        (3U << 16U);
    g_f103_model.gpio.ODR &= ~32U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_platform_resources_release(&resources));
    EXPECT_EQ(4U, (g_f103_model.gpio.CRL >> 20U) & 15U);
    EXPECT_EQ(3U, (g_f103_model.gpio.CRL >> 16U) & 15U);
    EXPECT_EQ(33U, g_f103_model.gpio.ODR);
    EXPECT_EQ(RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN,
              g_f103_model.rcc.APB2ENR);
}
TEST_F(F103Platform, GateRefusalRetainsOnlyCleanupReceiptWithoutPinCapture) {
    g_f103_model.ignore_write = &g_f103_model.rcc.APB2ENR;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32f103_platform_resources_acquire(&resources, &config));
    EXPECT_FALSE((resources.owned & NX_STM32F103_OWN_PINS) != 0U);
    EXPECT_NE(0U, resources.owned &
                      (NX_STM32F103_OWN_GPIOA | NX_STM32F103_OWN_AFIO |
                       NX_STM32F103_OWN_SPI1 | NX_STM32F103_OWN_ADC1));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_platform_resources_release(&resources));
    EXPECT_EQ(0U, resources.owned &
                      (NX_STM32F103_OWN_GPIOA | NX_STM32F103_OWN_AFIO |
                       NX_STM32F103_OWN_SPI1 | NX_STM32F103_OWN_ADC1));
}
TEST_F(F103Platform,
       HiddenForeignPinRejectsAfterGateAndIsNeverCapturedOrReset) {
    g_f103_model.gpio.CRL =
        (g_f103_model.gpio.CRL & ~(15U << 20U)) | (3U << 20U);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f103_platform_resources_acquire(&resources, &config));
    EXPECT_FALSE((resources.owned & NX_STM32F103_OWN_PINS) != 0U);
    EXPECT_NE(0U, resources.owned &
                      (NX_STM32F103_OWN_GPIOA | NX_STM32F103_OWN_AFIO |
                       NX_STM32F103_OWN_SPI1 | NX_STM32F103_OWN_ADC1));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_platform_resources_release(&resources));
    EXPECT_EQ(3U, (g_f103_model.gpio.CRL >> 20U) & 15U);
    EXPECT_EQ(0U, g_f103_model.rcc.APB2ENR);
}
TEST_F(F103Platform, FailedRestoreRetainsReceiptAndRejectsRestartUntilRetry) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32f103_platform_resources_acquire(&resources, &config));
    g_f103_model.gpio.CRL =
        (g_f103_model.gpio.CRL & ~(15U << 20U)) | (2U << 20U);
    g_f103_model.ignore_write = &g_f103_model.gpio.CRL;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f103_platform_resources_release(&resources));
    EXPECT_TRUE((resources.owned & NX_STM32F103_OWN_PINS) != 0U);
    EXPECT_NE(0U, g_f103_model.rcc.APB2ENR & RCC_APB2ENR_IOPAEN);
    const auto writes = g_f103_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32f103_platform_resources_acquire(&resources, &config));
    EXPECT_EQ(writes, g_f103_model.writes);
    g_f103_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_platform_resources_release(&resources));
    EXPECT_FALSE((resources.owned & NX_STM32F103_OWN_PINS) != 0U);
    EXPECT_EQ(0U, g_f103_model.rcc.APB2ENR);
}
TEST_F(F103Platform, FailedPeripheralGateWithdrawalCanBeRetried) {
    config.peripherals = NX_STM32F103_PLATFORM_UART;
    config.pins = 12U;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32f103_platform_resources_acquire(&resources, &config));
    g_f103_model.ignore_write = &g_f103_model.rcc.APB1ENR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f103_platform_resources_release(&resources));
    EXPECT_EQ(NX_STM32F103_OWN_USART2,
              resources.owned & NX_STM32F103_OWN_USART2);
    EXPECT_NE(0U, g_f103_model.rcc.APB2ENR & RCC_APB2ENR_IOPAEN);
    g_f103_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_platform_resources_release(&resources));
    EXPECT_EQ(0U, resources.owned & NX_STM32F103_OWN_USART2);
}
TEST_F(F103Platform, InvalidSelectionAndWrongContextHaveNoHardwareEffects) {
    config.peripherals = NX_STM32F103_PLATFORM_DMA_RX;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f103_platform_resources_acquire(&resources, &config));
    config.peripherals = 0U;
    auto saved = nx_arch_irq_save();
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_stm32f103_platform_resources_acquire(&resources, &config));
    nx_arch_irq_restore(saved);
    EXPECT_EQ(0U, g_f103_model.writes);
}

TEST_F(F103Platform, ControllerSelectionCannotOmitItsPhysicalPins) {
    config.pins = 0U;
    config.peripherals = NX_STM32F103_PLATFORM_UART;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f103_platform_resources_acquire(&resources, &config));
    EXPECT_EQ(0U, g_f103_model.writes);
    g_nx_stm32f103_system.started = false;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f103_platform_preflight(&config));
}
