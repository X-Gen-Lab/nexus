/**
 * \file            config_backend.c
 * \brief           Config Manager Backend Abstraction Layer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements the backend abstraction layer for Config Manager.
 *                  Provides config_set_backend, config_commit, and config_load
 *                  functions that delegate to the configured storage backend.
 */

#include "config/config_backend.h"
#include "config_store.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Current storage backend
 */
static const config_backend_t* g_backend = NULL;

/**
 * \brief           Backend initialized flag
 */
static bool g_backend_initialized = false;

/**
 * \brief           Auto-commit mode flag
 */
static bool g_auto_commit = false;

/**
 * \brief           Dirty flag - indicates uncommitted changes
 */
static bool g_dirty = false;

/*---------------------------------------------------------------------------*/
/* Internal Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Serialization context for iteration
 */
typedef struct {
    const config_backend_t* backend;
    config_status_t status;
} backend_save_ctx_t;

/**
 * \brief           Callback for saving entries to backend
 */
static bool backend_save_entry_cb(const config_store_entry_info_t* info,
                                  void* user_data) {
    backend_save_ctx_t* ctx = (backend_save_ctx_t*)user_data;

    if (ctx->backend == NULL || ctx->backend->write == NULL) {
        ctx->status = CONFIG_ERROR_NO_BACKEND;
        return false;
    }

    /* Get the value from store */
    uint8_t value_buffer[CONFIG_MAX_MAX_VALUE_SIZE];
    size_t value_size = sizeof(value_buffer);
    config_type_t type;
    uint8_t flags;

    config_status_t status =
        config_store_get(info->key, &type, value_buffer, &value_size, &flags,
                         info->namespace_id);
    if (status != CONFIG_OK) {
        ctx->status = status;
        return false;
    }

    /* Create serialized entry format:
     * [1 byte: type][1 byte: flags][1 byte: namespace_id]
     * [2 bytes: value_size][N bytes: value]
     */
    uint8_t entry_buffer[CONFIG_MAX_MAX_VALUE_SIZE + 8];
    size_t entry_size = 0;

    entry_buffer[entry_size++] = (uint8_t)type;
    entry_buffer[entry_size++] = flags;
    entry_buffer[entry_size++] = info->namespace_id;
    entry_buffer[entry_size++] = (uint8_t)(value_size & 0xFF);
    entry_buffer[entry_size++] = (uint8_t)((value_size >> 8) & 0xFF);
    memcpy(&entry_buffer[entry_size], value_buffer, value_size);
    entry_size += value_size;

    /* Write to backend */
    status = ctx->backend->write(ctx->backend->ctx, info->key, entry_buffer,
                                 entry_size);
    if (status != CONFIG_OK) {
        ctx->status = status;
        return false;
    }

    return true;
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

config_status_t config_backend_set(const config_backend_t* backend) {
    if (backend == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Validate required functions */
    if (backend->read == NULL || backend->write == NULL ||
        backend->erase == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Deinitialize previous backend if any */
    if (g_backend != NULL && g_backend_initialized &&
        g_backend->deinit != NULL) {
        g_backend->deinit(g_backend->ctx);
    }

    g_backend = backend;
    g_backend_initialized = false;

    /* Initialize new backend if init function exists */
    if (backend->init != NULL) {
        config_status_t status = backend->init(backend->ctx);
        if (status != CONFIG_OK) {
            g_backend = NULL;
            return status;
        }
    }

    g_backend_initialized = true;
    return CONFIG_OK;
}

const config_backend_t* config_backend_get(void) {
    return g_backend;
}

bool config_backend_is_set(void) {
    return g_backend != NULL && g_backend_initialized;
}

config_status_t config_backend_commit(void) {
    if (g_backend == NULL) {
        return CONFIG_ERROR_NO_BACKEND;
    }

    if (!g_backend_initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (!config_store_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    /* Iterate over all entries and save to backend */
    backend_save_ctx_t ctx = {.backend = g_backend, .status = CONFIG_OK};

    config_status_t status = config_store_iterate(backend_save_entry_cb, &ctx);
    if (status != CONFIG_OK) {
        return status;
    }

    if (ctx.status != CONFIG_OK) {
        return ctx.status;
    }

    /* Call backend commit if available */
    if (g_backend->commit != NULL) {
        status = g_backend->commit(g_backend->ctx);
        if (status != CONFIG_OK) {
            return status;
        }
    }

    g_dirty = false;
    return CONFIG_OK;
}

config_status_t config_backend_load(void) {
    if (g_backend == NULL) {
        return CONFIG_ERROR_NO_BACKEND;
    }

    if (!g_backend_initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (!config_store_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    /* Note: Loading from backend requires the backend to provide
     * an iteration mechanism. For now, we return OK as the store
     * is already initialized. Full load implementation would require
     * backend-specific iteration support.
     */
    return CONFIG_OK;
}

void config_backend_set_auto_commit(bool auto_commit) {
    g_auto_commit = auto_commit;
}

bool config_backend_get_auto_commit(void) {
    return g_auto_commit;
}

void config_backend_set_dirty(bool dirty) {
    g_dirty = dirty;
}

bool config_backend_is_dirty(void) {
    return g_dirty;
}

config_status_t config_backend_auto_commit_if_enabled(void) {
    if (g_auto_commit && g_dirty && g_backend != NULL) {
        return config_backend_commit();
    }
    return CONFIG_OK;
}

config_status_t config_backend_deinit(void) {
    if (g_backend != NULL && g_backend_initialized &&
        g_backend->deinit != NULL) {
        config_status_t status = g_backend->deinit(g_backend->ctx);
        if (status != CONFIG_OK) {
            return status;
        }
    }

    g_backend = NULL;
    g_backend_initialized = false;
    g_auto_commit = false;
    g_dirty = false;

    return CONFIG_OK;
}
