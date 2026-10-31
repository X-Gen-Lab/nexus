/**
 * \file            gd32f303_factory_test.cpp
 * \brief           Actual generated GD303 factory, assembly and cleanup tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
#include "model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
extern "C" void USART0_IRQHandler(void);
class GD303Factory : public testing::Test {
  protected:
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
        g_gd303_writes = 0;
        g_gd303_density = (48U << 16U) | 256U;
        g_gd303_hold_update = g_gd303_hold_irq_off = false;
        GPIO_CTL0(GPIOA) = GPIO_CTL1(GPIOA) = 0x44444444U;
        RCU_CTL = RCU_CTL_IRC8MEN | RCU_CTL_IRC8MSTB | (16U << 3U);
        USART_STAT0(USART0) = USART_STAT0_TC | USART_STAT0_TBE;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            g_gd303_ignore = nullptr;
            g_gd303_hold_irq_off = false;
            USART_STAT0(USART0) |= USART_STAT0_TC;
            nx_uart_port_service(nx_factory_uart(NX_UART_ID_SERIAL));
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
            munmap(mapping, 0x30000U);
        }
    }
};
TEST_F(GD303Factory, LookupReturnsStaticIdentityBeforeAnyHardwareAccess) {
    const auto* gpio = nx_factory_gpio(NX_GPIO_ID_OUTPUT);
    const auto* uart = nx_factory_uart(NX_UART_ID_SERIAL);
    ASSERT_NE(nullptr, gpio);
    ASSERT_NE(nullptr, uart);
    EXPECT_EQ(gpio, nx_factory_gpio(NX_GPIO_ID_OUTPUT));
    EXPECT_EQ(uart, nx_factory_uart(NX_UART_ID_SERIAL));
    EXPECT_EQ(nullptr, nx_factory_gpio(NX_GPIO_ID_COUNT));
    EXPECT_EQ(nullptr, nx_factory_uart(NX_UART_ID_COUNT));
    EXPECT_EQ(0U, g_gd303_writes);
    EXPECT_EQ(NX_ERROR_INVALID, nx_gpio_port_write(gpio, 32, 0));
}
TEST_F(GD303Factory, FullStartStopRestoresPinsAndOwnedClocks) {
    GPIO_CTL0(GPIOA) = 0x44444443U;
    auto result = nx_platform_start();
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_TRUE(g_nx_gd32f303_system.started);
    EXPECT_EQ(2U, (GPIO_CTL0(GPIOA) >> 20U) & 15U);
    EXPECT_EQ(10U, (GPIO_CTL1(GPIOA) >> 4U) & 15U);
    EXPECT_EQ(69U, USART_BAUD(USART0));
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
    EXPECT_EQ(0U, RCU_APB2EN);
    EXPECT_EQ(0x44444443U, GPIO_CTL0(GPIOA));
    EXPECT_EQ(0x44444444U, GPIO_CTL1(GPIOA));
    EXPECT_FALSE(g_gd303_irq[USART0_IRQn]);
    EXPECT_FALSE(g_nx_gd32f303_system.started);
}
TEST_F(GD303Factory, PoweredForeignPinRejectsBeforeAnyGlobalClockEffect) {
    RCU_APB2EN = RCU_APB2EN_PAEN;
    GPIO_CTL1(GPIOA) = (GPIO_CTL1(GPIOA) & ~(15U << 8U)) | (3U << 8U);
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, g_gd303_writes);
    EXPECT_EQ(RCU_APB2EN_PAEN, RCU_APB2EN);
    EXPECT_FALSE(g_nx_gd32f303_system.started);
}
TEST_F(GD303Factory, PendingUsartRejectsWithoutGlobalWrites) {
    g_gd303_pending[USART0_IRQn] = true;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(0U, g_gd303_writes);
    EXPECT_TRUE(g_gd303_pending[USART0_IRQn]);
}
TEST_F(GD303Factory, LateWireCompletionDrainsBeforePlatformReclaimsPins) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    uint8_t payload[1]{0x21};
    nx_uart_tx_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS, nx_uart_tx_prepare(&request, payload, 1, 10000));
    const auto* uart = nx_factory_uart(NX_UART_ID_SERIAL);
    ASSERT_EQ(NX_SUCCESS, nx_uart_port_submit(uart, &request));
    USART0_IRQHandler();
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_stop());
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    EXPECT_NE(0U, RCU_APB2EN & RCU_APB2EN_PAEN);
    USART_STAT0(USART0) |= USART_STAT0_TC;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, RCU_APB2EN & RCU_APB2EN_PAEN);
}
TEST_F(GD303Factory, FailedPinRestoreRetainsPlatformAndAllowsCleanupRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    g_gd303_ignore = &GPIO_CTL1(GPIOA);
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_NE(0U, RCU_APB2EN & RCU_APB2EN_PAEN);
    EXPECT_TRUE(g_nx_gd32f303_system.started);
    g_gd303_ignore = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCU_APB2EN & RCU_APB2EN_PAEN);
}
