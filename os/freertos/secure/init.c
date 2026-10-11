/**
 * \file            init.c
 * \brief           Reviewed split-world exception and FP setup with saved masks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "FreeRTOSConfig.h"
#include "private/hardware.h"
#include "secure_init.h"

NX_SECURE_GATEWAY void SecureInit_DePrioritizeNSExceptions(void) {
    if (!NX_SECURE_NONSECURE_CALLER() || nx_arch_exception_number() != 11u) {
        return;
    }
    nx_secure_masks_t saved = nx_secure_masks_save();
    uint32_t aircr = (uint32_t)nx_secure_hw_aircr();
    aircr &= ~((0xffffu << 16) | (1u << 14));
    aircr |= (0x05fau << 16) | (1u << 14);
    nx_secure_hw_set_aircr(aircr);
    nx_arch_dsb();
    nx_arch_isb();
    nx_secure_masks_restore(saved);
}

NX_SECURE_GATEWAY void SecureInit_EnableNSFPUAccess(void) {
#if configENABLE_FPU || configENABLE_MVE
    if (!NX_SECURE_NONSECURE_CALLER() || nx_arch_exception_number() != 11u) {
        return;
    }
    nx_secure_masks_t saved = nx_secure_masks_save();
    uint32_t access = (uint32_t)nx_secure_hw_nsacr();
    nx_secure_hw_set_nsacr(access | (1u << 10) | (1u << 11));
    uint32_t control = (uint32_t)nx_secure_hw_fpccr();
    /* Share the reviewed FP/MVE bank with NS tasks, but mark the bank Secure
     * on exceptions so callee-saved contents cannot leak across worlds. */
    control = (control & ~(1u << 29)) | (1u << 26);
    nx_secure_hw_set_fpccr(control);
    nx_arch_dsb();
    nx_arch_isb();
    nx_secure_masks_restore(saved);
#endif
}
