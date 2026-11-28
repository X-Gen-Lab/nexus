/**
 * \file            adc_test.cpp
 * \brief           F103 ADC calibration, poll freshness and retained stop
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/adc/instance.h"
#include "drivers/adc/trigger_dma.h"
#include "nexus/arch/arch.h"
#include <gtest/gtest.h>

ADC_TypeDef f103_adc_model_adc{};
GPIO_TypeDef f103_adc_model_gpio{};
RCC_TypeDef f103_adc_model_rcc{};
TIM_TypeDef f103_adc_model_tim{};
TIM_TypeDef f103_adc_model_tim1{};
DMA_TypeDef f103_adc_model_dma{};
DMA_Channel_TypeDef f103_adc_model_lane{};

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
    bool hold_dma_disable;
    bool transfer_error;
    bool finish_while_starting;
    unsigned triggers;
    nx_time_us_t transfer_at;
    volatile uint16_t* stage;
    TIM_TypeDef* trigger_timer;
    bool skip_compare_flag;
    bool adc_completes_first;
    nx_time_us_t timer_done_at;
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
    } else if (reg == &DMA1->IFCR) {
        EXPECT_EQ(0U, value & DMA_IFCR_CGIF1);
        DMA1->ISR &= ~value;
        return;
    } else if (reg == &DMA1_Channel1->CCR && (*reg & DMA_CCR_EN) != 0U &&
               (value & DMA_CCR_EN) == 0U && bus.hold_dma_disable) {
        return;
    } else if ((reg == &TIM3->CR1 || reg == &TIM1->CR1) &&
               (value & TIM_CR1_CEN) != 0U) {
        TIM_TypeDef* timer = reg == &TIM1->CR1 ? TIM1 : TIM3;
        const bool tim1 = timer == TIM1;
        bus.trigger_timer = timer;
        EXPECT_EQ(TIM_CR1_OPM, value & TIM_CR1_OPM);
        EXPECT_EQ(tim1 ? 0U : TIM_CR2_MMS_1, timer->CR2 & TIM_CR2_MMS);
        EXPECT_EQ(0U, timer->DIER);
        if (tim1) {
            EXPECT_EQ(2U, timer->CCR1);
            EXPECT_EQ(0U, timer->CCER);
            EXPECT_EQ(0U, timer->BDTR);
        }
        EXPECT_NE(0U, DMA1_Channel1->CCR & DMA_CCR_EN);
        EXPECT_EQ(1U, DMA1_Channel1->CNDTR);
        EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bus.stage)),
                  DMA1_Channel1->CMAR);
        EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ADC1->DR)),
                  DMA1_Channel1->CPAR);
        EXPECT_EQ(static_cast<uint32_t>(ADC_CR2_ADON | ADC_CR2_DMA |
                                        ADC_CR2_EXTTRIG |
                                        (tim1 ? 0U : ADC_CR2_EXTSEL_2)),
                  ADC1->CR2);
        ++bus.triggers;
        bus.transfer_at = bus.now + 12U;
        if (bus.finish_while_starting) {
            timer->CR1 = value & ~static_cast<uint32_t>(TIM_CR1_CEN);
            timer->SR = TIM_SR_UIF |
                        (tim1 && !bus.skip_compare_flag ? TIM_SR_CC1IF : 0U);
            DMA1_Channel1->CNDTR = 0U;
            DMA1->ISR |= DMA_ISR_TCIF1;
            *bus.stage = static_cast<uint16_t>(0x800U + bus.triggers);
            bus.transfer_at = 0U;
            return;
        }
    } else if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_TIM3RST) != 0U) {
        *TIM3 = {};
        TIM3->ARR = UINT16_MAX;
    } else if (reg == &RCC->APB2RSTR && (value & RCC_APB2RSTR_TIM1RST) != 0U) {
        *TIM1 = {};
        TIM1->ARR = UINT16_MAX;
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
    if (!bus.hold_conversion && bus.transfer_at != 0U &&
        bus.now >= bus.transfer_at &&
        (bus.trigger_timer->CR1 & TIM_CR1_CEN) != 0U) {
        bus.timer_done_at = bus.now + (bus.adc_completes_first ? 2U : 0U);
        bus.transfer_at = 0U;
        if (bus.transfer_error) {
            DMA1->ISR |= DMA_ISR_TEIF1;
        } else {
            *bus.stage = static_cast<uint16_t>(0x800U + bus.triggers);
            DMA1_Channel1->CNDTR = 0U;
            DMA1->ISR |= DMA_ISR_TCIF1;
        }
    }
    if (bus.timer_done_at != 0U && bus.now >= bus.timer_done_at) {
        auto* timer = bus.trigger_timer;
        timer->CR1 &= ~static_cast<uint32_t>(TIM_CR1_CEN);
        timer->SR =
            TIM_SR_UIF |
            (timer == TIM1 && !bus.skip_compare_flag ? TIM_SR_CC1IF : 0U);
        bus.timer_done_at = 0U;
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

class F103AdcTrigger : public F103Adc {
  protected:
    nx_stm32f1_adc_trigger_dma_t triggered{};
    nx_adc_port_t face{&nx_stm32f1_adc_trigger_dma_ops, &triggered};
    nx_dma_memory_region_t memory{};
    void SetUp() override {
        F103Adc::SetUp();
        RCC->AHBENR = 0U;
        RCC->APB1ENR = 0U;
        RCC->APB1RSTR = 0U;
        *TIM3 = {};
        TIM3->ARR = UINT16_MAX;
        *TIM1 = {};
        TIM1->ARR = UINT16_MAX;
        *DMA1 = {};
        *DMA1_Channel1 = {};
        memory = {reinterpret_cast<uintptr_t>(&triggered.sample), 2U,
                  NX_DMA_MEMORY_WRITE};
        bus.stage = &triggered.sample;
    }

    nx_result_t prepare_trigger(bool tim1 = false) {
        return nx_stm32f1_adc_trigger_dma_prepare(&triggered, 8000000U, 3300U,
                                                  5U, &memory, 1U, tim1);
    }

    void TearDown() override {
        bus.hold_dma_disable = false;
        EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_trigger_dma_close(&triggered));
    }
};

TEST_F(F103AdcTrigger, CalibratesOnceAndStartsDmaBeforeTheOnePulseTrigger) {
    ASSERT_EQ(NX_SUCCESS, prepare_trigger());
    uint16_t sample = 0U;
    size_t count = 0U;
    for (unsigned shot = 1U; shot <= 2U; ++shot) {
        bus.finish_while_starting = shot == 2U;
        ASSERT_EQ(NX_SUCCESS, nx_adc_port_sample(&face, &sample, 1U,
                                                 bus.now + 100U, &count));
        EXPECT_EQ(1U, count);
        EXPECT_EQ(0x800U + shot, sample);
        EXPECT_EQ(0U, DMA1_Channel1->CCR);
        EXPECT_EQ(0U, DMA1_Channel1->CMAR);
    }
    EXPECT_EQ(2U, bus.triggers);
    EXPECT_EQ(1U, bus.calibrations);
    EXPECT_EQ(0U, bus.starts);
    EXPECT_EQ(0U, bus.samples_read);
}

TEST_F(F103AdcTrigger, TransferErrorDoesNotPublishAStagedValue) {
    ASSERT_EQ(NX_SUCCESS, prepare_trigger());
    bus.transfer_error = true;
    uint16_t sample = 77U;
    size_t count = 2U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&face, &sample, 1U, bus.now + 100U, &count));
    EXPECT_EQ(77U, sample);
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0U, ADC1->CR2);
    EXPECT_EQ(0U, DMA1_Channel1->CCR & DMA_CCR_EN);
}

TEST_F(F103AdcTrigger, TimeoutHoldsOneStaticDestinationUntilLaneStops) {
    ASSERT_EQ(NX_SUCCESS, prepare_trigger());
    bus.hold_conversion = true;
    bus.hold_dma_disable = true;
    uint16_t sample = 91U;
    size_t count = 2U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&face, &sample, 1U, bus.now + 20U, &count));
    EXPECT_EQ(91U, sample);
    EXPECT_EQ(0U, count);
    EXPECT_NE(0U, DMA1_Channel1->CMAR);
    EXPECT_EQ(0U, ADC1->CR2);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_adc_trigger_dma_close(&triggered));
    EXPECT_NE(0U, RCC->AHBENR & RCC_AHBENR_DMA1EN);
    bus.hold_dma_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_trigger_dma_close(&triggered));
    EXPECT_EQ(0U, DMA1_Channel1->CMAR);
    EXPECT_EQ(0U, RCC->AHBENR & RCC_AHBENR_DMA1EN);
}

TEST_F(F103AdcTrigger, RejectsForeignTimerAndPreservesAValidSharedDmaGate) {
    RCC->APB1ENR = RCC_APB1ENR_TIM3EN;
    EXPECT_EQ(NX_ERROR_BUSY, prepare_trigger());
    EXPECT_EQ(0U, bus.writes);
    RCC->APB1ENR = 0U;
    RCC->AHBENR = RCC_AHBENR_DMA1EN;
    ASSERT_EQ(NX_SUCCESS, prepare_trigger());
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_trigger_dma_close(&triggered));
    EXPECT_EQ(RCC_AHBENR_DMA1EN, RCC->AHBENR);
}

TEST_F(F103AdcTrigger, RequiresTheWholeStaticSampleInDmaVisibleSram) {
    memory.size = 1U;
    EXPECT_EQ(NX_ERROR_PERMISSION, prepare_trigger());
    EXPECT_EQ(0U, bus.writes);
}

TEST_F(F103AdcTrigger, Tim1CompareTriggersOneRegularResultWithoutOutputs) {
    ASSERT_EQ(NX_SUCCESS, prepare_trigger(true));
    uint16_t output = 0x1234U;
    size_t count = 0U;
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_TIM1EN);
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_TIM3EN);
    EXPECT_EQ(2U, TIM1->CCR1);
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_port_sample(&face, &output, 1U, bus.now + 100U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x801U, output);
    EXPECT_EQ(0U, TIM1->BDTR);
    EXPECT_EQ(0U, TIM1->CCER);
    EXPECT_NE(0U, TIM1->SR & TIM_SR_CC1IF);
    EXPECT_EQ(0U, bus.starts);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_adc_trigger_dma_close(&triggered));
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_TIM1EN);
}

TEST_F(F103AdcTrigger, Tim1RejectsAForeignOutputOrUnprovenCompare) {
    RCC->APB2ENR = RCC_APB2ENR_TIM1EN;
    EXPECT_EQ(NX_ERROR_BUSY, prepare_trigger(true));
    EXPECT_EQ(0U, bus.writes);
    RCC->APB2ENR = 0U;
    ASSERT_EQ(NX_SUCCESS, prepare_trigger(true));
    TIM1->CCER = TIM_CCER_CC1E;
    uint16_t output = 42U;
    size_t count = 1U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&face, &output, 1U, bus.now + 100U, &count));
    EXPECT_EQ(0U, bus.triggers);
    TIM1->CCER = 0U;
    bus.skip_compare_flag = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&face, &output, 1U, bus.now + 100U, &count));
    EXPECT_EQ(1U, bus.triggers);
    EXPECT_EQ(0U, count);
    EXPECT_EQ(42U, output);
    EXPECT_EQ(0U, DMA1_Channel1->CMAR);
    EXPECT_EQ(0U, ADC1->CR2);
}

TEST_F(F103AdcTrigger, Tim1FastDmaResultStillWaitsForTimerUpdate) {
    ASSERT_EQ(NX_SUCCESS, prepare_trigger(true));
    bus.adc_completes_first = true;
    uint16_t output = 0U;
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_port_sample(&face, &output, 1U, bus.now + 100U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0x801U, output);
    EXPECT_EQ(0U, bus.timer_done_at);
    EXPECT_NE(0U, TIM1->SR & TIM_SR_UIF);
    EXPECT_EQ(0U, TIM1->CR1 & TIM_CR1_CEN);
}
