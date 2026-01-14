/**
 * \file            config_backend_internal.h
 * \brief           Config Manager Backend Internal Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Internal header for config backend functionality.
 *                  This header is not part of the public API.
 */

#ifndef CONFIG_BACKEND_INTERNAL_H
#define CONFIG_BACKEND_INTERNAL_H

#include "config/config_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Set the storage backend
 * \param[in]       backend: Pointer to backend structure
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_backend_set(const config_backend_t* backend);

/**
 * \brief           Get the current storage backend
 * \return          Pointer to current backend, or NULL if not set
 */
const config_backend_t* config_backend_get(void);

/**
 * \brief           Check if a backend is set and initialized
 * \return          true if backend is ready, false otherwise
 */
bool config_backend_is_set(void);

/**
 * \brief           Commit all changes to the backend
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_backend_commit(void);

/**
 * \brief           Load all configurations from the backend
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_backend_load(void);

/**
 * \brief           Set auto-commit mode
 * \param[in]       auto_commit: true to enable auto-commit
 */
void config_backend_set_auto_commit(bool auto_commit);

/**
 * \brief           Get auto-commit mode
 * \return          true if auto-commit is enabled
 */
bool config_backend_get_auto_commit(void);

/**
 * \brief           Set dirty flag
 * \param[in]       dirty: true if there are uncommitted changes
 */
void config_backend_set_dirty(bool dirty);

/**
 * \brief           Check if there are uncommitted changes
 * \return          true if dirty, false otherwise
 */
bool config_backend_is_dirty(void);

/**
 * \brief           Auto-commit if enabled and dirty
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_backend_auto_commit_if_enabled(void);

/**
 * \brief           Deinitialize the backend
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_backend_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_BACKEND_INTERNAL_H */
