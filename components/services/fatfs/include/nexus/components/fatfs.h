/**
 * \file            fatfs.h
 * \brief           FatFs single-volume profile over an existing Block owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FATFS_H
#define NEXUS_COMPONENTS_FATFS_H
#include "nexus/io/block.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Caller-owned volume; private vendor state remains in its workspace.
 */
typedef struct {
    const nx_block_port_t* port;
    nx_clock_t clock;
    uint64_t first_block;
    uint64_t block_count;
    void* state;
    nx_result_t error;
    bool mounted;
    bool entered;
    bool faulted;
} nx_fatfs_t;
/** \brief Immutable scope supplied by the product's same-owner serialization.
 */
typedef struct {
    nx_fatfs_t* filesystem;
    nx_time_us_t deadline;
} nx_fatfs_operation_t;
/**
 * \brief Resolve the exact active volume operation for vendor disk callbacks.
 * \return Same scope pointer passed to the current API, or NULL outside calls.
 * \note Consumer supplies this function. No library current-device registry,
 *       implicit initialization, task or lock. One FatFs volume per link
 * profile.
 */
const nx_fatfs_operation_t* nx_fatfs_operation_get(void);
/** \brief Exact workspace bytes, including vendor state and formatting buffer.
 */
size_t nx_fatfs_workspace_bytes(void);
/** \brief Required alignment for metadata and the selected Block buffer. */
size_t nx_fatfs_workspace_alignment(void);
/**
 * \brief Bind an already active writable 512-byte Block media session.
 * \param[out] filesystem: Unused caller metadata.
 * \param[in] port: Stable existing Block owner, not a raw NOR Flash wrapper.
 * \param[in] clock: Ready monotonic provider-domain clock.
 * \param[in] first_block: Product partition start in whole sectors.
 * \param[in] block_count: Positive product partition extent.
 * \param[in,out] workspace: Disjoint aligned storage retained until unmount.
 * \param[in] bytes: At least the exact reported requirement.
 * \return SUCCESS, INVALID, STATE or UNSUPPORTED without media effects.
 * \note No VFS, FTL, heap or second hardware owner. Long names are disabled.
 */
nx_result_t nx_fatfs_init(nx_fatfs_t* filesystem, const nx_block_port_t* port,
                          nx_clock_t clock, uint64_t first_block,
                          uint64_t block_count, void* workspace, size_t bytes);
/**
 * \brief Format only an explicitly selected unmounted volume.
 * \param[in] operation: Exact active product scope and finite full-call budget.
 * \return SUCCESS or error; destructive partial effects may remain.
 * \note Never called by mount or error recovery. FAT is not a journaled
 * database.
 */
nx_result_t nx_fatfs_format(const nx_fatfs_operation_t* operation);
/**
 * \brief Mount existing FAT data without fallback formatting.
 * \param[in] operation: Active product scope for an unmounted volume.
 * \return SUCCESS or error; prior media uncertainty keeps admission closed.
 */
nx_result_t nx_fatfs_mount(const nx_fatfs_operation_t* operation);
/**
 * \brief Unmount after users join, retaining the existing Block owner.
 * \param[in] operation: Active product scope for the exact selected volume.
 * \return SUCCESS or error; timeout does not authorize removing media or
 * clocks.
 */
nx_result_t nx_fatfs_unmount(const nx_fatfs_operation_t* operation);
/**
 * \brief Write one short-name relative file and synchronize before return.
 * \param[in] operation: Mounted same-owner scope with one finite deadline.
 * \param[in] path: Relative 8.3 path, at most 63 bytes, without volume prefix.
 * \param[in] data: Immutable bytes borrowed until return only.
 * \param[in] bytes: Exact length, NULL permitted only for an empty file.
 * \return SUCCESS or error; failed writes may have changed a sector prefix.
 */
nx_result_t nx_fatfs_put(const nx_fatfs_operation_t* operation,
                         const char* path, const void* data, size_t bytes);
/**
 * \brief Copy one complete file without silently accepting a short Block read.
 * \param[in] operation: Mounted exact active scope and finite deadline.
 * \param[in] path: Relative short-name path.
 * \param[out] data: Disjoint output, or NULL for a size query.
 * \param[in,out] bytes: Capacity on entry, required length on
 * success/EXHAUSTED. \return SUCCESS, EMPTY, EXHAUSTED or error; failure output
 * is unspecified.
 */
nx_result_t nx_fatfs_get(const nx_fatfs_operation_t* operation,
                         const char* path, void* data, size_t* bytes);
#ifdef __cplusplus
}
#endif
#endif
