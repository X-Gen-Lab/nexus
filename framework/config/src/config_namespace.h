/**
 * \file            config_namespace.h
 * \brief           Config Manager Namespace Internal Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Internal header for namespace management functionality.
 *                  This header is not part of the public API.
 */

#ifndef CONFIG_NAMESPACE_H
#define CONFIG_NAMESPACE_H

#include "config/config_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of namespace handle structure */
struct config_namespace;

/**
 * \brief           Initialize the namespace manager
 * \param[in]       max_namespaces: Maximum number of namespaces
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_namespace_init(uint8_t max_namespaces);

/**
 * \brief           Deinitialize the namespace manager
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_namespace_deinit(void);

/**
 * \brief           Check if namespace manager is initialized
 * \return          true if initialized, false otherwise
 */
bool config_namespace_is_initialized(void);

/**
 * \brief           Get namespace ID by name
 * \param[in]       name: Namespace name
 * \param[out]      ns_id: Pointer to store namespace ID
 * \return          CONFIG_OK on success, CONFIG_ERROR_NOT_FOUND if not found
 */
config_status_t config_namespace_get_id(const char* name, uint8_t* ns_id);

/**
 * \brief           Create or get namespace ID
 * \param[in]       name: Namespace name
 * \param[out]      ns_id: Pointer to store namespace ID
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_namespace_create(const char* name, uint8_t* ns_id);

/**
 * \brief           Get namespace name by ID
 * \param[in]       ns_id: Namespace ID
 * \param[out]      name: Buffer to store namespace name
 * \param[in]       name_size: Size of name buffer
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_namespace_get_name(uint8_t ns_id, char* name,
                                          size_t name_size);

/**
 * \brief           Check if namespace ID is valid
 * \param[in]       ns_id: Namespace ID to check
 * \return          true if valid, false otherwise
 */
bool config_namespace_is_valid_id(uint8_t ns_id);

/**
 * \brief           Get the number of active namespaces
 * \param[out]      count: Pointer to store count
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_namespace_get_count(size_t* count);

/**
 * \brief           Get namespace ID from handle
 * \param[in]       handle: Namespace handle
 * \param[out]      ns_id: Pointer to store namespace ID
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t
config_namespace_get_handle_id(const struct config_namespace* handle,
                               uint8_t* ns_id);

/**
 * \brief           Check if namespace handle is valid
 * \param[in]       handle: Namespace handle
 * \return          true if valid, false otherwise
 */
bool config_namespace_is_valid_handle(const struct config_namespace* handle);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_NAMESPACE_H */
