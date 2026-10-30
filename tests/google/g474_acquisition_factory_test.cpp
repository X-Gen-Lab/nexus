/**
 * \file            g474_acquisition_factory_test.cpp
 * \brief           Actual ADC/I2C/Flash static construction and cold rollback
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_acquisition_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
ADC_TypeDef g_g474_acquisition_adc[2];
ADC_Common_TypeDef g_g474_acquisition_common;
TIM_TypeDef g_g474_acquisition_timer;
DMA_TypeDef g_g474_acquisition_dma;
DMA_Channel_TypeDef g_g474_acquisition_channel;
DMAMUX_Channel_TypeDef g_g474_acquisition_mux;
I2C_TypeDef g_g474_acquisition_i2c;
SYSCFG_TypeDef g_g474_acquisition_syscfg;
}
namespace {
bool hold_calibration;
} /* namespace */
extern "C" void nx_g474_acquisition_factory_write(volatile uint32_t* reg,
                                                  uint32_t value) {
    if (reg == &ADC1->ISR) {
        ADC1->ISR &= ~value;
        return;
    }
    if (reg == &DMA1->IFCR) {
        const_cast<volatile uint32_t&>(DMA1->ISR) &= ~value;
        return;
    }
    if (reg == &I2C1->ICR) {
        I2C1->ISR &= ~value;
        return;
    }
    if (reg == &RCC->APB1RSTR1 && (value & RCC_APB1RSTR1_I2C1RST) != 0U)
        std::memset(I2C1, 0, sizeof(*I2C1));
    nx_g474_model_write(reg, value);
}
extern "C" void nx_g474_acquisition_factory_poll(nx_stm32g474_adc_state_t* p) {
    ++g_g474_model.timer.CNT;
    if ((p->registers->CR & ADC_CR_ADCAL) != 0U && !hold_calibration)
        p->registers->CR &= ~(uint32_t)ADC_CR_ADCAL;
    if ((p->registers->CR & ADC_CR_ADEN) != 0U)
        p->registers->ISR |= ADC_ISR_ADRDY;
    if ((p->registers->CR & ADC_CR_ADSTP) != 0U)
        p->registers->CR &= ~(uint32_t)(ADC_CR_ADSTP | ADC_CR_ADSTART);
    if ((p->registers->CR & ADC_CR_ADDIS) != 0U)
        p->registers->CR &= ~(uint32_t)(ADC_CR_ADDIS | ADC_CR_ADEN);
}
class G474AcquisitionFactory : public testing::Test {
  protected:
    void SetUp() override {
        nx_g474_model_reset();
        std::memset(g_g474_acquisition_adc, 0, sizeof(g_g474_acquisition_adc));
        std::memset(ADC12_COMMON, 0, sizeof(*ADC12_COMMON));
        std::memset(TIM6, 0, sizeof(*TIM6));
        std::memset(DMA1, 0, sizeof(*DMA1));
        std::memset(DMA1_Channel1, 0, sizeof(*DMA1_Channel1));
        std::memset(DMAMUX1_Channel0, 0, sizeof(*DMAMUX1_Channel0));
        std::memset(I2C1, 0, sizeof(*I2C1));
        std::memset(SYSCFG, 0, sizeof(*SYSCFG));
        ADC1->CR = ADC2->CR = ADC_CR_DEEPPWD;
        hold_calibration = false;
    }
    void TearDown() override {
        g_g474_model.ignore_write = nullptr;
        hold_calibration = false;
        auto* state = static_cast<nx_stm32g474_adc_stream_state_t*>(
            nx_factory_adc(NX_ADC_ID_SENSOR)->context);
        if (state->adc.registers != nullptr)
            nx_g474_acquisition_factory_poll(&state->adc);
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
};
class G474I2cForeignHsiTrim : public G474AcquisitionFactory,
                              public testing::WithParamInterface<uint32_t> {};
TEST_P(G474I2cForeignHsiTrim, RejectsBeforeI2cMuxOrPlatformOwnership) {
    RCC->ICSCR =
        (GetParam() << RCC_ICSCR_HSITRIM_Pos) | (0xa5U << RCC_ICSCR_HSICAL_Pos);
    const RCC_TypeDef before = *RCC;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_EQ(0, std::memcmp(&before, RCC, sizeof(before)));
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, g_g474_model.writes);
}
INSTANTIATE_TEST_SUITE_P(NonResetTrims, G474I2cForeignHsiTrim,
                         testing::Values(0U, 16U, 63U, 65U, 127U));
