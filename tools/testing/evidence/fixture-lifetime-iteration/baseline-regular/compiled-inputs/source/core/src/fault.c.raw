/**
 * \file            fault.c
 * \brief           Stateless weak platform fault observation boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"

/** \brief No kernel, device, allocation or retained diagnostic is implicit. */
__attribute__((weak)) void nx_fault_observe(nx_fault_domain_t domain,
                                            nx_result_t reason,
                                            const char* file, unsigned line) {
    (void)domain;
    (void)reason;
    (void)file;
    (void)line;
}
