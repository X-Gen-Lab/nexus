/**
 * \file            i2c_instruction_barrier_model.c
 * \brief           External CPU barrier for isolated I2C GPIO dependencies
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"

/** \brief No physical instruction pipeline exists in this register fixture. */
void nx_arch_isb(void) {
}
