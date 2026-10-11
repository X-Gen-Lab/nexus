/**
 * \file            cache.h
 *
 * \brief           Explicit whole-line CPU cache maintenance mechanisms.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_CACHE_H
#define NEXUS_ARCH_CACHE_H

#include "nexus/arch/features.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Publish dirty data cache lines to the point of coherency.
 *
 * \param[in]       address: First byte of exclusively owned complete lines.
 * \param[in]       bytes: Nonzero whole-line length in the CPU address space.
 *
 * \return          OK after maintenance and barriers; INVALID for partial
 *                  lines or an overflowing/non-32-bit range; CONTEXT for an
 *                  unprivileged caller; UNSUPPORTED for an absent or disabled
 *                  reviewed cache. Rejection performs no maintenance writes.
 *
 * \note            Privileged task/configurable-IRQ context in the current
 *                  security domain only. The caller excludes other CPU,
 *                  security-world and DMA users of every complete line until
 *                  return, and keeps cache configuration stable. No hidden
 *                  IRQ mask, allocation or cache enable occurs. Supported
 *                  geometry is an explicitly reviewed 32-byte line; range
 *                  rounding is forbidden. Work is bounded by bytes/32 and is
 *                  not a DMA drain, device completion or transfer of ownership.
 */
nx_arch_result_t nx_arch_dcache_clean(uintptr_t address, size_t bytes);
/**
 * \brief           Discard complete data cache lines without writing them.
 *
 * \param[in]       address: First byte of exclusively owned complete lines.
 * \param[in]       bytes: Nonzero whole-line length in the CPU address space.
 *
 * \return          The same context, range and capability results as clean.
 *
 * \note            The caller permits all dirty data in these lines to be
 *                  discarded and satisfies clean's exclusion/context contract.
 *                  No adjacent line is touched or rounded into the range.
 *                  For DMA receive, providers coordinate both pre-DMA and
 *                  post-drain maintenance; this call does not establish idle.
 */
nx_arch_result_t nx_arch_dcache_invalidate(uintptr_t address, size_t bytes);
/**
 * \brief           Publish dirty data and discard the complete cache lines.
 *
 * \param[in]       address: First byte of exclusively owned complete lines.
 * \param[in]       bytes: Nonzero whole-line length in the CPU address space.
 *
 * \return          The same context, range and capability results as clean.
 *
 * \note            Satisfies clean's exclusion/context contract. Distinct from
 *                  invalidate: dirty contents are written before discard.
 */
nx_arch_result_t nx_arch_dcache_clean_invalidate(uintptr_t address,
                                                 size_t bytes);
/**
 * \brief           Make a complete owned range coherent for CPU instructions.
 *
 * \param[in]       address: First byte of exclusively owned complete lines.
 * \param[in]       bytes: Nonzero whole-line length in the CPU address space.
 *
 * \return          OK after data clean, instruction invalidate and execution
 *                  barriers; INVALID/CONTEXT as above; UNSUPPORTED when both
 *                  reviewed caches are absent or a present cache is disabled.
 *
 * \note            Satisfies clean's exclusion/context contract. All present
 *                  caches are checked before any maintenance. With only one
 *                  reviewed cache, only that cache is maintained. The caller
 *                  prevents execution from the changing range, including other
 *                  security-world users. This is not code validation, an MPU
 *                  policy change or a global cache/branch-predictor flush.
 */
nx_arch_result_t nx_arch_instruction_sync(uintptr_t address, size_t bytes);
#ifdef __cplusplus
}
#endif
#endif
