/**
 * \file            config_store.c
 * \brief           Config Manager Storage Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements the core configuration storage functionality
 *                  including key-value storage, lookup, and memory management.
 *                  Uses static memory allocation for embedded systems.
 */

#include "config_store.h"
#include "config/config.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Internal Structures                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Configuration entry structure
 */
typedef struct {
    char key[CONFIG_MAX_MAX_KEY_LEN];         /**< Key name */
    config_type_t type;                       /**< Value type */
    uint8_t flags;                            /**< Entry flags */
    uint16_t value_size;                      /**< Size of stored value */
    uint8_t namespace_id;                     /**< Namespace identifier */
    uint8_t value[CONFIG_MAX_MAX_VALUE_SIZE]; /**< Value storage */
    bool in_use;                              /**< Entry is in use */
} config_entry_internal_t;

/**
 * \brief           Config store context structure
 */
typedef struct {
    bool initialized;        /**< Store is initialized */
    uint16_t max_keys;       /**< Maximum number of keys */
    uint8_t max_key_len;     /**< Maximum key length */
    uint16_t max_value_size; /**< Maximum value size */
    size_t entry_count;      /**< Current number of entries */
    config_entry_internal_t entries[CONFIG_MAX_MAX_KEYS]; /**< Entry storage */
} config_store_ctx_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Global config store context
 */
static config_store_ctx_t g_store_ctx;

/*---------------------------------------------------------------------------*/
/* Internal Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find entry by key
 * \param[in]       key: Key to find
 * \param[in]       namespace_id: Namespace identifier
 * \return          Pointer to entry if found, NULL otherwise
 */
static config_entry_internal_t* config_store_find_entry(const char* key,
                                                        uint8_t namespace_id) {
    if (key == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < g_store_ctx.max_keys; ++i) {
        if (g_store_ctx.entries[i].in_use &&
            g_store_ctx.entries[i].namespace_id == namespace_id &&
            strncmp(g_store_ctx.entries[i].key, key, g_store_ctx.max_key_len) ==
                0) {
            return &g_store_ctx.entries[i];
        }
    }
    return NULL;
}

/**
 * \brief           Find free entry slot
 * \return          Pointer to free entry if available, NULL otherwise
 */
static config_entry_internal_t* config_store_find_free_entry(void) {
    for (size_t i = 0; i < g_store_ctx.max_keys; ++i) {
        if (!g_store_ctx.entries[i].in_use) {
            return &g_store_ctx.entries[i];
        }
    }
    return NULL;
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize the config store
 * \param[in]       max_keys: Maximum number of keys
 * \param[in]       max_key_len: Maximum key length
 * \param[in]       max_value_size: Maximum value size
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_init(uint16_t max_keys, uint8_t max_key_len,
                                  uint16_t max_value_size) {
    /* Validate parameters */
    if (max_keys < CONFIG_MIN_MAX_KEYS || max_keys > CONFIG_MAX_MAX_KEYS) {
        return CONFIG_ERROR_INVALID_PARAM;
    }
    if (max_key_len < CONFIG_MIN_MAX_KEY_LEN ||
        max_key_len > CONFIG_MAX_MAX_KEY_LEN) {
        return CONFIG_ERROR_INVALID_PARAM;
    }
    if (max_value_size < CONFIG_MIN_MAX_VALUE_SIZE ||
        max_value_size > CONFIG_MAX_MAX_VALUE_SIZE) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Initialize context */
    memset(&g_store_ctx, 0, sizeof(g_store_ctx));
    g_store_ctx.max_keys = max_keys;
    g_store_ctx.max_key_len = max_key_len;
    g_store_ctx.max_value_size = max_value_size;
    g_store_ctx.entry_count = 0;
    g_store_ctx.initialized = true;

    return CONFIG_OK;
}

/**
 * \brief           Deinitialize the config store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_deinit(void) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    memset(&g_store_ctx, 0, sizeof(g_store_ctx));
    return CONFIG_OK;
}

/**
 * \brief           Check if store is initialized
 * \return          true if initialized, false otherwise
 */
bool config_store_is_initialized(void) {
    return g_store_ctx.initialized;
}

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
                                 uint8_t namespace_id) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    /* Validate parameters */
    if (key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    size_t key_len = strlen(key);
    if (key_len == 0 || key_len >= g_store_ctx.max_key_len) {
        return CONFIG_ERROR_KEY_TOO_LONG;
    }

    if (size > g_store_ctx.max_value_size) {
        return CONFIG_ERROR_VALUE_TOO_LARGE;
    }

    /* Find existing entry or allocate new one */
    config_entry_internal_t* entry = config_store_find_entry(key, namespace_id);

    if (entry == NULL) {
        /* Allocate new entry */
        entry = config_store_find_free_entry();
        if (entry == NULL) {
            return CONFIG_ERROR_NO_SPACE;
        }
        g_store_ctx.entry_count++;
    }

    /* Store the entry */
    memset(entry->key, 0, sizeof(entry->key));
    /* Safe string copy - manually copy up to max_key_len - 1 characters */
    size_t copy_len = key_len < (size_t)(g_store_ctx.max_key_len - 1)
                          ? key_len
                          : (size_t)(g_store_ctx.max_key_len - 1);
    memcpy(entry->key, key, copy_len);
    entry->key[copy_len] = '\0';
    entry->type = type;
    entry->flags = flags;
    entry->value_size = (uint16_t)size;
    entry->namespace_id = namespace_id;
    memcpy(entry->value, value, size);
    entry->in_use = true;

    return CONFIG_OK;
}

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
                                 uint8_t namespace_id) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    /* Validate parameters */
    if (key == NULL || size == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Find entry */
    config_entry_internal_t* entry = config_store_find_entry(key, namespace_id);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Check buffer size */
    if (value != NULL && *size < entry->value_size) {
        *size = entry->value_size;
        return CONFIG_ERROR_BUFFER_TOO_SMALL;
    }

    /* Return data */
    if (type != NULL) {
        *type = entry->type;
    }
    if (flags != NULL) {
        *flags = entry->flags;
    }
    if (value != NULL) {
        memcpy(value, entry->value, entry->value_size);
    }
    *size = entry->value_size;

    return CONFIG_OK;
}

