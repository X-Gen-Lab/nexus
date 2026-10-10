/**
 * \file            stream.c
 *
 * \brief           Concrete UART IDLE and ADC trigger model block producers.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "provider.h"
#include <string.h>

/** \brief Validate exact initialized storage before retaining a stream. */
static nx_result_t attach(nx_stream_t* stream, size_t unit) {
    if (stream == NULL || stream->slots == NULL || stream->count == 0 ||
        stream->count > SIZE_MAX / sizeof(nx_stream_slot_t)) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->data == NULL || slot->capacity < unit ||
            slot->capacity % unit != 0 ||
            (unit != 1 && (uintptr_t)slot->data % _Alignof(uint16_t) != 0)) {
            return NX_ERROR_INVALID;
        }
        if (slot->state != NX_STREAM_SLOT_FREE) {
            return NX_ERROR_BUSY;
        }
    }
    stream->stopping = false;
    return NX_SUCCESS;
}

/** \brief Retain aggregate loss without overwriting any held block. */
static void loss(nx_stream_t* stream) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (stream->losses != UINT32_MAX) {
        ++stream->losses;
    }
    nx_arch_irq_restore(saved);
}

/** \brief Switch an empty selected byte/event model to exact block storage. */
nx_result_t nx_native_uart_rx_start(void* context, nx_stream_t* stream) {
    nx_native_uart_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!port->opened || port->stopping) {
        return NX_ERROR_STATE;
    }
    if (port->rx_stream != NULL || port->rx_count != 0 || port->loss) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = attach(stream, 1);
    if (result == NX_SUCCESS) {
        port->rx_stream = stream;
        port->rx_filling = false;
        port->rx_producing = false;
        port->rx_block_length = 0;
    }
    return result;
}

/** \brief Publish a partial frame only after the modeled writer is detached. */
static nx_result_t idle(nx_native_uart_state_t* port) {
    if (!port->rx_filling || port->rx_block_length == 0) {
        return NX_ERROR_EMPTY;
    }
    nx_result_t result =
        nx_stream_publish(port->rx_stream, &port->rx_fill,
                          port->rx_block_length, NX_STREAM_BOUNDARY_IDLE);
    if (result == NX_SUCCESS) {
        port->rx_filling = false;
        port->rx_block_length = 0;
    }
    return result;
}

/** \brief One serialized RX fact writes only its current producer loan. */
nx_result_t nx_native_uart_block_receive(nx_native_uart_state_t* port,
                                         uint8_t byte, uint32_t flags) {
    if (port == NULL || port->rx_stream == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!port->opened || port->stopping || port->rx_stream->stopping) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (port->rx_producing) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    port->rx_producing = true;
    const nx_irq_wake_t* wake = port->wake;
    nx_arch_irq_restore(saved);
    nx_result_t result = NX_SUCCESS;
    if (flags != 0) {
        loss(port->rx_stream);
    }
    if ((flags & NX_UART_EVENT_NO_BYTE) == 0) {
        if (!port->rx_filling) {
            result = nx_stream_reserve(port->rx_stream, &port->rx_fill);
            if (result == NX_SUCCESS) {
                port->rx_filling = true;
                port->rx_block_length = 0;
            }
        }
        if (result == NX_SUCCESS) {
            port->rx_fill.data[port->rx_block_length++] = byte;
            if (port->rx_block_length == port->rx_fill.capacity) {
                result = nx_stream_publish(port->rx_stream, &port->rx_fill,
                                           port->rx_block_length, 0);
                if (result == NX_SUCCESS) {
                    port->rx_filling = false;
                    port->rx_block_length = 0;
                }
            }
        }
    }
    saved = nx_arch_irq_save();
    port->rx_producing = false;
    nx_arch_irq_restore(saved);
    nx_irq_wake_signal(wake);
    return result;
}

