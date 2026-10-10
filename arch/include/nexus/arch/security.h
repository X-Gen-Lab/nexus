/**
 * \file            security.h
 *
 * \brief           Local security-state and explicit SAU region mechanisms.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_SECURITY_H
#define NEXUS_ARCH_SECURITY_H
#include "nexus/arch/features.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exact 32-byte-aligned SAU region owned by the Secure caller. */
typedef struct {
    uint32_t base;
    uint32_t limit;
    bool nonsecure_callable;
} nx_arch_sau_region_t;
/** \brief Validated SAU words; encoding never changes hardware. */
typedef struct {
    uint32_t rbar;
    uint32_t rlar;
} nx_arch_sau_words_t;
/**
 * \brief           Query this image's compiled security state.
 *
 * \return          Single-domain, Secure or NonSecure reviewed image state.
 *
 * \note            Bounded read-only Task/IRQ query, no hardware access. This
 *                  does not grant access to another world's memory or devices.
 */
nx_arch_security_state_t nx_arch_security_state(void);
/**
 * \brief           Encode an explicit SAU region without hardware access.
 *
 * \param[in]       region: Base aligned to 32 bytes; inclusive limit ending
 *                  at a complete 32-byte boundary. Base must not exceed limit.
 * \param[out]      words: Encoded region, unchanged on failure.
 *
 * \return          OK or INVALID for null arguments or invalid boundaries.
 *
 * \note            Pure Task/IRQ operation. Encoding marks the requested
 *                  region enabled but does not enable SAU hardware. External
 *                  policy owns overlap, IDAU restrictions and NSC veneers.
 */
nx_arch_result_t nx_arch_sau_encode(const nx_arch_sau_region_t* region,
                                    nx_arch_sau_words_t* words);
/**
 * \brief           Program one owned SAU region while the SAU is disabled.
 *
 * \param[in]       index: Region index below the actual hardware TYPE count.
 * \param[in]       region: Explicit reviewed region and NSC permission.
 *
 * \return          OK, INVALID, CONTEXT or UNSUPPORTED without retained state.
 *
 * \note            Secure privileged Thread with incoming PRIMASK set only.
 *                  Caller excludes all other region/selector users and keeps
 *                  NMI/HardFault outside this metadata. Requires reviewed SAU
 *                  region support. Does not modify CTRL, ALLNS, another region,
 *                  another security world's registers or enable the SAU.
 *                  Current RNR is restored; DSB/ISB complete region writes.
 *                  Policy, secure gateways and IDAU configuration stay
 * external.
 */
nx_arch_result_t nx_arch_sau_write(uint32_t index,
                                   const nx_arch_sau_region_t* region);
/**
 * \brief           Disable one owned SAU region while the SAU is disabled.
 *
 * \param[in]       index: Region index below the actual hardware TYPE count.
 *
 * \return          OK, INVALID, CONTEXT or UNSUPPORTED without other writes.
 *
 * \note            Same context and ownership contract as nx_arch_sau_write.
 *                  Preserves CTRL, ALLNS, other regions and incoming RNR.
 */
nx_arch_result_t nx_arch_sau_clear(uint32_t index);
#ifdef __cplusplus
}
#endif
#endif
