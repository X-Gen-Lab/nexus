/**
 * \file            stm32_identity.h
 * \brief           Private STM32 documented debug identity register source
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_STM32_IDENTITY_H
#define NEXUS_PRIVATE_STM32_IDENTITY_H

#include "nexus/core/silicon.h"

/**
 * \brief           Explicit MMIO spans for the reviewed STM32 register layout.
 * \note            Caller supplies its exact SDK IDCODE and Flash-size address
 *                  in legal cold privileged Thread/security context. No clock,
 *                  debug authorization or register accessibility is inferred.
 *                  This layout must not be inferred for another vendor merely
 *                  because a register is also called DBG_ID.
 */
typedef struct {
    volatile const uint32_t* idcode;
    volatile const uint16_t* flash_kib;
} nx_stm32_identity_registers_t;

/** \brief Checked distinct spans; wraps reject before volatile access. */
static inline bool nx_stm32_identity_overlaps(const void* first, size_t bytes,
                                              uintptr_t second,
                                              size_t second_bytes) {
    uintptr_t begin = (uintptr_t)first;
    return bytes > UINTPTR_MAX - begin || second_bytes > UINTPTR_MAX - second ||
           (begin < second + second_bytes && second < begin + bytes);
}

/**
 * \brief           Read each register once without writes or reference loans.
 * \param[in]       context: Stable exact nx_stm32_identity_registers_t.
 * \param[out]      out: Distinct copied identity, unchanged on error.
 * \return          SUCCESS, INVALID for missing/aliased spans, IO for zero or
 *                  erased/unavailable Flash-size word. No SDK fallback density.
 * \note            IDCODE DEV_ID is bits11:0; REV_ID is bits31:16. Neither
 *                  identifies exact package or orderable part. This adapter
 *                  does not read before DATA/BSS initialization, authorize
 *                  privilege/security, or catch an inaccessible MMIO fault.
 */
static inline nx_result_t nx_stm32_identity_read(const void* context,
                                                 nx_silicon_identity_t* out) {
    const nx_stm32_identity_registers_t* registers =
        (const nx_stm32_identity_registers_t*)context;
    if (registers == NULL || out == NULL || registers->idcode == NULL ||
        registers->flash_kib == NULL ||
        nx_stm32_identity_overlaps(out, sizeof(*out), (uintptr_t)registers,
                                   sizeof(*registers)) ||
        nx_stm32_identity_overlaps(out, sizeof(*out),
                                   (uintptr_t)registers->idcode,
                                   sizeof(*registers->idcode)) ||
        nx_stm32_identity_overlaps(out, sizeof(*out),
                                   (uintptr_t)registers->flash_kib,
                                   sizeof(*registers->flash_kib))) {
        return NX_ERROR_INVALID;
    }
    uint32_t idcode = *registers->idcode;
    uint32_t kib = *registers->flash_kib;
    if (kib == 0U || kib == UINT16_MAX) {
        return NX_ERROR_IO;
    }
    nx_silicon_identity_t identity = {idcode & UINT32_C(0xfff), idcode >> 16U,
                                      kib * 1024U, true};
    *out = identity;
    return NX_SUCCESS;
}
#endif
