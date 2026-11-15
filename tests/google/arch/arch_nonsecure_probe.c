/**
 * \file            arch_nonsecure_probe.c
 * \brief           Exercise invalid C enum inputs without C++ enum conversion
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "arch_nonsecure_model.h"

nx_arch_result_t nx_arch_nonsecure_test_raw(const void* address, size_t bytes,
                                            unsigned access) {
    /* C permits these representable integer values in this C API's enum. */
    return nx_arch_nonsecure_check(address, bytes,
                                   (nx_arch_nonsecure_access_t)access);
}
