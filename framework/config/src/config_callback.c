/**
 * \file            config_callback.c
 * \brief           Config Manager Callback Notification Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements callback notification functionality for the
 *                  Config Manager. Callbacks are invoked when configuration
 *                  values change, allowing applications to respond dynamically.
 *
 *                  Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6
 */

#include "config/config.h"
#include "config_callback.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Internal Structures                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Callback entry structure
 */
typedef struct {
    char key[CONFIG_MAX_MAX_KEY_LEN];   /**< Key to watch (empty for wildcard) */
    config_change_cb_t callback;         /**< Callback function */
    void* user_data;                     /**< User-provided context */
    bool wildcard;                       /**< Is this a wildcard callback */
    bool in_use;                         /**< Entry is in use */
} config_callback_entry_t;

/**
 * \brief           Callback handle internal structure
 */
struct config_callback {
    size_t index;                        /**< Index in callback array */
    bool valid;                          /**< Handle is valid */
};

/**
 * \brief           Callback manager context
 */
typedef struct {
    bool initialized;                    /**< Manager is initialized */
    uint8_t max_callbacks;               /**< Maximum number of callbacks */
    size_t callback_count;               /**< Number of registered callbacks */
    config_callback_entry_t callbacks[CONFIG_DEFAULT_MAX_CALLBACKS]; /**< Callback storage */
    struct config_callback handles[CONFIG_DEFAULT_MAX_CALLBACKS];    /**< Handle storage */
} config_callback_ctx_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Global callback manager context
 */
static config_callback_ctx_t g_cb_ctx;

/*---------------------------------------------------------------------------*/
/* Internal Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find free callback slot
 * \return          Index of free slot if available, -1 otherwise
 */
