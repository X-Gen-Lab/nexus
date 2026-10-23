/**
 * \file            nonsecure.h
 * \brief           Private CMSE address permission instruction boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_CORTEX_M_PRIVATE_NONSECURE_H
#define NEXUS_ARCH_CORTEX_M_PRIVATE_NONSECURE_H
#include "mechanisms.h"
#if NEXUS_ARCH_SECURITY_STATE == 1
#ifdef NEXUS_ARCH_MECHANISM_MODEL
#include "arch_nonsecure_model.h"
#else
#include <arm_cmse.h>
#endif
/** \brief The CPU checks attribution and the NonSecure unprivileged MPU. */
static inline void* nx_arch_hw_nonsecure_range(const void* address,
                                               size_t bytes, bool writable) {
    const int flags = CMSE_NONSECURE | CMSE_MPU_UNPRIV |
                      (writable ? CMSE_MPU_READWRITE : CMSE_MPU_READ);
#ifdef NEXUS_ARCH_MECHANISM_MODEL
    return nx_arch_model_nonsecure_range((void*)address, bytes, flags);
#else
    return cmse_check_address_range((void*)address, bytes, flags);
#endif
}
#endif
#endif /* NEXUS_ARCH_CORTEX_M_PRIVATE_NONSECURE_H */
