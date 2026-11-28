/**
 * \file            regular_calibration.h
 * \brief           Bounded STM32F1 regular ADC calibration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F1_ADC_REGULAR_CALIBRATION_H
#define NX_STM32F1_ADC_REGULAR_CALIBRATION_H

#include "nexus/core/time.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_adc.h"

enum {
    POWER_SETTLE_US = 4U,
    CALIBRATION_BUDGET_US = 256U,
    CALIBRATION_POLLS = 4096U
};

/* RM0008 sampling windows plus the 12.5-cycle SAR conversion. */
static const uint8_t nx_stm32f1_adc_regular_ticks[] = {14U, 20U, 26U, 41U,
                                                       54U, 68U, 84U, 252U};

static inline bool nx_stm32f1_adc_power_down(ADC_TypeDef* adc) {
    /* ADON=0 aborts conversion and calibration without any output loan. */
    WRITE_REG(adc->CR2, 0U);
    nx_arch_dsb();
    return READ_REG(adc->CR2) == 0U;
}

static inline bool nx_stm32f1_adc_power_settled(void) {
    const nx_time_us_t start = nx_time_now_us();
    for (unsigned i = 0U; i < CALIBRATION_POLLS; ++i) {
        if (nx_time_now_us() - start >= POWER_SETTLE_US) {
            return true;
        }
    }
    return false;
}

static inline bool nx_stm32f1_adc_calibration_finished(ADC_TypeDef* adc,
                                                       uint32_t flag,
                                                       nx_time_us_t deadline) {
    for (unsigned i = 0U; i < CALIBRATION_POLLS; ++i) {
        if ((READ_REG(adc->CR2) & flag) == 0U) {
            return !nx_deadline_expired(deadline, nx_time_now_us());
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            break;
        }
    }
    return false;
}

static inline nx_result_t nx_stm32f1_adc_calibrate(ADC_TypeDef* adc,
                                                   uint32_t ready) {
    nx_time_us_t deadline;
    nx_result_t result = nx_deadline_finite_after(
        nx_time_now_us(), CALIBRATION_BUDGET_US, &deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    LL_ADC_Enable(adc);
    nx_arch_dsb();
    if (READ_REG(adc->CR2) != ready) {
        return NX_ERROR_IO;
    }
    /* Four nominal microseconds cover tSTAB and two ADC cycles at 4 MHz. */
    if (!nx_stm32f1_adc_power_settled() ||
        nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    SET_BIT(adc->CR2, ADC_CR2_RSTCAL);
    nx_arch_dsb();
    if (!nx_stm32f1_adc_calibration_finished(adc, ADC_CR2_RSTCAL, deadline)) {
        return NX_ERROR_TIMEOUT;
    }
    LL_ADC_StartCalibration(adc);
    nx_arch_dsb();
    if (!nx_stm32f1_adc_calibration_finished(adc, ADC_CR2_CAL, deadline)) {
        return NX_ERROR_TIMEOUT;
    }
    WRITE_REG(adc->SR, 0U);
    nx_arch_dsb();
    return READ_REG(adc->CR2) == ready && READ_REG(adc->SR) == 0U ? NX_SUCCESS
                                                                  : NX_ERROR_IO;
}

#endif
