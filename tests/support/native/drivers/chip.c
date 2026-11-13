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
/** \brief A host model has no physical UID, including a size-only query. */
nx_result_t nx_chip_uid_read(void* output, size_t capacity, size_t* bytes) {
    uintptr_t address = (uintptr_t)output;
    uintptr_t length = (uintptr_t)bytes;
    if (bytes == NULL || length % _Alignof(size_t) != 0U ||
        sizeof(*bytes) > UINTPTR_MAX - length ||
        (output == NULL && capacity != 0U) ||
        capacity > UINTPTR_MAX - address ||
        (output != NULL && address < length + sizeof(*bytes) &&
         length < address + capacity)) {
        return NX_ERROR_INVALID;
    }
    return NX_ERROR_UNSUPPORTED;
}
