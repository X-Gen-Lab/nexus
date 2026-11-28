/**
 * \file            adc_test.cpp
 * \brief           F103 ADC calibration, poll freshness and retained stop
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/adc/instance.h"
#include "nexus/arch/arch.h"
#include <gtest/gtest.h>

ADC_TypeDef f103_adc_model_adc{};
GPIO_TypeDef f103_adc_model_gpio{};
RCC_TypeDef f103_adc_model_rcc{};

static struct {
    nx_time_us_t now;
    nx_time_us_t powered_at;
    nx_time_us_t reset_until;
    nx_time_us_t cal_until;
    nx_time_us_t conversion_until;
    unsigned writes;
    unsigned resets;
    unsigned reset_calibrations;
    unsigned calibrations;
    unsigned starts;
    unsigned samples_read;
    bool live;
    bool irq_idle;
    bool hold_cal;
    bool hold_conversion;
    bool hold_power_down;
    bool hold_eoc;
} bus;

extern "C" uint32_t f103_adc_model_read(const volatile uint32_t* reg) {
    if (reg == &ADC1->DR) {
        ++bus.samples_read;
        if (!bus.hold_eoc) {
            ADC1->SR &= ~(uint32_t)ADC_SR_EOC;
        }
    }
    return *reg;
}

extern "C" void f103_adc_model_write(volatile uint32_t* reg, uint32_t value) {
    ++bus.writes;
    if (reg == &ADC1->CR2) {
        const uint32_t old = ADC1->CR2;
        if ((old & ADC_CR2_ADON) != 0U && (value & ADC_CR2_ADON) == 0U &&
            bus.hold_power_down) {
            return;
        }
        if ((old & ADC_CR2_ADON) == 0U && (value & ADC_CR2_ADON) != 0U) {
            bus.powered_at = bus.now;
        }
        if ((value & ADC_CR2_RSTCAL) != 0U && (old & ADC_CR2_RSTCAL) == 0U) {
            EXPECT_GE(bus.now - bus.powered_at, 4U);
            EXPECT_TRUE((value & ADC_CR2_ADON) != 0U);
            ++bus.reset_calibrations;
            bus.reset_until = bus.now + 4U;
        }
        if ((value & ADC_CR2_CAL) != 0U && (old & ADC_CR2_CAL) == 0U) {
            EXPECT_TRUE((value & ADC_CR2_ADON) != 0U);
            EXPECT_EQ(0U, old & ADC_CR2_RSTCAL);
            EXPECT_GT(bus.reset_calibrations, 0U);
            ++bus.calibrations;
            bus.cal_until = bus.now + 21U;
        }
        if ((value & ADC_CR2_SWSTART) != 0U && (old & ADC_CR2_SWSTART) == 0U) {
            EXPECT_GT(bus.calibrations, 0U);
            EXPECT_EQ(0U, old & ADC_CR2_CAL);
            EXPECT_EQ(static_cast<uint32_t>(ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG |
                                            ADC_CR2_ADON),
                      value &
                          (ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG | ADC_CR2_ADON));
            EXPECT_EQ(0U, value & (ADC_CR2_CONT | ADC_CR2_JEXTTRIG |
                                   ADC_CR2_JSWSTART | ADC_CR2_DMA));
            EXPECT_EQ(0U, ADC1->SR & ADC_SR_EOC);
            ++bus.starts;
            bus.conversion_until = bus.now + 10U;
        } else if (value == old && (value & ADC_CR2_ADON) != 0U) {
            ADD_FAILURE()
                << "Writing ADON alone can trigger an implicit conversion";
        }
        if ((value & ADC_CR2_ADON) == 0U) {
            bus.conversion_until = 0U;
        }
    } else if (reg == &ADC1->SR) {
        const uint32_t preserve = bus.hold_eoc ? ADC_SR_EOC : 0U;
        *reg &= value | preserve;
        return;
    } else if (reg == &RCC->APB2RSTR && (value & RCC_APB2RSTR_ADC1RST) != 0U) {
        ++bus.resets;
        ADC1->CR1 = 0U;
        ADC1->CR2 = 0U;
        ADC1->SR = 0U;
        ADC1->SMPR2 = 0U;
        ADC1->SQR1 = 0U;
        ADC1->SQR2 = 0U;
        ADC1->SQR3 = 0U;
    } else if (reg == &GPIOA->BSRR) {
        GPIOA->ODR = (GPIOA->ODR | (value & UINT16_MAX)) & ~(value >> 16U);
    }
    *reg = value;
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    ++bus.now;
    if (bus.reset_until != 0U && bus.now >= bus.reset_until) {
        ADC1->CR2 &= ~(uint32_t)ADC_CR2_RSTCAL;
        bus.reset_until = 0U;
    }
    if (!bus.hold_cal && bus.cal_until != 0U && bus.now >= bus.cal_until) {
        ADC1->CR2 &= ~(uint32_t)ADC_CR2_CAL;
        bus.cal_until = 0U;
    }
    if ((ADC1->CR2 & ADC_CR2_SWSTART) != 0U) {
        ADC1->CR2 &= ~(uint32_t)ADC_CR2_SWSTART;
        ADC1->SR |= ADC_SR_STRT;
    }
    if (!bus.hold_conversion && bus.conversion_until != 0U &&
        bus.now >= bus.conversion_until) {
        ADC1->SR |= ADC_SR_EOC;
        ADC1->DR = 0x8a0U + bus.starts;
        bus.conversion_until = 0U;
    }
    return bus.now;
}

extern "C" bool f103_adc_model_irq_idle(void) {
    return bus.irq_idle;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return bus.live;
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

class F103Adc : public testing::Test {
  protected:
    nx_stm32f1_adc_t state{};
    const nx_adc_port_t port{&nx_stm32f1_adc_ops, &state};

    void SetUp() override {
        bus = {};
        bus.live = true;
        bus.irq_idle = true;
        RCC->APB2ENR = 0U;
        RCC->APB2RSTR = 0U;
        RCC->CFGR = 0U;
        GPIOA->CRL = UINT32_C(0x44444444);
        GPIOA->CRH = UINT32_C(0x44444444);
        GPIOA->ODR = 0x1U;
        ADC1->CR1 = 0U;
        ADC1->CR2 = 0U;
        ADC1->SR = 0U;
    }

    nx_result_t prepare(uint32_t code = 5U) {
        return nx_stm32f1_adc_prepare(&state, 8000000U, 3300U, code);
    }
};

TEST_F(F103Adc, CalibratesOnceThenReportsRegularRawSamplesAndExactTiming) {
    ASSERT_EQ(NX_SUCCESS, prepare());
    EXPECT_EQ(1U, bus.calibrations);
    EXPECT_EQ(0U, bus.starts);
    EXPECT_EQ(0U, GPIOA->CRL & 0xfU);
    EXPECT_EQ(5U, ADC1->SMPR2 & ADC_SMPR2_SMP0);
    nx_adc_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_info(&port, &info));
    EXPECT_EQ(12U, info.resolution_bits);
    EXPECT_EQ(3300U, info.reference_mv);
    EXPECT_EQ(1U, info.channel_count);
    nx_adc_sequence_info_t timing{};
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_sequence_info(&port, &timing));
    EXPECT_EQ(68U, timing.scan_ticks);
    EXPECT_EQ(4000000U, timing.ticks_per_second);
    uint16_t output = 0U;
    size_t count = 7U;
    for (uint32_t expected : {0x8a1U, 0x8a2U}) {
        ASSERT_EQ(NX_SUCCESS, nx_adc_port_sample(&port, &output, 1U,
                                                 bus.now + 100U, &count));
        EXPECT_EQ(expected, output);
        EXPECT_EQ(1U, count);
        EXPECT_EQ(0U, ADC1->CR2 & ADC_CR2_EXTTRIG);
    }
    EXPECT_EQ(1U, bus.calibrations);
    EXPECT_EQ(2U, bus.samples_read);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_close(&state));
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(UINT32_C(0x44444444), GPIOA->CRL);
    EXPECT_EQ(1U, GPIOA->ODR & 1U);
}

TEST_F(F103Adc, RejectsUnreviewedClockSourceAndSharedVectorBeforeAnyWrite) {
    RCC->CFGR = RCC_CFGR_ADCPRE_DIV4;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, prepare());
    RCC->CFGR = 0U;
    bus.irq_idle = false;
    EXPECT_EQ(NX_ERROR_BUSY, prepare());
    bus.irq_idle = true;
    RCC->APB2ENR = RCC_APB2ENR_ADC2EN;
    EXPECT_EQ(NX_ERROR_BUSY, prepare());
    RCC->APB2ENR = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, prepare(8U));
    EXPECT_EQ(0U, bus.writes);
}

TEST_F(F103Adc, TimeoutPowersDownAndPublishesNoSampleOrLoan) {
    ASSERT_EQ(NX_SUCCESS, prepare());
    bus.hold_conversion = true;
    uint16_t output = 0x5555U;
    size_t count = 7U;
    ASSERT_EQ(NX_ERROR_TIMEOUT,
              nx_adc_port_sample(&port, &output, 1U, bus.now + 30U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0x5555U, output);
    EXPECT_EQ(0U, bus.samples_read);
    EXPECT_EQ(0U, ADC1->CR2 & ADC_CR2_ADON);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_adc_port_sample(&port, &output, 1U, bus.now + 100U, &count));
    EXPECT_EQ(1U, bus.starts);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_close(&state));
}

TEST_F(F103Adc, RefusedPowerDownRetainsGatesAndPadsUntilRetry) {
    ASSERT_EQ(NX_SUCCESS, prepare());
    bus.hold_power_down = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_adc_close(&state));
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_IOPAEN);
    EXPECT_EQ(0U, GPIOA->CRL & 0xfU);
    bus.hold_power_down = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_close(&state));
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(UINT32_C(0x44444444), GPIOA->CRL);
}

TEST_F(F103Adc, CalibrationFailureKeepsCleanupReceiptAndNoConversion) {
    bus.hold_cal = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, prepare());
    EXPECT_EQ(1U, bus.calibrations);
    EXPECT_EQ(0U, bus.starts);
    EXPECT_EQ(0U, ADC1->CR2 & ADC_CR2_ADON);
    EXPECT_NE(0U, RCC->APB2ENR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_close(&state));
    EXPECT_EQ(0U, RCC->APB2ENR);
}

TEST_F(F103Adc, StaleEocCannotStandForANewSample) {
    ASSERT_EQ(NX_SUCCESS, prepare());
    ADC1->SR = ADC_SR_STRT | ADC_SR_EOC;
    ADC1->DR = 0x444U;
    bus.hold_eoc = true;
    uint16_t output = 0x5555U;
    size_t count = 7U;
    ASSERT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&port, &output, 1U, bus.now + 100U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0x5555U, output);
    EXPECT_EQ(0U, bus.starts);
    EXPECT_EQ(0U, bus.samples_read);
    EXPECT_EQ(0U, ADC1->CR2 & ADC_CR2_ADON);
}

TEST_F(F103Adc, AfterCompletedSampleStopFailurePreservesItsCount) {
    ASSERT_EQ(NX_SUCCESS, prepare(7U));
    bus.hold_eoc = true;
    bus.hold_power_down = true;
    uint16_t output = 0x5555U;
    size_t count = 7U;
    ASSERT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&port, &output, 1U, bus.now + 100U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x8a1U, output);
    EXPECT_EQ(1U, bus.starts);
    EXPECT_NE(0U, ADC1->CR2 & ADC_CR2_ADON);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_adc_port_sample(&port, &output, 1U, bus.now + 100U, &count));
    bus.hold_power_down = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_close(&state));
}