/**
 * \brief           Check if a key exists
 * \param[in]       key: Configuration key
 * \param[in]       namespace_id: Namespace identifier
 * \param[out]      exists: Pointer to store result
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_exists(const char* key, uint8_t namespace_id,
                                    bool* exists) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || exists == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_entry_internal_t* entry = config_store_find_entry(key, namespace_id);
    *exists = (entry != NULL);

    return CONFIG_OK;
}

/**
 * \brief           Get the type of a stored value
 * \param[in]       key: Configuration key
 * \param[in]       namespace_id: Namespace identifier
 * \param[out]      type: Pointer to store type
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get_type(const char* key, uint8_t namespace_id,
                                      config_type_t* type) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || type == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_entry_internal_t* entry = config_store_find_entry(key, namespace_id);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    *type = entry->type;
    return CONFIG_OK;
}

/**
 * \brief           Delete a configuration key
 * \param[in]       key: Configuration key
 * \param[in]       namespace_id: Namespace identifier
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_delete(const char* key, uint8_t namespace_id) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_entry_internal_t* entry = config_store_find_entry(key, namespace_id);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Clear the entry */
    memset(entry, 0, sizeof(*entry));
    entry->in_use = false;
    g_store_ctx.entry_count--;

    return CONFIG_OK;
}

