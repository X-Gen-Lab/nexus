/**
 * \file            dispatch.c
 * \brief           Explicit all-topology modulation dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-24
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/control/modulation.h"

nx_result_t nx_modulation_plan(const nx_modulation_config_t* config,
                               nx_motor_ab_t requested_voltage,
                               float bus_voltage_v,
                               nx_modulation_output_t* out) {
    if (nx_modulation_check(config) != NX_SUCCESS)
        return NX_ERROR_INVALID;
    return config->topology == NX_MODULATION_SINGLE_SHUNT
               ? nx_modulation_bus_plan(config, requested_voltage,
                                        bus_voltage_v, out)
               : nx_modulation_phase_plan(config, requested_voltage,
                                          bus_voltage_v, out);
}
