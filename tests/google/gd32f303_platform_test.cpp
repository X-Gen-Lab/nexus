/**
 * \file            gd32f303_platform_test.cpp
 * \brief           Real GD303 cold admission, resource ownership and retry
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
#include "model.h"
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
class GD303Platform : public testing::Test {
  protected:
    nx_gd32f303_platform_config_t config{32U, 0U, 0U};
    nx_gd32f303_platform_resources_t resources{};
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        std::memset(g_gd303_irq, 0, sizeof(g_gd303_irq));
        std::memset(g_gd303_pending, 0, sizeof(g_gd303_pending));
        g_gd303_ignore = nullptr;
        g_gd303_writes = 0U;
        g_gd303_density = (48U << 16U) | 256U;
        g_nx_gd32f303_system = {};
        g_nx_gd32f303_system.started = true;
        g_nx_gd32f303_system.core_hz = 8000000U;
        RCU_CTL = RCU_CTL_IRC8MEN | RCU_CTL_IRC8MSTB;
        GPIO_CTL0(GPIOA) = GPIO_CTL1(GPIOA) = 0x44444444U;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            g_gd303_ignore = nullptr;
            EXPECT_EQ(NX_SUCCESS,
                      nx_gd32f303_platform_resources_release(&resources));
            munmap(mapping, 0x30000U);
        }
        if (nx_arch_irq_is_masked())
            nx_arch_irq_restore(nx_arch_irq_state_t{0U});
    }
};
TEST_F(GD303Platform, ForeignPoweredPinRejectsWithoutClockEffects) {
    g_nx_gd32f303_system.started = false;
    RCU_APB2EN = RCU_APB2EN_PAEN;
    GPIO_CTL0(GPIOA) = (GPIO_CTL0(GPIOA) & ~(15U << 20U)) | (3U << 20U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_platform_preflight(&config));
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303Platform, SelectedDmaPendingAndUsartVectorRejectBeforeWrites) {
    g_nx_gd32f303_system.started = false;
    config.pins = 1536U;
    config.peripherals =
        NX_GD32F303_PLATFORM_UART | NX_GD32F303_PLATFORM_DMA_TX;
    g_gd303_pending[DMA0_Channel3_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_platform_preflight(&config));
    g_gd303_pending[DMA0_Channel3_IRQn] = false;
    g_gd303_irq[USART0_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_platform_preflight(&config));
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303Platform, ExtiAndBothAdcDividerFieldsRemainExact) {
    g_nx_gd32f303_system.started = false;
    config.pins = 3U;
    config.exti_lines = 2U;
    EXTI_PD = 2U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_platform_preflight(&config));
    EXTI_PD = 0U;
    config.peripherals = NX_GD32F303_PLATFORM_ADC;
    RCU_CFG1 = RCU_CFG1_ADCPSC_3;
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_platform_preflight(&config));
    RCU_CFG1 = 0U;
    RCU_CFG0 = RCU_CFG0_ADCPSC_2;
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_platform_preflight(&config));
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303Platform, OneReceiptRestoresOnlySelectedPinAndKeepsForeignClock) {
    RCU_APB2EN = RCU_APB2EN_PAEN | RCU_APB2EN_PBEN;
    GPIO_OCTL(GPIOA) = 33U;
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32f303_platform_resources_acquire(&resources, &config));
    GPIO_CTL0(GPIOA) = (GPIO_CTL0(GPIOA) & ~((15U << 20U) | (15U << 16U))) |
                       (2U << 20U) | (3U << 16U);
    GPIO_OCTL(GPIOA) &= ~32U;
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_platform_resources_release(&resources));
    EXPECT_EQ(4U, (GPIO_CTL0(GPIOA) >> 20U) & 15U);
    EXPECT_EQ(3U, (GPIO_CTL0(GPIOA) >> 16U) & 15U);
    EXPECT_EQ(33U, GPIO_OCTL(GPIOA));
    EXPECT_EQ(RCU_APB2EN_PAEN | RCU_APB2EN_PBEN, RCU_APB2EN);
}
TEST_F(GD303Platform, RefusedGateDoesNotCapturePinsAndCleanupCanComplete) {
    g_gd303_ignore = &RCU_APB2EN;
    EXPECT_EQ(NX_ERROR_IO,
              nx_gd32f303_platform_resources_acquire(&resources, &config));
    EXPECT_FALSE(resources.pins.captured);
    EXPECT_TRUE(resources.pins.retained);
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_platform_resources_release(&resources));
    EXPECT_FALSE(resources.pins.retained);
}
TEST_F(GD303Platform, FailedRestoreRetainsOneOwnerUntilExplicitRetry) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32f303_platform_resources_acquire(&resources, &config));
    GPIO_CTL0(GPIOA) = (GPIO_CTL0(GPIOA) & ~(15U << 20U)) | (2U << 20U);
    g_gd303_ignore = &GPIO_CTL0(GPIOA);
    EXPECT_EQ(NX_ERROR_IO, nx_gd32f303_platform_resources_release(&resources));
    EXPECT_TRUE(resources.pins.retained);
    const auto writes = g_gd303_writes;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32f303_platform_resources_acquire(&resources, &config));
    EXPECT_EQ(writes, g_gd303_writes);
    EXPECT_NE(0U, RCU_APB2EN & RCU_APB2EN_PAEN);
    g_gd303_ignore = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_platform_resources_release(&resources));
    EXPECT_FALSE(resources.pins.retained);
    EXPECT_EQ(0U, RCU_APB2EN);
}
TEST_F(GD303Platform, PwmGateRefusalRetainsReceiptAndRestoresPinsBeforeRetry) {
    config.peripherals = NX_GD32F303_PLATFORM_PWM;
    config.pins = 64U;
    g_gd303_ignore = &RCU_APB1EN;
    EXPECT_EQ(NX_ERROR_IO,
              nx_gd32f303_platform_resources_acquire(&resources, &config));
    EXPECT_EQ(NX_GD32F303_OWN_TIMER2, resources.owned & NX_GD32F303_OWN_TIMER2);
    EXPECT_TRUE(resources.pins.retained);
    g_gd303_ignore = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_platform_resources_release(&resources));
    EXPECT_EQ(0U, resources.owned & NX_GD32F303_OWN_TIMER2);
    EXPECT_EQ(0U, RCU_APB2EN);
}
TEST_F(GD303Platform, InvalidSelectionAndMaskedContextNeverWriteHardware) {
    config.peripherals = NX_GD32F303_PLATFORM_DMA_RX;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_gd32f303_platform_resources_acquire(&resources, &config));
    config.peripherals = 0U;
    auto saved = nx_arch_irq_save();
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_gd32f303_platform_resources_acquire(&resources, &config));
    nx_arch_irq_restore(saved);
    EXPECT_EQ(0U, g_gd303_writes);
}

TEST_F(GD303Platform, ControllerSelectionCannotOmitItsPhysicalPins) {
    config.pins = 0U;
    config.peripherals = NX_GD32F303_PLATFORM_UART;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_gd32f303_platform_resources_acquire(&resources, &config));
    EXPECT_EQ(0U, g_gd303_writes);
    g_nx_gd32f303_system.started = false;
    EXPECT_EQ(NX_ERROR_INVALID, nx_gd32f303_platform_preflight(&config));
}
