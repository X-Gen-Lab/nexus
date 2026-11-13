/**
 * \file            chip.c
 * \brief           Official UID and IDCODE reads with exact Flash field width
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/chip.h"
#include "nexus/arch/arch.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_utils.h"

/** \brief Output cannot overwrite the physical observations being collected. */
static bool overlaps(uintptr_t output, size_t capacity, uintptr_t registers,
                     size_t bytes) {
    return output < registers + bytes && registers < output + capacity;
}
/** \brief Reject known MMIO aliases before any write or hardware observation.
 */
static bool output_valid(const void* output, size_t capacity,
                         size_t alignment) {
    uintptr_t address = (uintptr_t)output;
    return output != NULL && address % alignment == 0U &&
           capacity <= UINTPTR_MAX - address &&
           !overlaps(address, capacity, UID_BASE, 3U * sizeof(uint32_t)) &&
           !overlaps(address, capacity, FLASHSIZE_BASE, sizeof(uint16_t)) &&
           !overlaps(address, capacity, (uintptr_t)DBGMCU, sizeof(*DBGMCU));
}
/** \brief No initialization, cached identity or clock side effect is required.
 */
nx_result_t nx_chip_info_read(nx_chip_info_t* info) {
    if (!output_valid(info, sizeof(*info), _Alignof(nx_chip_info_t))) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    /* The pinned LL helper uses a uint32_t lvalue at F407's halfword-aligned
     * factory field. A documented halfword load also works with UNALIGN_TRP. */
    uint16_t flash_size = *(const volatile uint16_t*)FLASHSIZE_BASE;
    *info = (nx_chip_info_t){
        .uid = {LL_GetUID_Word0(), LL_GetUID_Word1(), LL_GetUID_Word2()},
        .device_id = (uint16_t)LL_DBGMCU_GetDeviceID(),
        .revision_id = (uint16_t)LL_DBGMCU_GetRevisionID(),
        .flash_size_kib = flash_size};
    return NX_SUCCESS;
}
/** \brief Read official UID words once, then serialize into opaque bytes. */
nx_result_t nx_chip_uid_read(void* output, size_t capacity, size_t* bytes) {
    if (!output_valid(bytes, sizeof(*bytes), _Alignof(size_t)) ||
        (output == NULL && capacity != 0U) ||
        (output != NULL && (!output_valid(output, capacity, 1U) ||
                            overlaps((uintptr_t)output, capacity,
                                     (uintptr_t)bytes, sizeof(*bytes))))) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (output == NULL) {
        *bytes = 3U * sizeof(uint32_t);
        return NX_SUCCESS;
    }
    if (capacity < 3U * sizeof(uint32_t)) {
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
}
