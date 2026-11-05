/**
 * \file            adc_stream_profile.c
 * \brief           Checked ADC-only cold stream admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-05
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/io/stream_profile.h"
#include "stream_profile_storage.h"

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
    nx_result_t result = nx_stream_profile_budget_validate(&profile->budget);
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
    result = nx_stream_profile_pool_validate(&profile->budget, profile,
                                             sizeof(*profile), profile->port,
                                             sizeof(*profile->port), stream);
    return result != NX_SUCCESS
               ? result
               : nx_adc_port_stream_start(profile->port, stream, trigger_hz);
}
