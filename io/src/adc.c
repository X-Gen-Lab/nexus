/**
 * \file            adc.c
 *
 * \brief           Checked typed dispatch through shared read-only provider
 *                  methods
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/adc.h"

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_adc_port_info(const nx_adc_port_t* port, nx_adc_info_t* info) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->info == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->info(port->context, info);
}

/** \brief Dispatch using this face's exact instance state. */
nx_result_t nx_adc_port_sample(const nx_adc_port_t* port, uint16_t* samples,
                               size_t capacity, nx_time_us_t deadline,
                               size_t* count) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->sample == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->sample(port->context, samples, capacity, deadline, count);
}

/** \brief Optional block-mode dispatch never implies an allocation or worker.
 */
nx_result_t nx_adc_port_stream_start(const nx_adc_port_t* port,
                                     nx_stream_t* stream, uint32_t trigger_hz) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->stream_start == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->stream_start(port->context, stream, trigger_hz);
}

/** \brief Optional block-mode dispatch never implies an allocation or worker.
 */
nx_result_t nx_adc_port_stream_stop(const nx_adc_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->ops->stream_stop == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return port->ops->stream_stop(port->context);
}

/** \brief Explicit executor service resumes only a proved free block. */
nx_result_t nx_adc_port_stream_service(const nx_adc_port_t* port) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stream_service == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->stream_service(port->context);
}

/** \brief Optional hints require the actual provider's IRQ ceiling proof. */
nx_result_t nx_adc_port_attach_wake(const nx_adc_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling) {
    if (port == NULL || port->ops == NULL || port->context == NULL) {
        return NX_ERROR_INVALID;
    }
    return port->ops->attach_wake == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->attach_wake(port->context, wake, syscall_ceiling);
}
