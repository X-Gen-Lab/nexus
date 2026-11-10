/**
 * \file            chip.c
 * \brief           Explicit host support has no physical silicon identity
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/chip.h"
/** \brief Preserve caller storage rather than inventing physical identity. */
nx_result_t nx_chip_info_read(nx_chip_info_t* info) {
    return info == NULL ? NX_ERROR_INVALID : NX_ERROR_UNSUPPORTED;
}
