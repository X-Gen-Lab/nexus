/**
 * \file            cache_control.h
 * \brief           Explicit owned Cortex-M7 cold cache transitions
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_CACHE_CONTROL_H
#define NEXUS_ARCH_CACHE_CONTROL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Data-cache ownership bit, independent of the hardware CCR layout. */
#define NX_ARCH_CACHE_DATA 1U
/** \brief Instruction-cache ownership bit. */
#define NX_ARCH_CACHE_INSTRUCTION 2U
/** \brief Cold-transition admission or execution result. */
typedef enum {
    NX_ARCH_CACHE_OK = 0,
    NX_ARCH_CACHE_INVALID,
    NX_ARCH_CACHE_CONTEXT,
    NX_ARCH_CACHE_UNSUPPORTED,
    NX_ARCH_CACHE_STATE,
    NX_ARCH_CACHE_IO
} nx_arch_cache_control_result_t;
/**
 * \brief           Caller-owned cache responsibility and selector recovery.
 * \note            Zero initialize before first use; do not modify fields or
 *                  copy a live object. Retain the object until owned_mask and
 *                  selector_pending are both zero. owned_mask conservatively
 *                  records cleanup responsibility, not enabled hardware bits.
 */
typedef struct {
    uint32_t owned_mask; /**< Retained data/instruction cache responsibility. */
    uint32_t
        saved_selector;    /**< Original CSSELR for an unfinished operation. */
    bool selector_pending; /**< Original selector restoration is outstanding. */
} nx_arch_cache_control_t;
/**
 * \brief           Admit cold caches and enable the explicitly selected ones.
 * \param[in,out]   control: Exclusively owned, initially empty control object.
 * \param[in]       mask: Nonzero combination of DATA and INSTRUCTION bits.
 * \return          OK after verified enable and selector restoration; INVALID
 *                  for arguments; CONTEXT for unsafe context; UNSUPPORTED for
 *                  an unreviewed CPU/geometry; STATE for live/foreign caches;
 *                  IO when a register write is not confirmed by readback.
 * \note            Reviewed M7 only. Requires privileged Thread, PRIMASK=1,
 *                  BASEPRI=FAULTMASK=0 and known-clean caches from a trusted
 *                  reset or prior successful owned clean/disable. CCR bits
 *                  being zero alone do not prove clean foreign state; never
 *                  adopt an unknown bootloader state. Caller
 *                  excludes all other cache users, DMA, NMI and HardFault
 *                  access to affected memory throughout the transition. It
 *                  owns memory attributes, execution and coherency policy;
 *                  enabling a cache does not establish DMA coherence. No IRQ
 *                  mask changes, allocation or cache adoption occur. Rejected
 *                  initial admission has no retained responsibility. An IO
 *                  failure can retain owned_mask/selector_pending; call
 *                  cold_disable and retain storage until it succeeds.
 */
nx_arch_cache_control_result_t
nx_arch_cache_cold_enable(nx_arch_cache_control_t* control, uint32_t mask);
/**
 * \brief           Clean and disable caches owned by this control object.
 * \param[in,out]   control: Same live object passed to cold_enable.
 * \return          OK only after cache disable and selector restoration;
 *                  INVALID, CONTEXT, UNSUPPORTED, STATE or IO retain ownership.
 * \note            Satisfies enable's exclusion and context contract after
 *                  every DMA/provider has drained. Rejects enabled caches
 *                  outside owned_mask. The data clean/disable transition uses
 *                  a stack-free leaf so no local dirty line can appear between
 *                  the final clean and CCR update. Its bounded work is four
 *                  ways times the verified set count, not constant latency.
 *                  Failed operations remain retryable without adopting a new
 *                  cache or forgetting the original selector. A zero object
 *                  is an idempotent no-op on a supported CPU profile.
 */
nx_arch_cache_control_result_t
nx_arch_cache_cold_disable(nx_arch_cache_control_t* control);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_ARCH_CACHE_CONTROL_H */
