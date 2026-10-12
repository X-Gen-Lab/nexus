/**
 * \file            nonsecure.h
 * \brief           Explicit NonSecure unprivileged address permission checks
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_NONSECURE_H
#define NEXUS_ARCH_NONSECURE_H
#include "nexus/arch/features.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Requested NonSecure unprivileged CPU access, never execute access. */
typedef enum {
    NX_ARCH_NONSECURE_READ = 0,
    NX_ARCH_NONSECURE_READWRITE = 1
} nx_arch_nonsecure_access_t;
/**
 * \brief           Check current NonSecure unprivileged access to a full span
 * \param[in]       address: Non-null start; never dereferenced by this API.
 * \param[in]       bytes: Positive length with a representable inclusive end.
 * \param[in]       access: Exact READ or READWRITE access, never a bit mask.
 * \return          OK for complete current access; INVALID for malformed or
 *                  denied spans; CONTEXT outside privileged Secure Thread;
 *                  UNSUPPORTED in Native, single-state or NonSecure images.
 * \note            Allocation-free permission observation only. Hardware CMSE
 *                  checks both security attribution and NonSecure MPU rights
 *                  as an unprivileged caller. Adjacent permitted regions may
 *                  be conservatively rejected. No memory is copied, pinned or
 *                  borrowed. Success neither authenticates a gateway caller
 *                  nor proves RAM, DMA reachability, lifetime or fault safety.
 *                  The integrating owner must exclude MMIO through its SoC
 *                  RAM policy and prevent attribution/mapping changes through
 *                  the subsequent access. Interrupts, another world, DMA and
 *                  faults are not excluded here; no masks or hardware policy
 *                  are changed. A gateway must separately snapshot its input
 *                  and validate output before any irreversible operation.
 */
nx_arch_result_t nx_arch_nonsecure_check(const void* address, size_t bytes,
                                         nx_arch_nonsecure_access_t access);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_ARCH_NONSECURE_H */
