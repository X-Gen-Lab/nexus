/**
 * \file            config_namespace.c
 * \brief           Config Manager Namespace Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements namespace management functionality for the
 *                  Config Manager. Namespaces provide isolation between
 *                  different modules' configurations.
 *
 *                  Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6
 */

#include "config_namespace.h"
#include "config/config.h"
#include "config_store.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Internal Structures                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Namespace entry structure
 */
typedef struct {
    char name[CONFIG_MAX_NS_NAME_LEN]; /**< Namespace name */
    bool active;                       /**< Namespace is active/in-use */
    uint8_t ref_count;                 /**< Reference count for open handles */
} config_namespace_entry_t;

/**
 * \brief           Namespace handle internal structure
 */
struct config_namespace {
    uint8_t ns_id; /**< Namespace ID */
    bool valid;    /**< Handle is valid */
};

/**
 * \brief           Namespace manager context
 */
typedef struct {
    bool initialized;       /**< Manager is initialized */
    uint8_t max_namespaces; /**< Maximum number of namespaces */
    size_t active_count;    /**< Number of active namespaces */
    config_namespace_entry_t
        namespaces[CONFIG_DEFAULT_MAX_NAMESPACES]; /**< Namespace storage */
    struct config_namespace
        handles[CONFIG_DEFAULT_MAX_NAMESPACES * 2]; /**< Handle storage */
} config_namespace_ctx_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Global namespace manager context
 */
static config_namespace_ctx_t g_ns_ctx;

/*---------------------------------------------------------------------------*/
/* Internal Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find namespace by name
 * \param[in]       name: Namespace name
 * \return          Namespace index if found, -1 otherwise
 */