/** \brief Inject an explicit IDLE fact; no waveform or clock-rate claim. */
nx_result_t nx_native_uart_model_idle(const nx_uart_port_t* binding) {
    if (binding == NULL || binding->ops != &nx_native_uart_ops ||
        binding->context == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_native_uart_state_t* port = binding->context;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (port->rx_stream == NULL || port->rx_stream->stopping || !port->opened ||
        port->stopping) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (port->rx_producing) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    port->rx_producing = true;
    const nx_irq_wake_t* wake = port->wake;
    nx_arch_irq_restore(saved);
    nx_result_t result = idle(port);
    saved = nx_arch_irq_save();
    port->rx_producing = false;
    nx_arch_irq_restore(saved);
    if (result == NX_SUCCESS) {
        nx_irq_wake_signal(wake);
    }
    return result;
}

/** \brief Detach the producer, publish its final prefix and retain consumers.
 */
nx_result_t nx_native_uart_rx_stop(void* context) {
    nx_native_uart_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->rx_stream == NULL) {
        return NX_SUCCESS;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    port->rx_stream->stopping = true;
    bool producing = port->rx_producing;
    nx_arch_irq_restore(saved);
    if (producing) {
        return NX_ERROR_BUSY;
    }
    if (port->rx_filling) {
        nx_result_t result = idle(port);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    nx_result_t result = nx_stream_stop(port->rx_stream);
    if (result == NX_SUCCESS) {
        port->rx_stream = NULL;
    }
    return result;
}

/** \brief Bind whole interleaved raw-count scan blocks to an explicit trigger.
 */
nx_result_t nx_native_adc_stream_start(void* context, nx_stream_t* stream,
                                       uint32_t trigger_hz) {
    nx_native_adc_state_t* port = context;
    if (port == NULL || port->samples == NULL || trigger_hz == 0 ||
        port->info.channel_count > SIZE_MAX / sizeof(uint16_t)) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active || port->stream != NULL) {
        return NX_ERROR_BUSY;
    }
    size_t bytes = port->info.channel_count * sizeof(uint16_t);
    nx_result_t result = attach(stream, bytes);
    if (result == NX_SUCCESS) {
        port->stream = stream;
        port->trigger_hz = trigger_hz;
    }
    return result;
}

/** \brief A modeled trigger publishes one complete scan after writer detach. */
nx_result_t nx_native_adc_model_trigger(const nx_adc_port_t* binding) {
    if (binding == NULL || binding->ops != &nx_native_adc_ops ||
        binding->context == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_native_adc_state_t* port = binding->context;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (port->stream == NULL || port->stream->stopping) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (port->active) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    port->active = true;
    nx_arch_irq_restore(saved);
    nx_stream_fill_t fill;
    nx_result_t result = nx_stream_reserve(port->stream, &fill);
    if (result == NX_SUCCESS) {
        uint32_t maximum = (1u << port->info.resolution_bits) - 1u;
        size_t length = port->info.channel_count * sizeof(uint16_t);
        for (size_t i = 0; i < port->info.channel_count; ++i) {
            if (i == port->fail_after || port->samples[i] > maximum) {
                result = NX_ERROR_IO;
                break;
            }
            memcpy(&fill.data[i * sizeof(uint16_t)], &port->samples[i],
                   sizeof(uint16_t));
        }
        if (result == NX_SUCCESS) {
            result = nx_stream_publish(port->stream, &fill, length,
                                       NX_STREAM_BOUNDARY_TRIGGER);
        } else {
            (void)nx_stream_abort(port->stream, &fill, true);
            loss(port->stream);
        }
    }
    saved = nx_arch_irq_save();
    port->active = false;
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Stop trigger admission and keep every consumer block until release.
 */
nx_result_t nx_native_adc_stream_stop(void* context) {
    nx_native_adc_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->stream == NULL) {
        return NX_SUCCESS;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    port->stream->stopping = true;
    bool active = port->active;
    nx_arch_irq_restore(saved);
    if (active) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_stream_stop(port->stream);
    if (result == NX_SUCCESS) {
        port->stream = NULL;
        port->trigger_hz = 0;
    }
    return result;
}

/** \brief Report whether a manual trigger can use its next exact free block. */
nx_result_t nx_native_adc_stream_service(void* context) {
    nx_native_adc_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if (port->stream != NULL && !port->stream->stopping) {
        result = !port->active &&
                         port->stream->slots[port->stream->producer].state ==
                             NX_STREAM_SLOT_FREE
                     ? NX_SUCCESS
                     : NX_ERROR_BUSY;
    }
    nx_arch_irq_restore(saved);
    return result;
}
