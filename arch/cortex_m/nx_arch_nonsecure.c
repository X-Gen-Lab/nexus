/**
 * \file            nx_arch_nonsecure.c
 * \brief           Explicit NonSecure address permission observation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/arch/nonsecure.h"
#include "private/nonsecure.h"

nx_arch_result_t nx_arch_nonsecure_check(const void* address, size_t bytes,
                                         nx_arch_nonsecure_access_t access) {
#if NEXUS_ARCH_SECURITY_STATE == 1
    if (address == NULL || bytes == 0U ||
        bytes - 1U > UINTPTR_MAX - (uintptr_t)address ||
        (access != NX_ARCH_NONSECURE_READ &&
         access != NX_ARCH_NONSECURE_READWRITE)) {
        return NX_ARCH_INVALID;
    }
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U) {
        return NX_ARCH_CONTEXT;
    }
    /* The returned pointer is an observation, not a lifetime token. Never
     * substitute an address or retain a previously granted permission. */
    return nx_arch_hw_nonsecure_range(
               address, bytes, access == NX_ARCH_NONSECURE_READWRITE) == address
               ? NX_ARCH_OK
               : NX_ARCH_INVALID;
#else
    (void)address;
    (void)bytes;
    (void)access;
    return NX_ARCH_UNSUPPORTED;
#endif
}