static int config_namespace_find_by_name(const char* name) {
    if (name == NULL) {
        return -1;
    }

    for (uint8_t i = 0; i < g_ns_ctx.max_namespaces; ++i) {
        if (g_ns_ctx.namespaces[i].active &&
            strncmp(g_ns_ctx.namespaces[i].name, name,
                    CONFIG_MAX_NS_NAME_LEN) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * \brief           Find free namespace slot
 * \return          Free slot index if available, -1 otherwise
 */
static int config_namespace_find_free_slot(void) {
    for (uint8_t i = 0; i < g_ns_ctx.max_namespaces; ++i) {
        if (!g_ns_ctx.namespaces[i].active) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * \brief           Find free handle slot
 * \return          Pointer to free handle if available, NULL otherwise
 */
static struct config_namespace* config_namespace_find_free_handle(void) {
    size_t max_handles = (size_t)g_ns_ctx.max_namespaces * 2;
    for (size_t i = 0; i < max_handles; ++i) {
        if (!g_ns_ctx.handles[i].valid) {
            return &g_ns_ctx.handles[i];
        }
    }
    return NULL;
}

/*---------------------------------------------------------------------------*/
/* Internal API Implementation                                               */
/*---------------------------------------------------------------------------*/

config_status_t config_namespace_init(uint8_t max_namespaces) {
    if (max_namespaces == 0 || max_namespaces > CONFIG_DEFAULT_MAX_NAMESPACES) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    memset(&g_ns_ctx, 0, sizeof(g_ns_ctx));
    g_ns_ctx.max_namespaces = max_namespaces;
    g_ns_ctx.active_count = 0;
    g_ns_ctx.initialized = true;

    /* Initialize default namespace (ID 0) */
    memset(g_ns_ctx.namespaces[0].name, 0, CONFIG_MAX_NS_NAME_LEN);
    memcpy(g_ns_ctx.namespaces[0].name, "default", 7);
    g_ns_ctx.namespaces[0].active = true;
    g_ns_ctx.namespaces[0].ref_count = 0;
    g_ns_ctx.active_count = 1;

    return CONFIG_OK;
}

config_status_t config_namespace_deinit(void) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    memset(&g_ns_ctx, 0, sizeof(g_ns_ctx));
    return CONFIG_OK;
}

bool config_namespace_is_initialized(void) {
    return g_ns_ctx.initialized;
}

config_status_t config_namespace_get_id(const char* name, uint8_t* ns_id) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (name == NULL || ns_id == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    int idx = config_namespace_find_by_name(name);
    if (idx < 0) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    *ns_id = (uint8_t)idx;
    return CONFIG_OK;
}

config_status_t config_namespace_create(const char* name, uint8_t* ns_id) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (name == NULL || ns_id == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Check name length */
    size_t name_len = strlen(name);
    if (name_len == 0 || name_len >= CONFIG_MAX_NS_NAME_LEN) {
        return CONFIG_ERROR_KEY_TOO_LONG;
    }

    /* Check if namespace already exists */
    int idx = config_namespace_find_by_name(name);
    if (idx >= 0) {
        *ns_id = (uint8_t)idx;
        return CONFIG_OK;
    }

    /* Find free slot */
    idx = config_namespace_find_free_slot();
    if (idx < 0) {
        return CONFIG_ERROR_NO_SPACE;
    }

    /* Create new namespace */
    memset(g_ns_ctx.namespaces[idx].name, 0, CONFIG_MAX_NS_NAME_LEN);
    size_t copy_len = name_len < (CONFIG_MAX_NS_NAME_LEN - 1)
                          ? name_len
                          : (CONFIG_MAX_NS_NAME_LEN - 1);
    memcpy(g_ns_ctx.namespaces[idx].name, name, copy_len);
    g_ns_ctx.namespaces[idx].active = true;
    g_ns_ctx.namespaces[idx].ref_count = 0;
    g_ns_ctx.active_count++;

    *ns_id = (uint8_t)idx;
    return CONFIG_OK;
}

config_status_t config_namespace_get_name(uint8_t ns_id, char* name,
                                          size_t name_size) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (name == NULL || name_size == 0) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (ns_id >= g_ns_ctx.max_namespaces ||
        !g_ns_ctx.namespaces[ns_id].active) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    size_t copy_len = strlen(g_ns_ctx.namespaces[ns_id].name);
    if (copy_len >= name_size) {
        return CONFIG_ERROR_BUFFER_TOO_SMALL;
    }

    memset(name, 0, name_size);
    memcpy(name, g_ns_ctx.namespaces[ns_id].name, copy_len);

    return CONFIG_OK;
}

bool config_namespace_is_valid_id(uint8_t ns_id) {
    if (!g_ns_ctx.initialized) {
        return false;
    }

    return ns_id < g_ns_ctx.max_namespaces && g_ns_ctx.namespaces[ns_id].active;
}

config_status_t config_namespace_get_count(size_t* count) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (count == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    *count = g_ns_ctx.active_count;
    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

config_status_t config_open_namespace(const char* name,
                                      config_ns_handle_t* handle) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (name == NULL || handle == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Create or get namespace */
    uint8_t ns_id;
    config_status_t status = config_namespace_create(name, &ns_id);
    if (status != CONFIG_OK) {
        return status;
    }

    /* Find free handle */
    struct config_namespace* ns_handle = config_namespace_find_free_handle();
    if (ns_handle == NULL) {
        return CONFIG_ERROR_NO_SPACE;
    }

    /* Initialize handle */
    ns_handle->ns_id = ns_id;
    ns_handle->valid = true;
    g_ns_ctx.namespaces[ns_id].ref_count++;

    *handle = ns_handle;
    return CONFIG_OK;
}

config_status_t config_close_namespace(config_ns_handle_t handle) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (handle == NULL || !handle->valid) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    uint8_t ns_id = handle->ns_id;
    if (ns_id >= g_ns_ctx.max_namespaces ||
        !g_ns_ctx.namespaces[ns_id].active) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Decrement reference count */
    if (g_ns_ctx.namespaces[ns_id].ref_count > 0) {
        g_ns_ctx.namespaces[ns_id].ref_count--;
    }

    /* Invalidate handle */
    handle->valid = false;

    return CONFIG_OK;
}

config_status_t config_erase_namespace(const char* name) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (name == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Find namespace */
    uint8_t ns_id;
    config_status_t status = config_namespace_get_id(name, &ns_id);
    if (status != CONFIG_OK) {
        return status;
    }

    /* Don't allow erasing default namespace (ID 0) */
    if (ns_id == CONFIG_DEFAULT_NAMESPACE_ID) {
        /* For default namespace, just clear all entries but keep namespace */
        return config_store_clear_namespace(CONFIG_DEFAULT_NAMESPACE_ID);
    }

    /* Clear all entries in this namespace */
    status = config_store_clear_namespace(ns_id);
    if (status != CONFIG_OK) {
        return status;
    }

    /* Mark namespace as inactive (only if no open handles) */
    if (g_ns_ctx.namespaces[ns_id].ref_count == 0) {
        g_ns_ctx.namespaces[ns_id].active = false;
        memset(g_ns_ctx.namespaces[ns_id].name, 0, CONFIG_MAX_NS_NAME_LEN);
        g_ns_ctx.active_count--;
    }

    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Namespace-scoped Operations                                               */
/*---------------------------------------------------------------------------*/

config_status_t config_ns_set_i32(config_ns_handle_t ns, const char* key,
                                  int32_t value) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || !ns->valid || key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!config_namespace_is_valid_id(ns->ns_id)) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    return config_store_set(key, CONFIG_TYPE_I32, &value, sizeof(int32_t),
                            CONFIG_FLAG_NONE, ns->ns_id);
}

config_status_t config_ns_get_i32(config_ns_handle_t ns, const char* key,
                                  int32_t* value, int32_t default_val) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || !ns->valid || key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!config_namespace_is_valid_id(ns->ns_id)) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Check if key exists and get its type */
    config_type_t type;
    config_status_t status = config_store_get_type(key, ns->ns_id, &type);

    if (status == CONFIG_ERROR_NOT_FOUND) {
        *value = default_val;
        return CONFIG_OK;
    }

    if (status != CONFIG_OK) {
        return status;
    }

    /* Verify type */
    if (type != CONFIG_TYPE_I32) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = sizeof(int32_t);
    return config_store_get(key, NULL, value, &size, NULL, ns->ns_id);
}

