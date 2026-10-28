/**
 * \file            u575_peripheral_factory_test.cpp
 * \brief           Actual generated U575 peripheral assembly and cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32u575_provider.h"
#include "u575_devices_model.h"
#include <gtest/gtest.h>
class U575PeripheralFactory : public testing::Test {
  protected:
    SPI_TypeDef spi{};
    nx_i2c_v2_registers_t& i2c{g_u575_i2c_registers};
    ADC_TypeDef& adc{g_u575_adc_registers};
    ADC_Common_TypeDef& common{g_u575_adc_common_registers};
    void SetUp() override {
        nx_u575_devices_reset();
        g_u575_spi = &spi;
        g_u575_i2c = &i2c;
        g_u575_adc = &adc;
        g_u575_adc_common = &common;
        g_u575_physical_flash = &g_u575_model.flash;
        g_u575_model.flash.NSCR = FLASH_NSCR_LOCK | FLASH_NSCR_OPTLOCK;
        g_u575_model.rcc.AHB3ENR = RCC_AHB3ENR_PWREN;
        g_u575_model.power.SVMCR = PWR_SVMCR_ASV;
        spi.SR = SPI_SR_TXP | SPI_SR_TXC;
        spi.CFG1 = 7U | (7U << SPI_CFG1_CRCSIZE_Pos);
    }
    void TearDown() override {
        g_u575_model.ignore_write = nullptr;
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
        nx_u575_devices_reset();
    }
};
TEST_F(U575PeripheralFactory, ColdAssemblyAndStopPreserveExternalPowerOwner) {
    auto start = nx_platform_start();
    ASSERT_EQ(NX_SUCCESS, start.primary);
    ASSERT_NE(nullptr, nx_factory_spi(NX_SPI_ID_BUS));
    EXPECT_EQ(&nx_stm32u575_spi_ops, nx_factory_spi(NX_SPI_ID_BUS)->ops);
    EXPECT_EQ(&nx_i2c_v2_ops, nx_factory_i2c(NX_I2C_ID_CONTROL)->ops);
    EXPECT_EQ(RCC_CCIPR1_I2C1SEL_1,
              g_u575_model.rcc.CCIPR1 & RCC_CCIPR1_I2C1SEL);
    EXPECT_NE(0U, g_u575_model.rcc.CR & RCC_CR_HSIRDY);
    EXPECT_EQ(&nx_stm32u575_adc_ops, nx_factory_adc(NX_ADC_ID_SENSE)->ops);
    const auto* capability = nx_factory_flash_capability(NX_FLASH_ID_STORAGE);
    const auto* geometry =
        nx_flash_port_geometry(nx_factory_flash(NX_FLASH_ID_STORAGE));
    ASSERT_NE(nullptr, capability);
    ASSERT_NE(nullptr, geometry);
    EXPECT_EQ(0x08000000U, geometry->base_address);
    EXPECT_EQ(2097152U, geometry->size);
    ASSERT_EQ(256U, geometry->sector_count);
    ASSERT_NE(nullptr, geometry->sectors);
    for (size_t index = 0U; index < 256U; ++index) {
        EXPECT_EQ(index * 8192U, geometry->sectors[index].offset);
        EXPECT_EQ(8192U, geometry->sectors[index].size);
    }
    EXPECT_EQ(geometry->program_unit, capability->unit_bytes);
    EXPECT_EQ(1U, capability->buffer_alignment);
    EXPECT_EQ(16U, GPIOA->ODR & 16U);
    EXPECT_EQ(0x300U, GPIOB->OTYPER & 0x300U);
    EXPECT_EQ(3U, GPIOC->MODER & 3U);
    EXPECT_EQ(0U, g_u575_model.flash.NSKEYR);
    EXPECT_TRUE(g_u575_model.irq_enabled[EXTI1_IRQn]);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(0U, GPIOC->MODER);
    EXPECT_EQ(0U, GPIOA->ODR);
    EXPECT_EQ(0U, g_u575_model.rcc.APB1ENR1);
    EXPECT_EQ(0U, g_u575_model.rcc.APB2ENR);
    EXPECT_EQ(RCC_AHB3ENR_PWREN, g_u575_model.rcc.AHB3ENR);
    EXPECT_EQ(PWR_SVMCR_ASV, g_u575_model.power.SVMCR);
    EXPECT_FALSE(g_u575_model.irq_enabled[EXTI1_IRQn]);
    EXPECT_EQ(0U, g_u575_model.rcc.CCIPR1 & RCC_CCIPR1_I2C1SEL);
    EXPECT_EQ(0U, g_u575_model.rcc.CR & (RCC_CR_HSION | RCC_CR_HSIRDY));
}
TEST_F(U575PeripheralFactory, ForeignEndpointChipSelectRejectsBeforeClocks) {
    g_u575_model.rcc.AHB2ENR1 = RCC_AHB2ENR1_GPIOAEN;
    GPIOA->MODER = 1U << 8U;
    auto start = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, start.primary);
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
}
TEST_F(U575PeripheralFactory, FailedAdcGateRestoresLeasesAndAddedClocks) {
    g_u575_model.ignore_write = &g_u575_model.rcc.AHB3ENR;
    auto start = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, start.primary);
    EXPECT_EQ(NX_SUCCESS, start.cleanup);
    EXPECT_EQ(0U, start.remaining_effects);
    EXPECT_EQ(0U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOB->MODER);
    EXPECT_EQ(0U, g_u575_model.rcc.APB1ENR1);
    EXPECT_EQ(RCC_AHB3ENR_PWREN, g_u575_model.rcc.AHB3ENR);
}
TEST_F(U575PeripheralFactory, ForeignKernelSourceRejectsBeforeSystemEffects) {
    g_u575_model.rcc.CR |= RCC_CR_HSIKERON;
    auto start = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, start.primary);
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
    EXPECT_NE(0U, g_u575_model.rcc.CR & RCC_CR_HSIKERON);
}
TEST_F(U575PeripheralFactory, ForeignFlashInterruptRejectsBeforeSystemEffects) {
    g_u575_model.irq_pending[FLASH_IRQn] = true;
    auto start = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, start.primary);
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
    EXPECT_TRUE(g_u575_model.irq_pending[FLASH_IRQn]);
    g_u575_model.irq_pending[FLASH_IRQn] = false;
}
TEST_F(U575PeripheralFactory, FailedHsiStartRollsBackAllOwnedEffects) {
    g_u575_hsi_hold_on = true;
    auto start = nx_platform_start();
    EXPECT_EQ(NX_ERROR_TIMEOUT, start.primary);
    EXPECT_EQ(NX_SUCCESS, start.cleanup);
    EXPECT_EQ(0U, start.remaining_effects);
    EXPECT_FALSE(g_nx_stm32u575_system.started);
    EXPECT_EQ(0U, g_u575_model.rcc.CR & (RCC_CR_HSION | RCC_CR_HSIRDY));
    EXPECT_EQ(0U, g_u575_model.rcc.APB1ENR1);
    EXPECT_EQ(RCC_AHB3ENR_PWREN, g_u575_model.rcc.AHB3ENR);
}
TEST_F(U575PeripheralFactory, FailedClockWithdrawalBlocksSystemStopUntilRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    g_u575_hsi_hold_off = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32u575_system.started);
    EXPECT_NE(0U, g_u575_model.rcc.APB1ENR1 & RCC_APB1ENR1_I2C1EN);
    g_u575_hsi_hold_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_FALSE(g_nx_stm32u575_system.started);
}
