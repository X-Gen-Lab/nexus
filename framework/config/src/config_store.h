/**
 * \file            config_store.h
 * \brief           Config Manager Internal Storage Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Internal header for config storage functionality.
 *                  This header is not part of the public API.
 */

#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include "config/config_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Default namespace ID
 */
#define CONFIG_DEFAULT_NAMESPACE_ID 0

/**
 * \brief           Initialize the config store
 * \param[in]       max_keys: Maximum number of keys
 * \param[in]       max_key_len: Maximum key length
 * \param[in]       max_value_size: Maximum value size
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_init(uint16_t max_keys, uint8_t max_key_len,
                                  uint16_t max_value_size);

/**
 * \brief           Deinitialize the config store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_deinit(void);

/**
 * \brief           Check if store is initialized
 * \return          true if initialized, false otherwise
 */
bool config_store_is_initialized(void);

/**
 * \brief           Store a value
 * \param[in]       key: Configuration key
 * \param[in]       type: Value type
 * \param[in]       value: Value data
 * \param[in]       size: Value size
 * \param[in]       flags: Entry flags
 * \param[in]       namespace_id: Namespace identifier
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_set(const char* key, config_type_t type,
                                 const void* value, size_t size, uint8_t flags,
                                 uint8_t namespace_id);

/**
 * \brief           Get a value
 * \param[in]       key: Configuration key
 * \param[out]      type: Value type (optional, can be NULL)
 * \param[out]      value: Buffer to store value
 * \param[in,out]   size: Input: buffer size, Output: actual size
 * \param[out]      flags: Entry flags (optional, can be NULL)
 * \param[in]       namespace_id: Namespace identifier
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get(const char* key, config_type_t* type,
                                 void* value, size_t* size, uint8_t* flags,
                                 uint8_t namespace_id);

/**
 * \brief           Check if a key exists
 * \param[in]       key: Configuration key
 * \param[in]       namespace_id: Namespace identifier
 * \param[out]      exists: Pointer to store result
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_exists(const char* key, uint8_t namespace_id,
                                    bool* exists);

/**
 * \brief           Get the type of a stored value
 * \param[in]       key: Configuration key
 * \param[in]       namespace_id: Namespace identifier
 * \param[out]      type: Pointer to store type
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get_type(const char* key, uint8_t namespace_id,
                                      config_type_t* type);

/**
 * \brief           Delete a configuration key
 * \param[in]       key: Configuration key
 * \param[in]       namespace_id: Namespace identifier
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_delete(const char* key, uint8_t namespace_id);

/**
 * \brief           Get the number of stored keys
 * \param[out]      count: Pointer to store count
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get_count(size_t* count);

/**
 * \brief           Clear all entries
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_clear_all(void);

/**
 * \brief           Get value size for a key
 * \param[in]       key: Configuration key
 * \param[in]       namespace_id: Namespace identifier
 * \param[out]      size: Pointer to store size
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get_size(const char* key, uint8_t namespace_id,
                                      size_t* size);

/**
 * \brief           Clear all entries in a namespace
 * \param[in]       namespace_id: Namespace identifier
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_clear_namespace(uint8_t namespace_id);

/**
 * \brief           Get count of entries in a namespace
 * \param[in]       namespace_id: Namespace identifier
 * \param[out]      count: Pointer to store count
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get_namespace_count(uint8_t namespace_id,
                                                 size_t* count);

/**
 * \brief           Entry information for iteration
 */
typedef struct {
    char key[CONFIG_MAX_MAX_KEY_LEN]; /**< Configuration key */
    config_type_t type;               /**< Value type */
    uint16_t value_size;              /**< Value size in bytes */
    uint8_t flags;                    /**< Entry flags */
    uint8_t namespace_id;             /**< Namespace identifier */
} config_store_entry_info_t;

/**
 * \brief           Iteration callback function type
 * \param[in]       info: Entry information
 * \param[in]       user_data: User-provided context
 * \return          true to continue iteration, false to stop
 */
typedef bool (*config_store_iterate_cb_t)(const config_store_entry_info_t* info,
                                          void* user_data);

/**
 * \brief           Iterate over all configuration entries
 * \param[in]       callback: Iteration callback
 * \param[in]       user_data: User-provided context
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_iterate(config_store_iterate_cb_t callback,
                                     void* user_data);

/**
 * \brief           Iterate over entries in a specific namespace
 * \param[in]       namespace_id: Namespace identifier
 * \param[in]       callback: Iteration callback
 * \param[in]       user_data: User-provided context
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_iterate_namespace(
    uint8_t namespace_id, config_store_iterate_cb_t callback, void* user_data);

/**
 * \brief           Get entry flags
 * \param[in]       key: Configuration key
 * \param[in]       namespace_id: Namespace identifier
 * \param[out]      flags: Pointer to store flags
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get_flags(const char* key, uint8_t namespace_id,
                                       uint8_t* flags);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_STORE_H */
