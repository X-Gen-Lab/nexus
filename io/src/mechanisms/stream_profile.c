/**
 * \file            stream_profile.c
 * \brief           Checked cold stream admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/stream_profile.h"
#include "nexus/arch/arch.h"

/** \brief Compare checked integer extents without unrelated pointer order. */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b ||
           (a < b + second_bytes && b < a + first_bytes);
}

/** \brief The reviewed double-buffer owner also needs a free replacement. */
static nx_result_t budget_check(const nx_stream_budget_t* budget) {
    if (budget->hardware_targets != 2U || budget->spare_blocks != 1U) {
        return NX_ERROR_INVALID;
    }
    nx_stream_budget_result_t calculated;
    nx_result_t result = nx_stream_budget_calculate(budget, &calculated);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!calculated.capacity_sufficient) {
        return NX_ERROR_EXHAUSTED;
    }
    return calculated.target_latency_sufficient ? NX_SUCCESS
                                                : NX_ERROR_UNSUPPORTED;
}

/** \brief Check the actual caller pool, never reset loans to fit a profile. */
static nx_result_t pool_check(const nx_stream_budget_t* budget,
                              const void* profile, size_t profile_bytes,
                              const void* port, size_t port_bytes,
                              const nx_stream_t* stream) {
    if (stream == NULL || stream->slots == NULL ||
        stream->count != budget->configured_blocks || stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) ||
        budget->units_per_block > SIZE_MAX / budget->unit_bytes) {
        return NX_ERROR_INVALID;
    }
    size_t metadata = stream->count * sizeof(*stream->slots);
    if (overlaps(profile, profile_bytes, stream, sizeof(*stream)) ||
        overlaps(profile, profile_bytes, stream->slots, metadata) ||
        overlaps(port, port_bytes, stream, sizeof(*stream)) ||
        overlaps(port, port_bytes, stream->slots, metadata) ||
        overlaps(stream, sizeof(*stream), stream->slots, metadata)) {
        return NX_ERROR_INVALID;
    }
    if (stream->stopping || stream->producer >= stream->count ||
        stream->consumer >= stream->count ||
        stream->outstanding > stream->count) {
        return NX_ERROR_STATE;
    }
    if (stream->outstanding != 0U) {
        return NX_ERROR_BUSY;
    }
    if (stream->epoch > UINT64_MAX - 2U) {
        return NX_ERROR_EXHAUSTED;
    }
    size_t bytes = budget->units_per_block * budget->unit_bytes;
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->state != NX_STREAM_SLOT_FREE) {
            return NX_ERROR_BUSY;
        }
        if (slot->data == NULL || slot->capacity != bytes ||
            overlaps(slot->data, bytes, profile, profile_bytes) ||
            overlaps(slot->data, bytes, port, port_bytes) ||
            overlaps(slot->data, bytes, stream, sizeof(*stream)) ||
            overlaps(slot->data, bytes, stream->slots, metadata)) {
            return NX_ERROR_INVALID;
        }
        for (size_t earlier = 0U; earlier < i; ++earlier) {
            if (overlaps(slot->data, bytes, stream->slots[earlier].data,
                         bytes)) {
                return NX_ERROR_INVALID;
            }
        }
    }
    return NX_SUCCESS;
}

/** \brief Validate first, then leave all admission/loan ownership to the port.
 */
nx_result_t
nx_uart_stream_profile_start(const nx_uart_stream_profile_t* profile,
                             nx_stream_t* stream) {
    if (profile == NULL || profile->port == NULL ||
        profile->port->ops == NULL || profile->port->context == NULL ||
        profile->budget.unit_bytes != 1U ||
        profile->budget.units_per_block > UINT16_MAX) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_result_t result = budget_check(&profile->budget);
    if (result == NX_SUCCESS) {
        result = pool_check(&profile->budget, profile, sizeof(*profile),
                            profile->port, sizeof(*profile->port), stream);
    }
    return result != NX_SUCCESS ? result
                                : nx_uart_port_rx_start(profile->port, stream);
}

/** \brief Actual selected ADC scan width must agree with the readonly budget.
 */
nx_result_t nx_adc_stream_profile_start(const nx_adc_stream_profile_t* profile,
                                        nx_stream_t* stream,
                                        uint32_t trigger_hz) {
    if (profile == NULL || profile->port == NULL ||
        profile->port->ops == NULL || profile->port->context == NULL ||
        trigger_hz == 0U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_result_t result = budget_check(&profile->budget);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (trigger_hz > profile->budget.units_per_second) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_adc_info_t info;
    result = nx_adc_port_info(profile->port, &info);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (info.channel_count == 0U ||
        info.channel_count > SIZE_MAX / sizeof(uint16_t) ||
        profile->budget.unit_bytes != info.channel_count * sizeof(uint16_t) ||
        profile->budget.units_per_block > UINT16_MAX / info.channel_count) {
        return NX_ERROR_INVALID;
    }
    result = pool_check(&profile->budget, profile, sizeof(*profile),
                        profile->port, sizeof(*profile->port), stream);
    return result != NX_SUCCESS
               ? result
               : nx_adc_port_stream_start(profile->port, stream, trigger_hz);
}
