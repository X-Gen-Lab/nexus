/**
 * \file            regular_poll.h
 * \brief           Shared bounded STM32F1 software-triggered regular conversion
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F1_ADC_REGULAR_POLL_H
#define NX_STM32F1_ADC_REGULAR_POLL_H

#include "nexus/core/time.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_adc.h"

#ifndef NX_STM32F1_ADC_IRQ_IDLE
#define NX_STM32F1_ADC_IRQ_IDLE(irq)                                           \
    (NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&          \
     NVIC_GetActive(irq) == 0U)
#endif

enum { ADC_REGULAR_POLLS = 4096U };

static inline bool nx_stm32f1_adc_hsi8_clock(void) {
    const uint32_t fields =
        RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE;
    return (READ_REG(RCC->CFGR) & fields) == RCC_CFGR_SWS_HSI;
}

static inline bool nx_stm32f1_adc_reset(ADC_TypeDef* adc, uint32_t reset) {
    const bool asserted = nx_stm32f1_write_masked(&RCC->APB2RSTR, reset, reset);
    return nx_stm32f1_write_masked(&RCC->APB2RSTR, reset, 0U) && asserted &&
           READ_REG(adc->CR1) == 0U && READ_REG(adc->CR2) == 0U &&
           READ_REG(adc->SR) == 0U;
}

/** \brief Start one clean regular rank and return a new right-aligned code. */
static inline nx_result_t nx_stm32f1_adc_poll_rank(ADC_TypeDef* adc,
                                                   nx_time_us_t deadline,
                                                   uint16_t* out) {
    LL_ADC_REG_StartConversionSWStart(adc);
    nx_arch_dsb();
    for (unsigned poll = 0U; poll < ADC_REGULAR_POLLS; ++poll) {
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            break;
        const uint32_t completed = ADC_SR_EOC | ADC_SR_STRT;
        if ((READ_REG(adc->SR) & completed) == completed) {
            const uint16_t raw = LL_ADC_REG_ReadConversionData12(adc);
            if (raw > 0x0fffU)
                return NX_ERROR_IO;
            *out = raw;
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_TIMEOUT;
}

#endif
