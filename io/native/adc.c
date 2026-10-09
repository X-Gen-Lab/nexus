/**
 * \file            adc.c
 *
 * \brief           Fixed low-rate scan valid-prefix and abort-settlement
 *                  model.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/io/native/model.h"

struct nx_adc_port {
    const uint16_t* samples;
    nx_adc_info_t info;
    size_t fail_after;
    bool active;
};
nx_adc_port_t g_nx_native_adc;
nx_adc_port_t* const nx_native_adc = &g_nx_native_adc;

/** \brief Fix reviewed sequence length, sample format and nominal reference. */
nx_result_t nx_native_adc_configure(const uint16_t* samples, size_t count,
                                    uint8_t resolution_bits,
                                    uint32_t reference_mv) {
    if (samples == NULL || count == 0 || resolution_bits == 0 ||
        resolution_bits > 16 || reference_mv == 0) {
        return NX_ERROR_INVALID;
    }
    g_nx_native_adc =
        (nx_adc_port_t){.samples = samples,
                        .info = {resolution_bits, reference_mv, count},
                        .fail_after = SIZE_MAX};
    return NX_SUCCESS;
}

/** \brief Report raw-count facts without asserting calibrated physical voltage.
 */
nx_result_t nx_adc_port_info(const nx_adc_port_t* port, nx_adc_info_t* info) {
    if (port == NULL || info == NULL || port->samples == NULL) {
        return NX_ERROR_INVALID;
    }
    *info = port->info;
    return NX_SUCCESS;
}

/** \brief Return only complete valid samples and end all buffer accesses. */
nx_result_t nx_adc_port_sample(nx_adc_port_t* port, uint16_t* samples,
                               size_t capacity, nx_time_us_t deadline,
                               size_t* count) {
    if (port == NULL || samples == NULL || count == NULL ||
        port->samples == NULL || capacity < port->info.channel_count) {
        return NX_ERROR_INVALID;
    }
    *count = 0;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    port->active = true;
    nx_result_t result = NX_SUCCESS;
    uint32_t maximum = (1u << port->info.resolution_bits) - 1u;
    for (size_t i = 0; i < port->info.channel_count; i++) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        if (i == port->fail_after || port->samples[i] > maximum) {
            result = NX_ERROR_IO;
            break;
        }
        samples[i] = port->samples[i];
        (*count)++;
        (void)nx_native_clock_advance(1);
    }
    port->active = false;
    return result;
}

/** \brief Inject a finite sample-prefix failure for output validity assertions.
 */
void nx_native_adc_fault(size_t sample_count) {
    g_nx_native_adc.fail_after = sample_count;
}
