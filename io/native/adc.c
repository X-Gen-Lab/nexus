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
#include "provider.h"

static nx_native_adc_state_t s_adc;
const nx_adc_port_t g_nx_native_adc = {&nx_native_adc_ops, &s_adc};
const nx_adc_port_t* const nx_native_adc = &g_nx_native_adc;

/** \brief Fix reviewed sequence length, sample format and nominal reference. */
nx_result_t nx_native_adc_configure_instance(nx_native_adc_state_t* port,
                                             const uint16_t* samples,
                                             size_t count,
                                             uint8_t resolution_bits,
                                             uint32_t reference_mv) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (samples == NULL || count == 0 || resolution_bits == 0 ||
        resolution_bits > 16 || reference_mv == 0) {
        return NX_ERROR_INVALID;
    }
    if (port->active || port->stream != NULL) {
        return NX_ERROR_BUSY;
    }
    *port =
        (nx_native_adc_state_t){.samples = samples,
                                .info = {resolution_bits, reference_mv, count},
                                .fail_after = SIZE_MAX};
    return NX_SUCCESS;
}

/** \brief Operate on the explicit default fixture only. */
nx_result_t nx_native_adc_configure(const uint16_t* samples, size_t count,
                                    uint8_t resolution_bits,
                                    uint32_t reference_mv) {
    return nx_native_adc_configure_instance(&s_adc, samples, count,
                                            resolution_bits, reference_mv);
}

/** \brief Release stream loans before withdrawing finite and stream admission.
 */
nx_result_t nx_native_adc_stop_instance(nx_native_adc_state_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_native_adc_stream_stop(port);
    if (result == NX_SUCCESS) {
        port->samples = NULL;
    }
    return result;
}

/** \brief Report raw-count facts without asserting calibrated physical voltage.
 */
static nx_result_t native_adc_info(const void* context, nx_adc_info_t* info) {
    const nx_native_adc_state_t* port = context;
    if (port == NULL || info == NULL || port->samples == NULL) {
        return NX_ERROR_INVALID;
    }
    *info = port->info;
    return NX_SUCCESS;
}

/** \brief Return only complete valid samples and end all buffer accesses. */
static nx_result_t native_adc_sample(void* context, uint16_t* samples,
                                     size_t capacity, nx_time_us_t deadline,
                                     size_t* count) {
    nx_native_adc_state_t* port = context;
    if (port == NULL || samples == NULL || count == NULL ||
        port->samples == NULL || capacity < port->info.channel_count) {
        return NX_ERROR_INVALID;
    }
    *count = 0;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active || port->stream != NULL) {
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
void nx_native_adc_fault_instance(nx_native_adc_state_t* port,
                                  size_t sample_count) {
    if (port == NULL) {
        return;
    }
    port->fail_after = sample_count;
}

/** \brief Operate on the explicit default fixture only. */
void nx_native_adc_fault(size_t sample_count) {
    nx_native_adc_fault_instance(&s_adc, sample_count);
}

/** \brief One readonly operation table is shared by every Native instance. */
const nx_adc_ops_t nx_native_adc_ops = {
    .info = native_adc_info,
    .sample = native_adc_sample,
    .stream_start = nx_native_adc_stream_start,
    .stream_stop = nx_native_adc_stream_stop,
    .stream_service = nx_native_adc_stream_service,
};

/** \brief Select exactly one Native face without affecting default fixtures. */
nx_result_t nx_native_adc_model_configure(const nx_adc_port_t* binding,
                                          const uint16_t* samples, size_t count,
                                          uint8_t resolution_bits,
                                          uint32_t reference_mv) {
    if (binding == NULL || binding->ops != &nx_native_adc_ops ||
        binding->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_native_adc_configure_instance(binding->context, samples, count,
                                            resolution_bits, reference_mv);
}

/** \brief Select exactly one Native face without affecting default fixtures. */
void nx_native_adc_model_fault(const nx_adc_port_t* binding,
                               size_t sample_count) {
    if (binding == NULL || binding->ops != &nx_native_adc_ops ||
        binding->context == NULL) {
        return;
    }
    nx_native_adc_fault_instance(binding->context, sample_count);
}
