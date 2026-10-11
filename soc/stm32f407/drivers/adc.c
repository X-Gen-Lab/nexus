/**
 * \file            adc.c
 * \brief           Fixed ADC1 software shots and low-rate scans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"

#ifndef NX_STM32_IO_POLL
#define NX_STM32_IO_POLL(kind, port) ((void)(kind), (void)(port))
#endif

/** \brief Initialize only independent ADC, no trigger, DMA or hidden scan task.
 */
nx_result_t nx_stm32_adc_initialize(nx_stm32_adc_state_t* port) {
    if (port == NULL || port->registers == NULL || port->common == NULL ||
        port->channels == NULL || port->sample_times == NULL ||
        port->channel_count == 0U || port->channel_count > 16U ||
        port->reference_mv == 0U || port->active) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0U; i < port->channel_count; ++i) {
        if (port->channels[i] > 15U || port->sample_times[i] > 7U) {
            return NX_ERROR_UNSUPPORTED;
        }
    }
    port->registers->CR2 = 0U;
    port->registers->CR1 = 0U;
    port->registers->SQR1 = 0U;
    port->registers->SQR2 = 0U;
    port->registers->SQR3 = 0U;
    port->registers->SR = 0U;
    port->common->CCR =
        (port->common->CCR & ~(uint32_t)ADC_CCR_ADCPRE) | ADC_CCR_ADCPRE_0;
    port->initialized = true;
    return NX_SUCCESS;
}

/** \brief Publish raw-count metadata and the nominal, uncalibrated reference.
 */
nx_result_t nx_stm32_adc_info(const void* context, nx_adc_info_t* info) {
    const nx_stm32_adc_state_t* port = context;
    if (port == NULL || !port->initialized || info == NULL) {
        return NX_ERROR_INVALID;
    }
    *info = (nx_adc_info_t){.resolution_bits = 12U,
                            .reference_mv = port->reference_mv,
                            .channel_count = port->channel_count};
    return NX_SUCCESS;
}

/** \brief Convert sequential software shots in the declared low-rate order. */
nx_result_t nx_stm32_adc_sample(void* context, uint16_t* samples,
                                size_t capacity, nx_time_us_t deadline,
                                size_t* count) {
    nx_stm32_adc_state_t* port = context;
    if (port == NULL || !port->initialized || samples == NULL ||
        count == NULL || capacity < port->channel_count) {
        return NX_ERROR_INVALID;
    }
    *count = 0U;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->active = true;
    port->registers->CR2 = ADC_CR2_ADON;
    nx_time_us_t stable = nx_deadline_after(nx_time_now_us(), 3U);
    while (!nx_deadline_expired(stable, nx_time_now_us())) {
        NX_STM32_IO_POLL(5U, port);
    }
    nx_result_t result = NX_SUCCESS;
    for (size_t i = 0U; i < port->channel_count; ++i) {
        uint32_t channel = port->channels[i];
        volatile uint32_t* sample =
            channel < 10U ? &port->registers->SMPR2 : &port->registers->SMPR1;
        uint32_t shift = channel < 10U ? channel * 3U : (channel - 10U) * 3U;
        *sample = (*sample & ~(7U << shift)) |
                  ((uint32_t)port->sample_times[i] << shift);
        port->registers->SQR3 = channel;
        port->registers->SR = 0U;
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        port->registers->CR2 |= ADC_CR2_SWSTART;
        for (;;) {
            NX_STM32_IO_POLL(5U, port);
            if ((port->registers->SR & ADC_SR_OVR) != 0U) {
                result = NX_ERROR_IO;
                break;
            }
            if (nx_deadline_expired(deadline, nx_time_now_us())) {
                result = NX_ERROR_TIMEOUT;
                break;
            }
            if ((port->registers->SR & ADC_SR_EOC) != 0U) {
                samples[(*count)++] = (uint16_t)(port->registers->DR & 0xFFFU);
                break;
            }
        }
        if (result != NX_SUCCESS) {
            break;
        }
    }
    port->registers->CR2 = 0U;
    nx_arch_dsb();
    port->registers->SR = 0U;
    port->active = false;
    if (result == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    return result;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_adc_ops_t nx_stm32_adc_ops = {
    .info = nx_stm32_adc_info,
    .sample = nx_stm32_adc_sample,
};
