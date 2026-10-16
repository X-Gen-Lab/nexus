/**
 * \file            g474_peripheral_factory_test.cpp
 * \brief           Actual static SPI1/SPI2 endpoints, cold IWDG and cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_peripheral_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32g474_provider.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
SPI_TypeDef g_g474_factory_spi[2];
IWDG_TypeDef g_g474_factory_watchdog;
DBGMCU_TypeDef g_g474_factory_debug;
}
class G474PeripheralFactory : public testing::Test {
  protected:
    void SetUp() override {
        nx_g474_model_reset();
        std::memset(g_g474_factory_spi, 0, sizeof(g_g474_factory_spi));
        std::memset(&g_g474_factory_watchdog, 0,
                    sizeof(g_g474_factory_watchdog));
        std::memset(&g_g474_factory_debug, 0, sizeof(g_g474_factory_debug));
        for (size_t index = 0; index < 2; ++index) {
            g_g474_model.spi[index] = &g_g474_factory_spi[index];
            g_g474_factory_spi[index].SR = SPI_SR_TXE;
        }
        g_g474_factory_watchdog.RLR = 4095;
        g_g474_factory_watchdog.WINR = 4095;
    }
    void TearDown() override {
        g_g474_model.ignore_write = nullptr;
        g_g474_model.hold_spi_cs_release = false;
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
};
TEST_F(G474PeripheralFactory, FactoryHasIndependentControllerAndEndpointFaces) {
    const auto* bus1 = nx_factory_spi(NX_SPI_ID_BUS1);
    const auto* bus2 = nx_factory_spi(NX_SPI_ID_BUS2);
    const auto* device1 = nx_factory_spi_device(NX_SPI_DEVICE_ID_DEVICE1);
    ASSERT_NE(nullptr, device1);
    EXPECT_NE(bus1, bus2);
    EXPECT_TRUE(nx_spi_endpoint_on_port(device1, bus1));
    EXPECT_FALSE(nx_spi_endpoint_on_port(device1, bus2));
    EXPECT_NE(nullptr, nx_factory_watchdog(NX_WATCHDOG_ID_WATCHDOG0));
    EXPECT_EQ(0U, g_g474_model.reads);
    EXPECT_EQ(0U, g_g474_model.writes);
}
TEST_F(G474PeripheralFactory, BothBusesTransferWithExactFifoAndSeparateCs) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    uint8_t tx = 0x12;
    uint8_t rx = 0;
    size_t transferred = 0;
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(
                              nx_factory_spi_device(NX_SPI_DEVICE_ID_DEVICE1),
                              &tx, &rx, 1, 1000, &transferred));
    EXPECT_EQ(0xedU, rx);
    EXPECT_EQ(1U, transferred);
    EXPECT_NE(0U, GPIOA->ODR & (1U << 4U));
    EXPECT_NE(0U, GPIOB->ODR & (1U << 12U));
    EXPECT_EQ(0U, g_g474_factory_watchdog.KR);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_SPI1EN);
    EXPECT_EQ(0U, RCC->APB1ENR1 & RCC_APB1ENR1_SPI2EN);
}
TEST_F(G474PeripheralFactory, LaterFifoSetupFailureRollsBackEarlierBusAndPins) {
    g_g474_model.ignore_write = &SPI2->CR2;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_SPI1EN);
    EXPECT_EQ(0U, RCC->APB1ENR1 & RCC_APB1ENR1_SPI2EN);
}
TEST_F(G474PeripheralFactory, ForeignSpiGateRejectsBeforeAnyGlobalClockWrite) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_SPI2EN;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_NE(0U, RCC->APB1ENR1 & RCC_APB1ENR1_SPI2EN);
}
TEST_F(G474PeripheralFactory, FailedCsStopRetainsPlatformUntilRealRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    /* Inject a latched idle-level fault before cold stop. Polling withdrawal
     * failure itself is terminal and is tested in its own isolated process. */
    GPIOA->ODR &= ~(1U << 4U);
    g_g474_model.spi_transmitted = 1;
    g_g474_model.hold_spi_cs_release = true;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_SPI1EN);
    EXPECT_NE(0U, RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN);
    g_g474_model.hold_spi_cs_release = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_SPI1EN);
}
