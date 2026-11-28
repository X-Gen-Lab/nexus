/**
 * \file            dual_dma_test.cpp
 * \brief           Dual ADC ordering, whole-pair commit and failed drain.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/adc/dual_dma.h"
#include "nexus/arch/arch.h"
#include <gtest/gtest.h>

ADC_TypeDef dual_model_master{};
ADC_TypeDef dual_model_slave{};
GPIO_TypeDef dual_model_gpio{};
RCC_TypeDef dual_model_rcc{};
DMA_TypeDef dual_model_dma{};
DMA_Channel_TypeDef dual_model_lane{};

namespace {
constexpr uint32_t converter_clocks = RCC_APB2ENR_ADC1EN | RCC_APB2ENR_ADC2EN;
constexpr uint32_t words = DMA_CCR_PSIZE_1 | DMA_CCR_MSIZE_1;
constexpr uint32_t software = ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG | ADC_CR2_ADON;
constexpr nx_time_us_t future = 1000U;

struct {
    nx_time_us_t time;
    nx_time_us_t complete;
    volatile uint32_t* packed;
    uint32_t calibration_mask;
    unsigned starts;
    bool irq_busy;
    bool transfer_error;
    bool late_error;
    bool hold_conversion;
    bool hold_disable;
    bool hold_slave_cal;
} bus;

class F103DualAdc : public testing::Test {
  protected:
    nx_stm32f1_adc_dual_dma_t owner{};
    nx_dma_memory_region_t region{};
    const nx_adc_port_t port{&nx_stm32f1_adc_dual_dma_ops, &owner};

    void SetUp() override {
        bus = {};
        dual_model_master = {};
        dual_model_slave = {};
        dual_model_gpio = {};
        dual_model_rcc = {};
        dual_model_dma = {};
        dual_model_lane = {};
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        GPIOA->CRL = 0x44444444U;
        bus.packed = &owner.packed;
        region = {reinterpret_cast<uintptr_t>(&owner), sizeof(owner),
                  NX_DMA_MEMORY_WRITE};
    }

    nx_result_t prepare(uint32_t code = 3U) {
        return nx_stm32f1_adc_dual_dma_prepare(&owner, 8000000U, 3300U, code,
                                               &region, 1U);
    }

    void TearDown() override {
        bus.hold_disable = false;
        EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_dual_dma_close(&owner));
    }
};

TEST_F(F103DualAdc, PacksIntoPrivateWordAndPublishesOrderedFreshPairs) {
    ASSERT_EQ(NX_SUCCESS, prepare());
    EXPECT_EQ(3U, bus.calibration_mask);
    EXPECT_EQ(1U, ADC1->SQR3);
    EXPECT_EQ(2U, ADC2->SQR3);
    EXPECT_EQ(0U, GPIOA->CRL & 0xff0U);
    nx_adc_sequence_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_sequence_info(&port, &info));
    EXPECT_EQ(2U, info.channel_count);
    EXPECT_EQ(41U, info.scan_ticks);
    EXPECT_EQ(4000000U, info.ticks_per_second);
    for (unsigned sequence : {1U, 2U}) {
        uint16_t samples[] = {0U, 0U, 42U};
        size_t count{};
        ASSERT_EQ(NX_SUCCESS,
                  nx_adc_port_sample(&port, samples, 3U, future, &count));
        EXPECT_EQ(2U, count);
        EXPECT_EQ(0x120U + sequence, samples[0]);
        EXPECT_EQ(0x8a0U + sequence, samples[1]);
        EXPECT_EQ(42U, samples[2]);
        EXPECT_EQ(0U, DMA1_Channel1->CMAR);
        EXPECT_EQ(0U, DMA1_Channel1->CPAR);
    }
}

TEST_F(F103DualAdc, SelectedSamplingWindowControlsBothConvertersAndDuration) {
    ASSERT_EQ(NX_SUCCESS, prepare(7U));
    EXPECT_EQ(7U << ADC_SMPR2_SMP1_Pos, ADC1->SMPR2);
    EXPECT_EQ(7U << ADC_SMPR2_SMP2_Pos, ADC2->SMPR2);
    nx_adc_sequence_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_sequence_info(&port, &info));
    EXPECT_EQ(252U, info.scan_ticks);
}

TEST_F(F103DualAdc, RejectsInheritedConverterOrLaneWithoutTakingIt) {
    RCC->APB2ENR = RCC_APB2ENR_ADC2EN;
    EXPECT_EQ(NX_ERROR_BUSY, prepare());
    EXPECT_EQ(RCC_APB2ENR_ADC2EN, RCC->APB2ENR);
    RCC->APB2ENR = 0U;
    RCC->AHBENR = RCC_AHBENR_DMA1EN;
    DMA1_Channel1->CNDTR = 4U;
    EXPECT_EQ(NX_ERROR_BUSY, prepare());
    EXPECT_EQ(4U, DMA1_Channel1->CNDTR);
    EXPECT_EQ(RCC_AHBENR_DMA1EN, RCC->AHBENR);
}

TEST_F(F103DualAdc, TransferErrorCannotPublishAPartialPair) {
    ASSERT_EQ(NX_SUCCESS, prepare());
    bus.transfer_error = true;
    uint16_t samples[] = {11U, 22U};
    size_t count = 9U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&port, samples, 2U, future, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(11U, samples[0]);
    EXPECT_EQ(22U, samples[1]);
    EXPECT_EQ(0U, DMA1_Channel1->CMAR);
    EXPECT_EQ(0U, ADC1->CR2);
    EXPECT_EQ(0U, ADC2->CR2);
}

TEST_F(F103DualAdc, LateDmaErrorDuringWithdrawalStillSuppressesResults) {
    ASSERT_EQ(NX_SUCCESS, prepare());
    bus.late_error = true;
    uint16_t samples[] = {11U, 22U};
    size_t count = 9U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&port, samples, 2U, future, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(11U, samples[0]);
    EXPECT_EQ(22U, samples[1]);
    EXPECT_EQ(0U, DMA1_Channel1->CMAR);
}

TEST_F(F103DualAdc, FailedTimeoutDrainRetainsPrivateStorageUntilWithdrawal) {
    ASSERT_EQ(NX_SUCCESS, prepare());
    bus.hold_conversion = bus.hold_disable = true;
    uint16_t samples[] = {11U, 22U};
    size_t count{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&port, samples, 2U, bus.time + 30U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(11U, samples[0]);
    EXPECT_EQ(22U, samples[1]);
    EXPECT_NE(0U, DMA1_Channel1->CMAR);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_adc_port_sample(&port, samples, 2U, future, &count));
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_adc_dual_dma_close(&owner));
    bus.hold_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_dual_dma_close(&owner));
    EXPECT_EQ(0U, DMA1_Channel1->CMAR);
    EXPECT_EQ(0U, RCC->APB2ENR & converter_clocks);
}

TEST_F(F103DualAdc, SlaveCalibrationFailureNeverArmsTheLane) {
    bus.hold_slave_cal = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, prepare());
    EXPECT_EQ(0U, bus.starts);
    EXPECT_EQ(0U, DMA1_Channel1->CMAR);
}
} /* namespace */

