/**
 * \file            config_default.c
 * \brief           Config Manager Default Value Management
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements default value management functionality including
 *                  registering defaults, fallback to defaults, and reset operations.
 *                  Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6
 */

#include "config/config.h"
#include "config_store.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Internal Structures                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Internal default value entry structure
 */
typedef struct {
    char key[CONFIG_MAX_MAX_KEY_LEN];   /**< Key name */
    config_type_t type;                  /**< Value type */
    uint16_t value_size;                 /**< Size of stored value */
    uint8_t value[CONFIG_MAX_MAX_VALUE_SIZE]; /**< Value storage */
    bool in_use;                         /**< Entry is in use */
} config_default_entry_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Default value storage
 */
static config_default_entry_t g_defaults[CONFIG_DEFAULT_MAX_DEFAULTS];

/**
 * \brief           Number of registered defaults
 */
static size_t g_default_count = 0;

/**
 * \brief           Defaults initialized flag
 */
static bool g_defaults_initialized = false;

/*---------------------------------------------------------------------------*/
/* Internal Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize defaults storage (called from config_init)
 */
static void
config_default_init_storage(void) {
    if (!g_defaults_initialized) {
        memset(g_defaults, 0, sizeof(g_defaults));
        g_default_count = 0;
        g_defaults_initialized = true;
    }
}

/**
 * \brief           Find default entry by key
 * \param[in]       key: Key to find
 * \return          Pointer to entry if found, NULL otherwise
 */
static config_default_entry_t*
config_default_find_entry(const char* key) {
    if (key == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < CONFIG_DEFAULT_MAX_DEFAULTS; ++i) {
        if (g_defaults[i].in_use &&
            strncmp(g_defaults[i].key, key, CONFIG_MAX_MAX_KEY_LEN) == 0) {
            return &g_defaults[i];
        }
    }
    return NULL;
}

/**
 * \brief           Find free default entry slot
 * \return          Pointer to free entry if available, NULL otherwise
 */
static config_default_entry_t*
config_default_find_free_entry(void) {
    for (size_t i = 0; i < CONFIG_DEFAULT_MAX_DEFAULTS; ++i) {
        if (!g_defaults[i].in_use) {
            return &g_defaults[i];
        }
    }
    return NULL;
}

/**
 * \brief           Store a default value internally
 * \param[in]       key: Configuration key
 * \param[in]       type: Value type
 * \param[in]       value: Value data
 * \param[in]       size: Value size
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t
config_default_store(const char* key, config_type_t type, const void* value, size_t size) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    /* Initialize storage if needed */
    config_default_init_storage();

    /* Validate parameters */
    if (key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    size_t key_len = strlen(key);
    if (key_len == 0 || key_len >= CONFIG_MAX_MAX_KEY_LEN) {
        return CONFIG_ERROR_KEY_TOO_LONG;
    }

    if (size > CONFIG_MAX_MAX_VALUE_SIZE) {
        return CONFIG_ERROR_VALUE_TOO_LARGE;
    }

    /* Find existing entry or allocate new one */
    config_default_entry_t* entry = config_default_find_entry(key);
    
    if (entry == NULL) {
        /* Allocate new entry */
        entry = config_default_find_free_entry();
        if (entry == NULL) {
            return CONFIG_ERROR_NO_SPACE;
        }
        g_default_count++;
    }

    /* Store the default value */
    memset(entry->key, 0, sizeof(entry->key));
    size_t copy_len = key_len < (CONFIG_MAX_MAX_KEY_LEN - 1) 
                      ? key_len 
                      : (CONFIG_MAX_MAX_KEY_LEN - 1);
    memcpy(entry->key, key, copy_len);
    entry->key[copy_len] = '\0';
    entry->type = type;
    entry->value_size = (uint16_t)size;
    memcpy(entry->value, value, size);
    entry->in_use = true;

    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

config_status_t
config_set_default_i32(const char* key, int32_t value) {
    return config_default_store(key, CONFIG_TYPE_I32, &value, sizeof(int32_t));
}

config_status_t
config_set_default_str(const char* key, const char* value) {
    if (value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }
    size_t len = strlen(value) + 1; /* Include null terminator */
    return config_default_store(key, CONFIG_TYPE_STRING, value, len);
}

config_status_t
config_set_default_u32(const char* key, uint32_t value) {
    return config_default_store(key, CONFIG_TYPE_U32, &value, sizeof(uint32_t));
}

config_status_t
config_set_default_i64(const char* key, int64_t value) {
    return config_default_store(key, CONFIG_TYPE_I64, &value, sizeof(int64_t));
}

config_status_t
config_set_default_float(const char* key, float value) {
    return config_default_store(key, CONFIG_TYPE_FLOAT, &value, sizeof(float));
}

config_status_t
config_set_default_bool(const char* key, bool value) {
    return config_default_store(key, CONFIG_TYPE_BOOL, &value, sizeof(bool));
}

config_status_t
config_reset_to_default(const char* key) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Find the default value */
    config_default_entry_t* entry = config_default_find_entry(key);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Delete current value if exists */
    bool exists = false;
    config_status_t status = config_store_exists(key, CONFIG_DEFAULT_NAMESPACE_ID, &exists);
    if (status != CONFIG_OK) {
        return status;
    }

    if (exists) {
        status = config_store_delete(key, CONFIG_DEFAULT_NAMESPACE_ID);
        if (status != CONFIG_OK) {
            return status;
        }
    }

    /* Store the default value as the current value */
    return config_store_set(key, entry->type, entry->value, entry->value_size,
                            CONFIG_FLAG_NONE, CONFIG_DEFAULT_NAMESPACE_ID);
}

