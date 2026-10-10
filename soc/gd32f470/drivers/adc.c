/**
 * \file            adc.c
 * \brief           GD32 ADC0 calibrated single-shot and sequential polling scan
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"

static nx_gd32_adc_state_t* s_adc;

/** \brief           Bound calibration waits without the SDK's infinite loop. */
static nx_result_t wait_clear(uint32_t mask, nx_time_us_t deadline) {
    for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
        if ((ADC_CTL1(ADC0) & mask) == 0u) {
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_IO;
}

/** \brief           Bind reviewed channels after bounded ADC stabilization. */
nx_result_t nx_gd32_adc_initialize(nx_gd32_adc_state_t* port,
                                   const uint8_t* channels, size_t count,
                                   uint32_t reference_mv,
                                   nx_time_us_t deadline) {
    if (!port || !channels || !count || count > 16u || !reference_mv) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0u; i < count; ++i) {
        if (channels[i] > 15u) {
            return NX_ERROR_INVALID;
        }
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (s_adc) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    for (size_t i = 0u; i < count; ++i) {
        unsigned channel = channels[i];
        unsigned index = channel < 8u ? 0u : channel < 10u ? 1u : 2u;
        unsigned pin = channel < 8u    ? channel
                       : channel < 10u ? channel - 8u
                                       : channel - 10u;
        RCU_AHB1EN |= UINT32_C(1) << index;
        gpio_mode_set(GPIOA + index * 0x400u, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                      UINT32_C(1) << pin);
    }
    rcu_periph_clock_enable(RCU_ADC0);
    adc_deinit();
    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);
    ADC_CTL0(ADC0) = 0u;
    ADC_CTL1(ADC0) = ADC_CTL1_ADCON;
    nx_time_us_t stable = nx_deadline_after(nx_time_now_us(), 10u);
    uint32_t polls = 0u;
    while (!nx_deadline_expired(stable, nx_time_now_us())) {
        if (++polls == 1000000u) {
            ADC_CTL1(ADC0) = 0u;
            rcu_periph_clock_disable(RCU_ADC0);
            return NX_ERROR_IO;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            ADC_CTL1(ADC0) = 0u;
            rcu_periph_clock_disable(RCU_ADC0);
            return NX_ERROR_TIMEOUT;
        }
    }
    ADC_CTL1(ADC0) |= ADC_CTL1_RSTCLB;
    nx_result_t status = wait_clear(ADC_CTL1_RSTCLB, deadline);
    if (status == NX_SUCCESS) {
        ADC_CTL1(ADC0) |= ADC_CTL1_CLB;
        status = wait_clear(ADC_CTL1_CLB, deadline);
    }
    if (status != NX_SUCCESS) {
        ADC_CTL1(ADC0) = 0u;
        rcu_periph_clock_disable(RCU_ADC0);
        return status;
    }
    *port = (nx_gd32_adc_state_t){.channels = channels,
                                  .channel_count = count,
                                  .reference_mv = reference_mv,
                                  .initialized = true};
    s_adc = port;
    return NX_SUCCESS;
}

/** \brief           Return raw count format without inventing calibrated volts.
 */
nx_result_t nx_gd32_adc_info(const void* context, nx_adc_info_t* info) {
    const nx_gd32_adc_state_t* port = context;
    if (!port || port != s_adc || !port->initialized || !info) {
        return NX_ERROR_INVALID;
    }
    *info = (nx_adc_info_t){12u, port->reference_mv, port->channel_count};
    return NX_SUCCESS;
}

/** \brief           Run one conversion per sequence element with valid prefix.
 */
nx_result_t nx_gd32_adc_sample(void* context, uint16_t* samples,
                               size_t capacity, nx_time_us_t deadline,
                               size_t* count) {
    nx_gd32_adc_state_t* port = context;
    if (!port || port != s_adc || !port->initialized || !samples || !count ||
        capacity < port->channel_count) {
        return NX_ERROR_INVALID;
    }
    *count = 0u;
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    port->active = true;
    nx_result_t status = NX_SUCCESS;
    if ((ADC_CTL1(ADC0) & ADC_CTL1_ADCON) == 0u) {
        ADC_CTL1(ADC0) = ADC_CTL1_ADCON;
        nx_time_us_t stable = nx_deadline_after(nx_time_now_us(), 10u);
        for (uint32_t polls = 0u;
             !nx_deadline_expired(stable, nx_time_now_us()); ++polls) {
            if (nx_deadline_expired(deadline, nx_time_now_us()) ||
                polls == 1000000u) {
                ADC_CTL1(ADC0) = 0u;
                port->active = false;
                return NX_ERROR_TIMEOUT;
            }
        }
    }
    for (size_t i = 0u; i < port->channel_count; ++i) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            status = NX_ERROR_TIMEOUT;
            break;
        }
        unsigned channel = port->channels[i];
        ADC_RSQ0(ADC0) = 0u;
        ADC_RSQ1(ADC0) = 0u;
        ADC_RSQ2(ADC0) = channel;
        if (channel < 10u) {
            ADC_SAMPT1(ADC0) |= 7u << (channel * 3u);
        } else {
            ADC_SAMPT0(ADC0) |= 7u << ((channel - 10u) * 3u);
        }
        ADC_STAT(ADC0) = 0u;
        ADC_CTL1(ADC0) |= ADC_CTL1_ADCON | ADC_CTL1_SWRCST;
        for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
            uint32_t flags = ADC_STAT(ADC0);
            if ((flags & ADC_STAT_ROVF) != 0u) {
                status = NX_ERROR_IO;
                break;
            }
            if (nx_deadline_expired(deadline, nx_time_now_us())) {
                status = NX_ERROR_TIMEOUT;
                break;
            }
            if ((flags & ADC_STAT_EOC) != 0u) {
                samples[(*count)++] = (uint16_t)(ADC_RDATA(ADC0) & 0xFFFu);
                break;
            }
            if (polls + 1u == 1000000u) {
                status = NX_ERROR_IO;
            }
        }
        if (status != NX_SUCCESS) {
            break;
        }
    }
    if (status != NX_SUCCESS) {
        ADC_CTL1(ADC0) = 0u;
        nx_gd32_peripheral_barrier();
        ADC_STAT(ADC0) = 0u;
    }
    port->active = false;
    if (status == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        status = NX_ERROR_TIMEOUT;
    }
    return status;
}

/** \brief           Disable conversions before releasing configuration storage.
 */
nx_result_t nx_gd32_adc_stop(nx_gd32_adc_state_t* port) {
    if (!port || port != s_adc || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    ADC_CTL1(ADC0) = 0u;
    nx_gd32_peripheral_barrier();
    ADC_STAT(ADC0) = 0u;
    rcu_periph_clock_disable(RCU_ADC0);
    port->initialized = false;
    s_adc = NULL;
    return NX_SUCCESS;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_adc_ops_t nx_gd32_adc_ops = {
    .info = nx_gd32_adc_info,
    .sample = nx_gd32_adc_sample,
};