void dual_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &RCC->APB2RSTR) {
        if ((value & RCC_APB2RSTR_ADC1RST) != 0U)
            *ADC1 = {};
        if ((value & RCC_APB2RSTR_ADC2RST) != 0U)
            *ADC2 = {};
    } else if (reg == &DMA1->IFCR) {
        EXPECT_EQ(0U, value & DMA_IFCR_CGIF1);
        DMA1->ISR &= ~value;
        return;
    } else if (reg == &GPIOA->BSRR) {
        GPIOA->ODR = (GPIOA->ODR | (value & 0xffffU)) & ~(value >> 16U);
    } else if (reg == &DMA1_Channel1->CCR && (*reg & DMA_CCR_EN) != 0U &&
               (value & DMA_CCR_EN) == 0U) {
        if (bus.hold_disable)
            return;
        if (bus.late_error)
            DMA1->ISR |= DMA_ISR_TEIF1;
    } else if (reg == &ADC1->CR2 || reg == &ADC2->CR2) {
        if ((value & ADC_CR2_CAL) != 0U &&
            (reg != &ADC2->CR2 || !bus.hold_slave_cal)) {
            bus.calibration_mask |= reg == &ADC1->CR2 ? 1U : 2U;
            value &= ~static_cast<uint32_t>(ADC_CR2_CAL);
        }
        value &= ~static_cast<uint32_t>(ADC_CR2_RSTCAL);
        if ((value & ADC_CR2_SWSTART) != 0U) {
            ASSERT_EQ(&ADC1->CR2, reg);
            EXPECT_EQ(software | ADC_CR2_DMA,
                      value & ~static_cast<uint32_t>(ADC_CR2_SWSTART));
            EXPECT_EQ(software, ADC2->CR2);
            EXPECT_EQ(3U, bus.calibration_mask);
            EXPECT_EQ(1U, DMA1_Channel1->CNDTR);
            EXPECT_EQ(words | DMA_CCR_EN, DMA1_Channel1->CCR);
            EXPECT_EQ(
                static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ADC1->DR)),
                DMA1_Channel1->CPAR);
            EXPECT_EQ(
                static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bus.packed)),
                DMA1_Channel1->CMAR);
            ++bus.starts;
            bus.complete = bus.time + 11U;
            value &= ~static_cast<uint32_t>(ADC_CR2_SWSTART);
        }
    }
    *reg = value;
}

bool dual_model_irq_idle(void) {
    return !bus.irq_busy;
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    ++bus.time;
    if (!bus.hold_conversion && bus.complete != 0U &&
        bus.time >= bus.complete) {
        bus.complete = 0U;
        *bus.packed = ((0x8a0U + bus.starts) << 16U) | (0x120U + bus.starts);
        DMA1_Channel1->CNDTR = 0U;
        DMA1->ISR |= DMA_ISR_TCIF1 | (bus.transfer_error ? DMA_ISR_TEIF1 : 0U);
    }
    return bus.time;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
extern "C" void nx_arch_dsb(void) {
}