TEST_F(G474AcquisitionFactory, DefaultTrimPreservesCalibrationAndI2cHsiMux) {
    const uint32_t internal =
        (64U << RCC_ICSCR_HSITRIM_Pos) | (0xa5U << RCC_ICSCR_HSICAL_Pos);
    RCC->ICSCR = internal;
    ASSERT_NE(nullptr,
              nx_factory_i2c_device(NX_I2C_DEVICE_ID_SENSOR_REGISTERS));
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    EXPECT_EQ(RCC_CCIPR_I2C1SEL_1, RCC->CCIPR & RCC_CCIPR_I2C1SEL);
    EXPECT_EQ(170000000U, g_nx_stm32g474_system.core_hz);
    EXPECT_EQ(internal, RCC->ICSCR);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(internal, RCC->ICSCR);
}
TEST_F(G474AcquisitionFactory, LookupIsPureAndFlashCapabilityMatchesGeometry) {
    const uint32_t before = g_g474_model.writes;
    const auto* sensor = nx_factory_adc(NX_ADC_ID_SENSOR);
    const auto* physical = nx_factory_flash(NX_FLASH_ID_PHYSICAL);
    const auto* capability = nx_factory_flash_capability(NX_FLASH_ID_PHYSICAL);
    ASSERT_NE(nullptr, sensor);
    ASSERT_NE(nullptr,
              nx_factory_i2c_device(NX_I2C_DEVICE_ID_SENSOR_REGISTERS));
    ASSERT_NE(nullptr, physical);
    ASSERT_NE(nullptr, capability);
    const auto* geometry = nx_flash_port_geometry(physical);
    ASSERT_NE(nullptr, geometry);
    EXPECT_EQ(0x08000000U, geometry->base_address);
    EXPECT_EQ(524288U, geometry->size);
    ASSERT_EQ(256U, geometry->sector_count);
    ASSERT_NE(nullptr, geometry->sectors);
    for (size_t index = 0U; index < 256U; ++index) {
        EXPECT_EQ(index * 2048U, geometry->sectors[index].offset);
        EXPECT_EQ(2048U, geometry->sectors[index].size);
    }
    EXPECT_EQ(8U, nx_flash_port_geometry(physical)->program_unit);
    EXPECT_EQ(NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE,
              nx_flash_port_geometry(physical)->program_flags);
    EXPECT_EQ(nx_flash_port_geometry(physical)->program_unit,
              capability->unit_bytes);
    EXPECT_EQ(1U, capability->buffer_alignment);
    EXPECT_EQ(before, g_g474_model.writes);
}
TEST_F(G474AcquisitionFactory,
       StartAndStopOwnEveryGateWithoutStartingAcquisition) {
    const uint32_t mode_a = GPIOA->MODER;
    const uint32_t mode_b = GPIOB->MODER;
    const auto started = nx_platform_start();
    ASSERT_EQ(NX_SUCCESS, started.primary);
    EXPECT_EQ(10U << ADC_CCR_PRESC_Pos, ADC12_COMMON->CCR);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN);
    EXPECT_EQ(RCC_CCIPR_ADC12SEL_1, RCC->CCIPR & RCC_CCIPR_ADC12SEL);
    EXPECT_EQ(5U, DMAMUX1_Channel0->CCR);
    EXPECT_EQ(0U, DMA1_Channel1->CCR & DMA_CCR_EN);
    EXPECT_EQ(0U, TIM6->CR1 & TIM_CR1_CEN);
    EXPECT_EQ(0U, ADC1->CR & ADC_CR_ADSTART);
    EXPECT_EQ(0x30420f13U, I2C1->TIMINGR);
    EXPECT_EQ(0U, FLASH->KEYR);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(mode_a, GPIOA->MODER);
    EXPECT_EQ(mode_b, GPIOB->MODER);
    EXPECT_EQ(0U, ADC12_COMMON->CCR);
    EXPECT_EQ(0U, DMAMUX1_Channel0->CCR);
    EXPECT_EQ(0U, RCC->AHB1ENR & (RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN));
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN);
    EXPECT_EQ(0U, RCC->APB1ENR1 & (RCC_APB1ENR1_I2C1EN | RCC_APB1ENR1_TIM6EN));
    EXPECT_EQ(0U, RCC->CCIPR & (RCC_CCIPR_I2C1SEL | RCC_CCIPR_ADC12SEL));
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN);
}
TEST_F(G474AcquisitionFactory, I2cPinFailureRollsBackEarlierAdcAndFlashOwners) {
    g_g474_model.ignore_write = &GPIOB->AFR[1];
    const auto started = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, started.primary);
    EXPECT_EQ(NX_SUCCESS, started.cleanup);
    EXPECT_EQ(0U, started.remaining_effects);
    EXPECT_EQ(ADC_CR_DEEPPWD, ADC1->CR);
    EXPECT_EQ(0U, ADC12_COMMON->CCR);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN);
}
TEST_F(G474AcquisitionFactory, ForeignDmaRejectsBeforeGlobalEffects) {
    RCC->AHB1ENR = RCC_AHB1ENR_DMA1EN;
    DMA1_Channel1->CCR = DMA_CCR_EN;
    const uint32_t before = g_g474_model.writes;
    const auto started = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, started.primary);
    EXPECT_EQ(before, g_g474_model.writes);
    EXPECT_EQ(DMA_CCR_EN, DMA1_Channel1->CCR);
}
TEST_F(G474AcquisitionFactory, FailedCalibrationRetainsTheOriginalClockLease) {
    hold_calibration = true;
    const auto started = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, started.primary);
    EXPECT_EQ(NX_ERROR_BUSY, started.cleanup);
    EXPECT_NE(0U, started.remaining_effects);
    EXPECT_NE(0U, RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN);
    hold_calibration = false;
    auto* state = static_cast<nx_stm32g474_adc_stream_state_t*>(
        nx_factory_adc(NX_ADC_ID_SENSOR)->context);
    nx_g474_acquisition_factory_poll(&state->adc);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN);
}