/**
 * \brief           Get the number of stored keys
 * \param[out]      count: Pointer to store count
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get_count(size_t* count) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (count == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    *count = g_store_ctx.entry_count;
    return CONFIG_OK;
}

/**
 * \brief           Clear all entries
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_clear_all(void) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    for (size_t i = 0; i < g_store_ctx.max_keys; ++i) {
        memset(&g_store_ctx.entries[i], 0, sizeof(g_store_ctx.entries[i]));
    }
    g_store_ctx.entry_count = 0;

    return CONFIG_OK;
}

/**
 * \brief           Get value size for a key
 * \param[in]       key: Configuration key
 * \param[in]       namespace_id: Namespace identifier
 * \param[out]      size: Pointer to store size
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get_size(const char* key, uint8_t namespace_id,
                                      size_t* size) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || size == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_entry_internal_t* entry = config_store_find_entry(key, namespace_id);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    *size = entry->value_size;
    return CONFIG_OK;
}

/**
 * \brief           Clear all entries in a namespace
 * \param[in]       namespace_id: Namespace identifier
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_clear_namespace(uint8_t namespace_id) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    for (size_t i = 0; i < g_store_ctx.max_keys; ++i) {
        if (g_store_ctx.entries[i].in_use &&
            g_store_ctx.entries[i].namespace_id == namespace_id) {
            memset(&g_store_ctx.entries[i], 0, sizeof(g_store_ctx.entries[i]));
            g_store_ctx.entry_count--;
        }
    }

    return CONFIG_OK;
}

/**
 * \brief           Get count of entries in a namespace
 * \param[in]       namespace_id: Namespace identifier
 * \param[out]      count: Pointer to store count
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get_namespace_count(uint8_t namespace_id,
                                                 size_t* count) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (count == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    size_t ns_count = 0;
    for (size_t i = 0; i < g_store_ctx.max_keys; ++i) {
        if (g_store_ctx.entries[i].in_use &&
            g_store_ctx.entries[i].namespace_id == namespace_id) {
            ns_count++;
        }
    }

    *count = ns_count;
    return CONFIG_OK;
}

/**
 * \brief           Iterate over all configuration entries
 * \param[in]       callback: Iteration callback
 * \param[in]       user_data: User-provided context
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_iterate(config_store_iterate_cb_t callback,
                                     void* user_data) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (callback == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < g_store_ctx.max_keys; ++i) {
        if (g_store_ctx.entries[i].in_use) {
            config_store_entry_info_t info;
            memset(&info, 0, sizeof(info));

            /* Copy key */
            size_t key_len = strlen(g_store_ctx.entries[i].key);
            size_t copy_len = key_len < (CONFIG_MAX_MAX_KEY_LEN - 1)
                                  ? key_len
                                  : (CONFIG_MAX_MAX_KEY_LEN - 1);
            memcpy(info.key, g_store_ctx.entries[i].key, copy_len);
            info.key[copy_len] = '\0';

            info.type = g_store_ctx.entries[i].type;
            info.value_size = g_store_ctx.entries[i].value_size;
            info.flags = g_store_ctx.entries[i].flags;
            info.namespace_id = g_store_ctx.entries[i].namespace_id;

            /* Call callback, stop if it returns false */
            if (!callback(&info, user_data)) {
                break;
            }
        }
    }

    return CONFIG_OK;
}

/**
 * \brief           Iterate over entries in a specific namespace
 * \param[in]       namespace_id: Namespace identifier
 * \param[in]       callback: Iteration callback
 * \param[in]       user_data: User-provided context
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_iterate_namespace(
    uint8_t namespace_id, config_store_iterate_cb_t callback, void* user_data) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (callback == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < g_store_ctx.max_keys; ++i) {
        if (g_store_ctx.entries[i].in_use &&
            g_store_ctx.entries[i].namespace_id == namespace_id) {
            config_store_entry_info_t info;
            memset(&info, 0, sizeof(info));

            /* Copy key */
            size_t key_len = strlen(g_store_ctx.entries[i].key);
            size_t copy_len = key_len < (CONFIG_MAX_MAX_KEY_LEN - 1)
                                  ? key_len
                                  : (CONFIG_MAX_MAX_KEY_LEN - 1);
            memcpy(info.key, g_store_ctx.entries[i].key, copy_len);
            info.key[copy_len] = '\0';

            info.type = g_store_ctx.entries[i].type;
            info.value_size = g_store_ctx.entries[i].value_size;
            info.flags = g_store_ctx.entries[i].flags;
            info.namespace_id = g_store_ctx.entries[i].namespace_id;

            /* Call callback, stop if it returns false */
            if (!callback(&info, user_data)) {
                break;
            }
        }
    }

    return CONFIG_OK;
}

/**
 * \brief           Get entry flags
 * \param[in]       key: Configuration key
 * \param[in]       namespace_id: Namespace identifier
 * \param[out]      flags: Pointer to store flags
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_store_get_flags(const char* key, uint8_t namespace_id,
                                       uint8_t* flags) {
    if (!g_store_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || flags == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_entry_internal_t* entry = config_store_find_entry(key, namespace_id);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    *flags = entry->flags;
    return CONFIG_OK;
}