config_status_t config_ns_set_u32(config_ns_handle_t ns, const char* key,
                                  uint32_t value) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || !ns->valid || key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!config_namespace_is_valid_id(ns->ns_id)) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    return config_store_set(key, CONFIG_TYPE_U32, &value, sizeof(uint32_t),
                            CONFIG_FLAG_NONE, ns->ns_id);
}

config_status_t config_ns_get_u32(config_ns_handle_t ns, const char* key,
                                  uint32_t* value, uint32_t default_val) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || !ns->valid || key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!config_namespace_is_valid_id(ns->ns_id)) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    config_type_t type;
    config_status_t status = config_store_get_type(key, ns->ns_id, &type);

    if (status == CONFIG_ERROR_NOT_FOUND) {
        *value = default_val;
        return CONFIG_OK;
    }

    if (status != CONFIG_OK) {
        return status;
    }

    if (type != CONFIG_TYPE_U32) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = sizeof(uint32_t);
    return config_store_get(key, NULL, value, &size, NULL, ns->ns_id);
}

config_status_t config_ns_set_str(config_ns_handle_t ns, const char* key,
                                  const char* value) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || !ns->valid || key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!config_namespace_is_valid_id(ns->ns_id)) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    size_t len = strlen(value) + 1;
    return config_store_set(key, CONFIG_TYPE_STRING, value, len,
                            CONFIG_FLAG_NONE, ns->ns_id);
}

config_status_t config_ns_get_str(config_ns_handle_t ns, const char* key,
                                  char* buffer, size_t buf_size) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || !ns->valid || key == NULL || buffer == NULL ||
        buf_size == 0) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!config_namespace_is_valid_id(ns->ns_id)) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    config_type_t type;
    config_status_t status = config_store_get_type(key, ns->ns_id, &type);

    if (status != CONFIG_OK) {
        return status;
    }

    if (type != CONFIG_TYPE_STRING) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = buf_size;
    status = config_store_get(key, NULL, buffer, &size, NULL, ns->ns_id);

    if (status != CONFIG_OK) {
        return status;
    }

    buffer[buf_size - 1] = '\0';
    return CONFIG_OK;
}

config_status_t config_ns_set_bool(config_ns_handle_t ns, const char* key,
                                   bool value) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || !ns->valid || key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!config_namespace_is_valid_id(ns->ns_id)) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    return config_store_set(key, CONFIG_TYPE_BOOL, &value, sizeof(bool),
                            CONFIG_FLAG_NONE, ns->ns_id);
}

config_status_t config_ns_get_bool(config_ns_handle_t ns, const char* key,
                                   bool* value, bool default_val) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || !ns->valid || key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!config_namespace_is_valid_id(ns->ns_id)) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    config_type_t type;
    config_status_t status = config_store_get_type(key, ns->ns_id, &type);

    if (status == CONFIG_ERROR_NOT_FOUND) {
        *value = default_val;
        return CONFIG_OK;
    }

    if (status != CONFIG_OK) {
        return status;
    }

    if (type != CONFIG_TYPE_BOOL) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = sizeof(bool);
    return config_store_get(key, NULL, value, &size, NULL, ns->ns_id);
}

config_status_t config_ns_exists(config_ns_handle_t ns, const char* key,
                                 bool* exists) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || !ns->valid || key == NULL || exists == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!config_namespace_is_valid_id(ns->ns_id)) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    return config_store_exists(key, ns->ns_id, exists);
}

config_status_t config_ns_delete(config_ns_handle_t ns, const char* key) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || !ns->valid || key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!config_namespace_is_valid_id(ns->ns_id)) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    return config_store_delete(key, ns->ns_id);
}

/*---------------------------------------------------------------------------*/
/* Namespace Handle Helper Functions                                         */
/*---------------------------------------------------------------------------*/

config_status_t
config_namespace_get_handle_id(const struct config_namespace* handle,
                               uint8_t* ns_id) {
    if (!g_ns_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (handle == NULL || ns_id == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (!handle->valid) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    *ns_id = handle->ns_id;
    return CONFIG_OK;
}

bool config_namespace_is_valid_handle(const struct config_namespace* handle) {
    if (!g_ns_ctx.initialized) {
        return false;
    }

    if (handle == NULL) {
        return false;
    }

    return handle->valid && config_namespace_is_valid_id(handle->ns_id);
}
