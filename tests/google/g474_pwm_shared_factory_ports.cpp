/**
 * \file            g474_pwm_shared_factory_ports.cpp
 * \brief           Exact GPIO field faults around the maintained G474 model
 * \author          Nexus Team
 * \version         0.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pwm_shared_factory_model.h"

nx_g474_shared_factory_faults_t g_shared_factory_faults;

extern "C" uint32_t nx_g474_shared_factory_read(volatile uint32_t* reg) {
    return nx_g474_shared_read(reg);
}

extern "C" void nx_g474_shared_factory_write(volatile uint32_t* reg,
                                             uint32_t value) {
    auto& fault = g_shared_factory_faults;
    if (reg == &GPIOA->MODER) {
        const uint32_t held = fault.refused_mode_fields;
        if ((value & held) != (*reg & held)) {
            ++fault.refused_mode_writes;
            value = (value & ~held) | (*reg & held);
        }
        const uint32_t pads = (3U << 12U) | (3U << 14U);
        if (fault.refuse_snapshot_mode &&
            (value & pads) == (fault.snapshot_mode & pads) &&
            (value & pads) != (*reg & pads)) {
            ++fault.refused_snapshot_writes;
            value = (value & ~pads) | (*reg & pads);
        }
    }
    if (reg == &GPIOA->BSRR && (value & fault.refused_latch_set) != 0U) {
        ++fault.refused_latch_writes;
        value &= ~fault.refused_latch_set;
    }
    nx_g474_shared_write(reg, value);
}
