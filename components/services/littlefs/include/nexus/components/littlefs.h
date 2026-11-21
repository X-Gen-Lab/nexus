/**
 * \file            littlefs.h
 * \brief           LittleFS over caller-selected Flash regions and workspaces
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_LITTLEFS_H
#define NEXUS_COMPONENTS_LITTLEFS_H
#include "nexus/components/fal.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Private vendor state and caches live in the caller workspace. */
typedef struct {
    nx_fal_t* flash;
    void* state;
    bool mounted;
    bool entered;
} nx_littlefs_t;
/** \brief Exact fixed-profile workspace size, including every cache buffer. */
size_t nx_littlefs_workspace_bytes(void);
/** \brief Required alignment for the caller-provided workspace. */
size_t nx_littlefs_workspace_alignment(void);
/**
 * \brief Bind metadata without touching or formatting Flash.
 * \param[out] filesystem: Unused caller metadata.
 * \param[in,out] flash: Stable uniform region bridge, retained until unmount.
 * \param[in,out] workspace: Disjoint aligned caller storage for vendor state.
 * \param[in] bytes: At least the reported exact workspace requirement.
 * \return SUCCESS, INVALID or UNSUPPORTED without I/O on rejection.
 * \note Task-only, same physical owner; no heap, VFS, worker or hidden lock.
 *       The fixed cache is 256 bytes; program units must divide that size.
 */
nx_result_t nx_littlefs_init(nx_littlefs_t* filesystem, nx_fal_t* flash,
                             void* workspace, size_t bytes);
/**
 * \brief Explicitly format an unmounted selected region.
 * \param[in,out] filesystem: Initialized exclusive metadata and live workspace.
 * \param[in] deadline: Finite whole-operation absolute deadline.
 * \return SUCCESS or error; partial destructive effects may remain on failure.
 * \note Product authorizes this call. Mount failures never invoke format.
 */
nx_result_t nx_littlefs_format(nx_littlefs_t* filesystem,
                               nx_time_us_t deadline);
/**
 * \brief Mount existing media without formatting it on failure.
 * \param[in,out] filesystem: Initialized unmounted state and stable binding.
 * \param[in] deadline: One finite deadline shared by all Flash callbacks.
 * \return SUCCESS or checked error; failed media is never treated as empty.
 */
nx_result_t nx_littlefs_mount(nx_littlefs_t* filesystem, nx_time_us_t deadline);
/**
 * \brief Unmount after every user has joined; do not stop the physical owner.
 * \param[in,out] filesystem: Mounted exclusive metadata.
 * \param[in] deadline: Finite budget in the same clock domain.
 * \return SUCCESS or retained error; caller workspace stays live until return.
 */
nx_result_t nx_littlefs_unmount(nx_littlefs_t* filesystem,
                                nx_time_us_t deadline);
/**
 * \brief Replace one file and close its borrowed handle within this call.
 * \param[in,out] filesystem: Mounted same-owner state, externally serialized.
 * \param[in] path: Relative bounded file path, at most 127 bytes.
 * \param[in] data: Immutable bytes borrowed only until return.
 * \param[in] bytes: Exact file bytes, NULL allowed only when zero.
 * \param[in] deadline: Shared finite deadline for open/write/sync/close.
 * \return SUCCESS or checked error; replacement is not a multi-file
 * transaction.
 */
nx_result_t nx_littlefs_put(nx_littlefs_t* filesystem, const char* path,
                            const void* data, size_t bytes,
                            nx_time_us_t deadline);
/**
 * \brief Read a whole file without reporting a truncated success.
 * \param[in,out] filesystem: Mounted serialized state.
 * \param[in] path: Exact relative bounded file path.
 * \param[out] data: Disjoint caller output, or NULL for a size query.
 * \param[in,out] bytes: Input capacity and output required file bytes.
 * \param[in] deadline: Shared finite budget, never renewed by a callback.
 * \return SUCCESS, EMPTY, EXHAUSTED or checked error; failure output is
 * invalid.
 */
nx_result_t nx_littlefs_get(nx_littlefs_t* filesystem, const char* path,
                            void* data, size_t* bytes, nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
