/**
 * \file            stm32f103_factory_test.cpp
 * \brief           Actual generated F103 Platform, pure factory and recovery
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32f103_provider.h"
#include <gtest/gtest.h>
#include <sys/mman.h>
class F103Factory : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        nx_f103_model_reset();
        g_f103_model.uart = USART2;
        USART2->SR = USART_SR_TXE | USART_SR_TC;
        g_f103_model.spi.SR = SPI_SR_TXE;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            g_f103_model.ignore_write = nullptr;
            g_f103_model.hold_irq_off = false;
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
            munmap(mapping, 0x30000U);
        }
    }
};
TEST_F(F103Factory, LookupHasStableIdentityWithoutHardwareAccess) {
#ifdef NX_F103_FACTORY_SPI
    const auto* face = nx_factory_spi(NX_SPI_ID_BUS);
    ASSERT_NE(nullptr, face);
    EXPECT_EQ(face, nx_factory_spi(NX_SPI_ID_BUS));
    EXPECT_EQ(&nx_stm32f103_spi_ops, face->ops);
    EXPECT_NE(nullptr, nx_device_peripheral);
    EXPECT_EQ(nullptr, nx_factory_spi(NX_SPI_ID_COUNT));
#else
    const auto* face = nx_factory_uart(NX_UART_ID_UART1);
    ASSERT_NE(nullptr, face);
    EXPECT_EQ(face, nx_factory_uart(NX_UART_ID_UART1));
    EXPECT_EQ(&nx_stm32f103_uart_ops, face->ops);
    EXPECT_NE(nullptr, nx_factory_gpio(NX_GPIO_ID_LED0));
    EXPECT_EQ(nullptr, nx_factory_uart(NX_UART_ID_COUNT));
#endif
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Factory, CompletePublicStartStopRestoresOneResourceLease) {
    g_f103_model.gpio.CRL = 0x44444443U;
    auto start = nx_platform_start();
    ASSERT_EQ(NX_SUCCESS, start.primary);
    EXPECT_EQ(NX_SUCCESS, start.cleanup);
    EXPECT_EQ(0U, start.remaining_effects);
    EXPECT_TRUE(g_nx_stm32f103_system.started);
#ifdef NX_F103_FACTORY_SPI
    EXPECT_EQ(16U, g_f103_model.gpio.ODR & 16U);
    uint8_t byte = 0x53U;
    uint8_t received = 0U;
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS,
              nx_spi_endpoint_transfer(nx_device_peripheral, &byte, &received,
                                       1U, 1000U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0xacU, received);
#else
    EXPECT_EQ(69U, USART2->BRR);
    EXPECT_TRUE(g_f103_model.irq_enabled[USART2_IRQn]);
    EXPECT_EQ(2U, (g_f103_model.gpio.CRL >> 20U) & 15U);
#endif
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0x44444443U, g_f103_model.gpio.CRL);
    EXPECT_EQ(0U, g_f103_model.rcc.APB2ENR);
    EXPECT_EQ(0U, g_f103_model.rcc.APB1ENR);
    EXPECT_FALSE(g_nx_stm32f103_system.started);
}
TEST_F(F103Factory, ForeignPoweredPadRejectsBeforeSystemClockEffects) {
    g_f103_model.rcc.APB2ENR = RCC_APB2ENR_IOPAEN;
    g_f103_model.gpio.CRL =
        (g_f103_model.gpio.CRL & ~(15U << 20U)) | (3U << 20U);
    auto start = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, start.primary);
    EXPECT_EQ(0U, start.remaining_effects);
    EXPECT_EQ(0U, g_f103_model.writes);
    EXPECT_EQ(RCC_APB2ENR_IOPAEN, g_f103_model.rcc.APB2ENR);
}
TEST_F(F103Factory, PendingSelectedIrqRejectsBeforeAnyWrites) {
#ifdef NX_F103_FACTORY_SPI
    g_f103_model.irq_pending[SPI1_IRQn] = true;
#else
    g_f103_model.irq_pending[USART2_IRQn] = true;
#endif
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(0U, g_f103_model.writes);
}
TEST_F(F103Factory, RefusedGateAcquisitionRollsBackActualSystemAndReceipt) {
    g_f103_model.ignore_write = &g_f103_model.rcc.APB2ENR;
    auto start = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, start.primary);
    EXPECT_EQ(NX_SUCCESS, start.cleanup);
    EXPECT_EQ(0U, start.remaining_effects);
    EXPECT_EQ(0U, g_f103_model.rcc.APB2ENR);
    EXPECT_EQ(0U, g_f103_model.rcc.APB1ENR);
    EXPECT_FALSE(g_nx_stm32f103_system.started);
}
TEST_F(F103Factory, RefusedPinRestoreRetainsPlatformUntilCleanupRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    g_f103_model.ignore_write = &g_f103_model.gpio.CRL;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32f103_system.started);
    EXPECT_NE(0U, g_f103_model.rcc.APB2ENR & RCC_APB2ENR_IOPAEN);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    g_f103_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_FALSE(g_nx_stm32f103_system.started);
    EXPECT_EQ(0U, g_f103_model.rcc.APB2ENR);
}
