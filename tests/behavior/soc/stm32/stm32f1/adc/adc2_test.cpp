/**
 * \file            adc2_test.cpp
 * \brief           Independent F103ZE ADC2 and ADC3 regular polling model
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/adc/instance.h"
#include "nexus/arch/arch.h"
#include <gtest/gtest.h>

ADC_TypeDef f103_adc_model_adc{};
ADC_TypeDef f103_adc_model_adc2{};
ADC_TypeDef f103_adc_model_adc3{};
GPIO_TypeDef f103_adc_model_gpio{};
RCC_TypeDef f103_adc_model_rcc{};

#if defined(NX_STM32F1_ADC3_POLL)
#define TEST_ADC        ADC3
#define TEST_GATE       RCC_APB2ENR_ADC3EN
#define TEST_RESET      RCC_APB2RSTR_ADC3RST
#define TEST_OTHER      ADC2
#define TEST_OTHER_GATE RCC_APB2ENR_ADC2EN
#define TEST_IRQ        ADC3_IRQn
constexpr unsigned channel = 3U;
#else
#define TEST_ADC        ADC2
#define TEST_GATE       RCC_APB2ENR_ADC2EN
#define TEST_RESET      RCC_APB2RSTR_ADC2RST
#define TEST_OTHER      ADC3
#define TEST_OTHER_GATE RCC_APB2ENR_ADC3EN
#define TEST_IRQ        ADC1_2_IRQn
constexpr unsigned channel = 2U;
#endif

static nx_time_us_t now;
static bool held;
static bool irq_idle;
static unsigned calibrated;
static unsigned writes;

extern "C" uint32_t f103_adc_model_read(const volatile uint32_t* reg) {
    if (reg == &TEST_ADC->DR)
        TEST_ADC->SR &= ~static_cast<uint32_t>(ADC_SR_EOC);
    return *reg;
}

extern "C" void f103_adc_model_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
    if (reg == &RCC->APB2RSTR) {
        EXPECT_EQ(0U, value & ((RCC_APB2RSTR_ADC1RST | RCC_APB2RSTR_ADC2RST |
                                RCC_APB2RSTR_ADC3RST) &
                               ~TEST_RESET));
        if ((value & TEST_RESET) != 0U)
            *TEST_ADC = {};
        *TEST_OTHER = {};
        TEST_OTHER->DR = 0x623U;
    } else if (reg == &TEST_ADC->CR2) {
        EXPECT_EQ(0U,
                  value & (ADC_CR2_DMA | ADC_CR2_JEXTTRIG | ADC_CR2_TSVREFE));
        if ((value & ADC_CR2_CAL) != 0U) {
            EXPECT_NE(0U, value & ADC_CR2_ADON);
            EXPECT_EQ(0U, value & ADC_CR2_RSTCAL);
            ++calibrated;
        }
    }
    *reg = value;
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    TEST_ADC->CR2 &= ~static_cast<uint32_t>(ADC_CR2_CAL | ADC_CR2_RSTCAL);
    if (!held && (TEST_ADC->CR2 & ADC_CR2_SWSTART) != 0U) {
        EXPECT_EQ(channel, TEST_ADC->SQR3);
        TEST_ADC->CR2 &= ~static_cast<uint32_t>(ADC_CR2_SWSTART);
        TEST_ADC->SR |= ADC_SR_STRT | ADC_SR_EOC;
        TEST_ADC->DR = 0x0ba5U;
    }
    return ++now;
}

extern "C" bool f103_adc_model_poll_irq_idle(IRQn_Type irq) {
    EXPECT_EQ(TEST_IRQ, irq);
    return irq_idle;
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

class F103HighDensityAdc : public testing::Test {
  protected:
    nx_stm32f1_adc_t owner{};
    const nx_adc_port_t port{&nx_stm32f1_adc_ops, &owner};

    void SetUp() override {
        *ADC1 = {};
        ADC1->DR = 0x512U;
        *TEST_ADC = {};
        *GPIOA = {};
        GPIOA->CRL = 0x44444444U;
        *RCC = {};
        now = writes = calibrated = 0U;
        held = false;
        irq_idle = true;
    }

    void TearDown() override {
        EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_close(&owner));
        EXPECT_EQ(0x512U, ADC1->DR);
        EXPECT_EQ(0x623U, TEST_OTHER->DR);
    }

    nx_result_t prepare() {
        return nx_stm32f1_adc_prepare(&owner, 8000000U, 3300U, 5U);
    }
};

TEST_F(F103HighDensityAdc, CalibratesAndReadsOnlyTheSelectedConverter) {
    ASSERT_EQ(NX_SUCCESS, prepare());
    EXPECT_EQ(TEST_GATE | RCC_APB2ENR_IOPAEN, RCC->APB2ENR);
    EXPECT_EQ(0x44444444U & ~(0xFU << (4U * channel)), GPIOA->CRL);
    EXPECT_EQ(1U, calibrated);
    EXPECT_EQ(5U << (3U * channel), TEST_ADC->SMPR2);
    nx_adc_sequence_info_t timing;
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_sequence_info(&port, &timing));
    EXPECT_EQ(68U, timing.scan_ticks);
    EXPECT_EQ(4000000U, timing.ticks_per_second);
    uint16_t raw = 0U;
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_port_sample(&port, &raw, 1U, now + 30U, &count));
    EXPECT_EQ(0xba5U, raw);
    EXPECT_EQ(1U, count);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_adc_close(&owner));
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
}

TEST_F(F103HighDensityAdc,
       RefusesOtherConvertersPrescalerAndVectorWithoutWrites) {
    for (const auto gate : {RCC_APB2ENR_ADC1EN, TEST_OTHER_GATE}) {
        RCC->APB2ENR = static_cast<uint32_t>(gate);
        EXPECT_EQ(NX_ERROR_BUSY, prepare());
        EXPECT_EQ(gate, RCC->APB2ENR);
    }
    RCC->APB2ENR = 0U;
    RCC->CFGR = RCC_CFGR_ADCPRE_0;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, prepare());
    RCC->CFGR = 0U;
    irq_idle = false;
    EXPECT_EQ(NX_ERROR_BUSY, prepare());
    EXPECT_EQ(0U, writes);
}

TEST_F(F103HighDensityAdc, TimeoutWithdrawsWithoutPublishingAndNeedsClose) {
    ASSERT_EQ(NX_SUCCESS, prepare());
    held = true;
    uint16_t raw = 0xbeefU;
    size_t count = 4U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_adc_port_sample(&port, &raw, 1U, now + 10U, &count));
    EXPECT_EQ(0xbeefU, raw);
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0U, TEST_ADC->CR2);
    EXPECT_NE(0U, RCC->APB2ENR & TEST_GATE);
}
