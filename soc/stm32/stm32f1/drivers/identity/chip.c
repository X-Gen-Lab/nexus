/**
 * \file            chip.c
 * \brief           Public F103 UID without debug-only device metadata.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/chip.h"
#include "dispatch_checks.h"
#include "nexus/arch/arch.h"
#include "stm32f1xx_ll_utils.h"

#define UID_BYTES (3U * sizeof(uint32_t))

/** \brief Reject known electronic signature aliases before publication. */
static bool output_valid(const void* output, size_t capacity,
                         size_t alignment) {
    return nx_io_dispatch_span(output, capacity, alignment) &&
           (capacity == 0U ||
            (!nx_io_dispatch_overlap(output, capacity, (void*)UID_BASE,
                                     UID_BYTES) &&
             !nx_io_dispatch_overlap(output, capacity, (void*)FLASHSIZE_BASE,
                                     sizeof(uint16_t)) &&
             !nx_io_dispatch_overlap(output, capacity, DBGMCU,
                                     sizeof(*DBGMCU))));
}

/** \brief ES096 §2.2.2 makes complete physical diagnostics unavailable. */
nx_result_t nx_chip_info_read(nx_chip_info_t* info) {
    if (!output_valid(info, sizeof(*info), _Alignof(nx_chip_info_t))) {
        return NX_ERROR_INVALID;
    }
    return nx_arch_is_privileged() ? NX_ERROR_UNSUPPORTED : NX_ERROR_CONTEXT;
}

/** \brief Read three official UID words and publish stable opaque bytes. */
nx_result_t nx_chip_uid_read(void* output, size_t capacity, size_t* bytes) {
    if (!output_valid(bytes, sizeof(*bytes), _Alignof(size_t)) ||
        (output == NULL && capacity != 0U) ||
        (output != NULL &&
         (!output_valid(output, capacity, 1U) ||
          (capacity != 0U &&
           nx_io_dispatch_overlap(output, capacity, bytes, sizeof(*bytes)))))) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
#if !defined(STM32F103xB)
    return NX_ERROR_UNSUPPORTED;
#else
    if (output == NULL) {
        *bytes = UID_BYTES;
        return NX_SUCCESS;
    }
    if (capacity < UID_BYTES) {
        return NX_ERROR_OVERFLOW;
    }
    const uint32_t words[] = {LL_GetUID_Word0(), LL_GetUID_Word1(),
                              LL_GetUID_Word2()};
    uint8_t* destination = output;
    for (size_t word = 0U; word < 3U; ++word) {
        for (unsigned byte = 0U; byte < 4U; ++byte) {
            destination[word * 4U + byte] =
                (uint8_t)(words[word] >> (byte * 8U));
        }
    }
    *bytes = sizeof(words);
    return NX_SUCCESS;
#endif
}