static int
config_callback_find_free_slot(void) {
    for (uint8_t i = 0; i < g_cb_ctx.max_callbacks; ++i) {
        if (!g_cb_ctx.callbacks[i].in_use) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * \brief           Find free handle slot
 * \return          Pointer to free handle if available, NULL otherwise
 */
static struct config_callback*
config_callback_find_free_handle(void) {
    for (uint8_t i = 0; i < g_cb_ctx.max_callbacks; ++i) {
        if (!g_cb_ctx.handles[i].valid) {
            return &g_cb_ctx.handles[i];
        }
    }
    return NULL;
}

/**
 * \brief           Check if a key matches a callback entry
 * \param[in]       entry: Callback entry
 * \param[in]       key: Key to check
 * \return          true if matches, false otherwise
 */
static bool
config_callback_key_matches(const config_callback_entry_t* entry, const char* key) {
    if (entry == NULL || key == NULL) {
        return false;
    }

    /* Wildcard callbacks match all keys */
    if (entry->wildcard) {
        return true;
    }

    /* Exact key match */
    return strncmp(entry->key, key, CONFIG_MAX_MAX_KEY_LEN) == 0;
}

/*---------------------------------------------------------------------------*/
/* Internal API Implementation                                               */
/*---------------------------------------------------------------------------*/

config_status_t
config_callback_init(uint8_t max_callbacks) {
    if (max_callbacks == 0 || max_callbacks > CONFIG_DEFAULT_MAX_CALLBACKS) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    memset(&g_cb_ctx, 0, sizeof(g_cb_ctx));
    g_cb_ctx.max_callbacks = max_callbacks;
    g_cb_ctx.callback_count = 0;
    g_cb_ctx.initialized = true;

    return CONFIG_OK;
}

config_status_t
config_callback_deinit(void) {
    if (!g_cb_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    memset(&g_cb_ctx, 0, sizeof(g_cb_ctx));
    return CONFIG_OK;
}

bool
config_callback_is_initialized(void) {
    return g_cb_ctx.initialized;
}

config_status_t
config_callback_notify(const char* key, config_type_t type,
                       const void* old_value, size_t old_size,
                       const void* new_value, size_t new_size) {
    if (!g_cb_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || new_value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    CONFIG_UNUSED(old_size);
    CONFIG_UNUSED(new_size);

    /* Iterate through all callbacks and invoke matching ones */
    for (uint8_t i = 0; i < g_cb_ctx.max_callbacks; ++i) {
        config_callback_entry_t* entry = &g_cb_ctx.callbacks[i];

        if (!entry->in_use || entry->callback == NULL) {
            continue;
        }

        if (config_callback_key_matches(entry, key)) {
            /* Invoke callback - continue even if it fails (Requirement 7.6) */
            entry->callback(key, type, old_value, new_value, entry->user_data);
        }
    }

    return CONFIG_OK;
}

config_status_t
config_callback_get_count(size_t* count) {
    if (!g_cb_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (count == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    *count = g_cb_ctx.callback_count;
    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

config_status_t
config_register_callback(const char* key, config_change_cb_t callback,
                         void* user_data, config_cb_handle_t* handle) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (!g_cb_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || callback == NULL || handle == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Check key length */
    size_t key_len = strlen(key);
    if (key_len == 0 || key_len >= CONFIG_MAX_MAX_KEY_LEN) {
        return CONFIG_ERROR_KEY_TOO_LONG;
    }

    /* Find free callback slot */
    int slot = config_callback_find_free_slot();
    if (slot < 0) {
        return CONFIG_ERROR_NO_SPACE;
    }

    /* Find free handle */
    struct config_callback* cb_handle = config_callback_find_free_handle();
    if (cb_handle == NULL) {
        return CONFIG_ERROR_NO_SPACE;
    }

    /* Register the callback */
    config_callback_entry_t* entry = &g_cb_ctx.callbacks[slot];
    memset(entry->key, 0, sizeof(entry->key));
    size_t copy_len = key_len < (CONFIG_MAX_MAX_KEY_LEN - 1)
                      ? key_len
                      : (CONFIG_MAX_MAX_KEY_LEN - 1);
    memcpy(entry->key, key, copy_len);
    entry->key[copy_len] = '\0';
    entry->callback = callback;
    entry->user_data = user_data;
    entry->wildcard = false;
    entry->in_use = true;

    /* Initialize handle */
    cb_handle->index = (size_t)slot;
    cb_handle->valid = true;

    g_cb_ctx.callback_count++;
    *handle = cb_handle;

    return CONFIG_OK;
}

config_status_t
config_register_wildcard_callback(config_change_cb_t callback,
                                  void* user_data,
                                  config_cb_handle_t* handle) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (!g_cb_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (callback == NULL || handle == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Find free callback slot */
    int slot = config_callback_find_free_slot();
    if (slot < 0) {
        return CONFIG_ERROR_NO_SPACE;
    }

    /* Find free handle */
    struct config_callback* cb_handle = config_callback_find_free_handle();
    if (cb_handle == NULL) {
        return CONFIG_ERROR_NO_SPACE;
    }

    /* Register the wildcard callback */
    config_callback_entry_t* entry = &g_cb_ctx.callbacks[slot];
    memset(entry->key, 0, sizeof(entry->key));
    entry->callback = callback;
    entry->user_data = user_data;
    entry->wildcard = true;
    entry->in_use = true;

    /* Initialize handle */
    cb_handle->index = (size_t)slot;
    cb_handle->valid = true;

    g_cb_ctx.callback_count++;
    *handle = cb_handle;

    return CONFIG_OK;
}

config_status_t
config_unregister_callback(config_cb_handle_t handle) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (!g_cb_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (handle == NULL || !handle->valid) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    size_t index = handle->index;
    if (index >= g_cb_ctx.max_callbacks) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_callback_entry_t* entry = &g_cb_ctx.callbacks[index];
    if (!entry->in_use) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Clear the callback entry */
    memset(entry, 0, sizeof(*entry));
    entry->in_use = false;

    /* Invalidate the handle */
    handle->valid = false;

    if (g_cb_ctx.callback_count > 0) {
        g_cb_ctx.callback_count--;
    }

    return CONFIG_OK;
}
