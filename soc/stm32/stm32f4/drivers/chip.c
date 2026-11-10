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
static bool overlaps(uintptr_t output, uintptr_t registers, size_t bytes) {
    return output < registers + bytes &&
           registers < output + sizeof(nx_chip_info_t);
}
/** \brief No initialization, cached identity or clock side effect is required.
 */
nx_result_t nx_chip_info_read(nx_chip_info_t* info) {
    uintptr_t address = (uintptr_t)info;
    if (info == NULL || address % _Alignof(nx_chip_info_t) != 0U ||
        sizeof(*info) > UINTPTR_MAX - address ||
        overlaps(address, UID_BASE, 3U * sizeof(uint32_t)) ||
        overlaps(address, FLASHSIZE_BASE, sizeof(uint16_t)) ||
        overlaps(address, (uintptr_t)DBGMCU, sizeof(*DBGMCU))) {
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