TEST_F(G474AcquisitionFactory, ForeignFlashModeRejectsBeforeGlobalEffects) {
    FLASH->CR = FLASH_CR_PG;
    const uint32_t before = g_g474_model.writes;
    const auto started = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, started.primary);
    EXPECT_EQ(0U, started.remaining_effects);
    EXPECT_EQ(before, g_g474_model.writes);
    EXPECT_EQ(FLASH_CR_PG, FLASH->CR);
    FLASH->CR = FLASH_CR_LOCK;
}
TEST_F(G474AcquisitionFactory,
       SwappedFlashBanksRollBackTheClockedMappingLease) {
    SYSCFG->MEMRMP = SYSCFG_MEMRMP_FB_MODE;
    const auto started = nx_platform_start();
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, started.primary);
    EXPECT_EQ(NX_SUCCESS, started.cleanup);
    EXPECT_EQ(0U, started.remaining_effects);
    EXPECT_EQ(0U, FLASH->KEYR);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN);
    EXPECT_EQ(0U, RCC->CCIPR & RCC_CCIPR_ADC12SEL);
}
TEST_F(G474AcquisitionFactory, RetainedAdcKernelMuxIsNotAdopted) {
    RCC->CCIPR = RCC_CCIPR_ADC12SEL_1;
    const uint32_t before = g_g474_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(before, g_g474_model.writes);
    RCC->CCIPR = 0U;
}

TEST_F(G474AcquisitionFactory, ForeignFlashIrqAndEccOwnerRejectBeforeClocks) {
    g_g474_model.irq_enabled[FLASH_IRQn] = true;
    uint32_t before = g_g474_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(before, g_g474_model.writes);
    g_g474_model.irq_enabled[FLASH_IRQn] = false;
    FLASH->ECCR = FLASH_ECCR_ECCIE;
    before = g_g474_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_platform_start().primary);
    EXPECT_EQ(before, g_g474_model.writes);
    FLASH->ECCR = 0U;
}
