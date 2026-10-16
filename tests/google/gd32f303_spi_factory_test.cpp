/**
 * \file            gd32f303_spi_factory_test.cpp
 * \brief           Real static SPI endpoint and inactive FWDGT assembly tests
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
class GD303SPIFactory : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(0x40000000U));
        g_gd303_debug_ctl0 = 0U;
        std::memset(g_gd303_irq, 0, sizeof(g_gd303_irq));
        std::memset(g_gd303_pending, 0, sizeof(g_gd303_pending));
        g_gd303_ignore = nullptr;
        g_gd303_writes = g_gd303_spi_bytes = 0;
        g_gd303_density = (48U << 16U) | 256U;
        g_gd303_hold_update = g_gd303_hold_irq_off = false;
        g_gd303_spi_bad_cs = g_gd303_spi_hold_rx = false;
        GPIO_CTL0(GPIOA) = GPIO_CTL1(GPIOA) = 0x44444444U;
        RCU_CTL = RCU_CTL_IRC8MEN | RCU_CTL_IRC8MSTB;
        SPI_STAT(SPI0) = SPI_STAT_TBE;
        FMC_OBSTAT = 4U;
    }
    void TearDown() override {
        if (mmio != MAP_FAILED) {
            g_gd303_ignore = nullptr;
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        }
        if (mmio != MAP_FAILED)
            munmap(mmio, 0x30000U);
    }
};
TEST_F(GD303SPIFactory, EndpointAndControllerHaveDifferentStaticIdentity) {
    const auto* port = nx_factory_spi(NX_SPI_ID_BUS);
    ASSERT_NE(nullptr, port);
    ASSERT_NE(nullptr, nx_device_peripheral);
    EXPECT_EQ(&nx_gd32f303_spi_ops, port->ops);
    EXPECT_EQ(&nx_gd32f303_spi_endpoint_ops, nx_device_peripheral->ops);
    EXPECT_EQ(nullptr, nx_factory_spi(NX_SPI_ID_COUNT));
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303SPIFactory,
       CompleteGeneratedEndpointTransfersThenRestoresItsLease) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    EXPECT_EQ(16U, GPIO_OCTL(GPIOA) & 16U);
    EXPECT_EQ(11U, (GPIO_CTL0(GPIOA) >> 20U) & 15U);
    uint8_t tx = 0x35;
    uint8_t rx = 0;
    size_t count = 0;
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(nx_device_peripheral, &tx,
                                                   &rx, 1, 1000, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0xcaU, rx);
    nx_watchdog_state_t observed{};
    EXPECT_EQ(NX_SUCCESS,
              nx_watchdog_port_state(
                  nx_factory_watchdog(NX_WATCHDOG_ID_INDEPENDENT), &observed));
    EXPECT_FALSE(observed.enabled);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0x44444444U, GPIO_CTL0(GPIOA));
    EXPECT_EQ(0U, RCU_APB2EN);
}
TEST_F(GD303SPIFactory, FailedPartialCsSetupUnwindsWithoutInventingAnOwner) {
    g_gd303_ignore = &GPIO_BOP(GPIOA);
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0x44444444U, GPIO_CTL0(GPIOA));
    EXPECT_EQ(0U, RCU_APB2EN);
    EXPECT_FALSE(g_nx_gd32f303_system.started);
}
TEST_F(GD303SPIFactory, ForeignPollingVectorRejectsBeforeGlobalEffects) {
    g_gd303_pending[SPI0_IRQn] = true;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(0U, g_gd303_writes);
    EXPECT_TRUE(g_gd303_pending[SPI0_IRQn]);
}