config_status_t
config_reset_all_to_defaults(void) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    /* Reset each registered default */
    for (size_t i = 0; i < CONFIG_DEFAULT_MAX_DEFAULTS; ++i) {
        if (g_defaults[i].in_use) {
            config_status_t status = config_reset_to_default(g_defaults[i].key);
            if (status != CONFIG_OK && status != CONFIG_ERROR_NOT_FOUND) {
                return status;
            }
        }
    }

    return CONFIG_OK;
}

config_status_t
config_register_defaults(const config_default_t* defaults, size_t count) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (defaults == NULL || count == 0) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Initialize storage if needed */
    config_default_init_storage();

    for (size_t i = 0; i < count; ++i) {
        const config_default_t* def = &defaults[i];
        config_status_t status = CONFIG_OK;

        switch (def->type) {
            case CONFIG_TYPE_I32:
                status = config_set_default_i32(def->key, def->value.i32_val);
                break;
            case CONFIG_TYPE_U32:
                status = config_set_default_u32(def->key, def->value.u32_val);
                break;
            case CONFIG_TYPE_I64:
                status = config_set_default_i64(def->key, def->value.i64_val);
                break;
            case CONFIG_TYPE_FLOAT:
                status = config_set_default_float(def->key, def->value.float_val);
                break;
            case CONFIG_TYPE_BOOL:
                status = config_set_default_bool(def->key, def->value.bool_val);
                break;
            case CONFIG_TYPE_STRING:
                status = config_set_default_str(def->key, def->value.str_val);
                break;
            default:
                status = CONFIG_ERROR_INVALID_PARAM;
                break;
        }

        if (status != CONFIG_OK) {
            return status;
        }
    }

    return CONFIG_OK;
}

/**
 * \brief           Get default value for a key (internal use)
 * \param[in]       key: Configuration key
 * \param[out]      type: Value type (optional, can be NULL)
 * \param[out]      value: Buffer to store value
 * \param[in,out]   size: Input: buffer size, Output: actual size
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t
config_get_default(const char* key, config_type_t* type, void* value, size_t* size) {
    if (key == NULL || size == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_default_entry_t* entry = config_default_find_entry(key);
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
    if (value != NULL) {
        memcpy(value, entry->value, entry->value_size);
    }
    *size = entry->value_size;

    return CONFIG_OK;
}

/**
 * \brief           Check if a default value exists for a key
 * \param[in]       key: Configuration key
 * \param[out]      exists: Pointer to store result
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t
config_has_default(const char* key, bool* exists) {
    if (key == NULL || exists == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_default_entry_t* entry = config_default_find_entry(key);
    *exists = (entry != NULL);

    return CONFIG_OK;
}

/**
 * \brief           Clear all registered defaults (called from config_deinit)
 */
void
config_default_clear_all(void) {
    memset(g_defaults, 0, sizeof(g_defaults));
    g_default_count = 0;
    g_defaults_initialized = false;
}
