/**
 * \file            fal.h
 * \brief           Explicit Flash-region transactions for storage consumers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FAL_H
#define NEXUS_COMPONENTS_FAL_H

#include "nexus/io/flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Caller-owned bridge; fields are private after successful binding.
 * \note No hardware owner, partition registry, heap or hidden lock. The exact
 *       provider, region and clock remain live until all consumers have joined.
 */
typedef struct {
    nx_flash_region_t region;
    nx_clock_t clock;
    nx_time_us_t deadline;
    nx_time_us_t last_time;
    nx_result_t error;
    uint32_t erase_bytes;
    uint32_t program_bytes;
    bool active;
    bool faulted;
    bool recovery_required;
} nx_fal_t;

/**
 * \brief Validate complete writable regions without reserving any storage.
 * \param[in] regions: Stable product-owned region array, or NULL when empty.
 * \param[in] count: Exact number of region descriptors.
 * \return SUCCESS; INVALID or PERMISSION for bounds, geometry or overlap.
 * \note Task-only metadata check. Same-provider regions must not overlap.
 *       Provider aliases sharing one context are treated as the same device.
 */
nx_result_t nx_fal_layout_validate(const nx_flash_region_t* regions,
                                   size_t count);

/**
 * \brief Bind one explicit uniformly erasable writable region.
 * \param[out] adapter: Unused or quiesced caller-owned state.
 * \param[in] region: Exact sector-aligned product selection, copied on success.
 * \param[in] clock: Finite monotonic clock in the provider's deadline domain.
 * \return SUCCESS, INVALID, PERMISSION or UNSUPPORTED; failure changes nothing.
 * \note Task-only. Fresh-erase-only media need a separately qualified profile
 *       and are rejected here. No initialization, probe or hardware access.
 */
nx_result_t nx_fal_bind(nx_fal_t* adapter, nx_flash_region_t region,
                        nx_clock_t clock);

/**
 * \brief Begin one externally serialized operation with a shared deadline.
 * \param[in,out] adapter: Bound idle bridge, same physical execution owner.
 * \param[in] deadline: Finite absolute deadline covering the entire operation.
 * \return SUCCESS, BUSY, STATE, INVALID or TIMEOUT without hardware effects.
 * \note Does not acquire a lock. All database and filesystem users of the same
 *       physical provider must share the product's execution serialization.
 */
nx_result_t nx_fal_begin(nx_fal_t* adapter, nx_time_us_t deadline);

/**
 * \brief End an operation after every synchronous callback has returned.
 * \param[in,out] adapter: Active serialized bridge.
 * \return First latched error or SUCCESS; STATE for an inactive bridge.
 * \note An error never implies rollback. Mutating failures require recovery
 *       before another begin; no caller buffer remains borrowed on return.
 */
nx_result_t nx_fal_end(nx_fal_t* adapter);

/**
 * \brief Read within the selected region and transaction budget.
 * \param[in,out] adapter: Active bridge; same owner throughout this call.
 * \param[in] offset: Region-relative byte offset.
 * \param[out] data: Writable bytes borrowed until return only.
 * \param[in] bytes: Positive representable byte count.
 * \return Exactly bytes on success, negative on failure; never a short success.
 */
int nx_fal_read(nx_fal_t* adapter, uint32_t offset, void* data, size_t bytes);

/**
 * \brief Program complete aligned units and settle the current hardware pulse.
 * \param[in,out] adapter: Active bridge with admitted non-ECC profile.
 * \param[in] offset: Region-relative aligned byte offset.
 * \param[in] data: Immutable input borrowed until return only.
 * \param[in] bytes: Positive byte count in complete program-unit multiples,
 *                  at most INT_MAX.
 * \return Exactly bytes on success, negative on failure; partial effects
 * remain. \note No retry or read-modify-write is hidden in this adapter.
 */
int nx_fal_program(nx_fal_t* adapter, uint32_t offset, const void* data,
                   size_t bytes);

/**
 * \brief Erase only complete region-contained physical sectors.
 * \param[in,out] adapter: Active serialized bridge.
 * \param[in] offset: Region-relative exact sector boundary.
 * \param[in] bytes: Positive complete-sector byte count, at most INT_MAX.
 * \return Exactly bytes on success, negative on failure; no outward rounding.
 */
int nx_fal_erase(nx_fal_t* adapter, uint32_t offset, size_t bytes);

/**
 * \brief Retry physical cleanup without replaying any database mutation.
 * \param[in,out] adapter: Inactive failed bridge retained by the same owner.
 * \return Underlying recovery result; SUCCESS permits logical reopen only.
 * \note Recovery never upgrades the original operation or proves its contents.
 *       A clock epoch loss requires external recovery, not deadline renewal.
 */
nx_result_t nx_fal_recover(nx_fal_t* adapter);

#ifdef __cplusplus
}
#endif
#endif
