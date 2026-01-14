/**
 * \file            config_mock_backend.c
 * \brief           Config Manager Mock Backend Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements a mock storage backend for Config Manager
 * testing. This backend supports error injection and operation tracking for
 * comprehensive unit and integration testing.
 */

#include "config/config_backend.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Maximum number of entries in Mock backend
 */
#ifndef CONFIG_MOCK_BACKEND_MAX_ENTRIES
#define CONFIG_MOCK_BACKEND_MAX_ENTRIES 64
#endif

/**
 * \brief           Maximum key length in Mock backend
 */
#ifndef CONFIG_MOCK_BACKEND_MAX_KEY_LEN
#define CONFIG_MOCK_BACKEND_MAX_KEY_LEN 64
#endif

/**
 * \brief           Maximum value size in Mock backend
 */
#ifndef CONFIG_MOCK_BACKEND_MAX_VALUE_SIZE
#define CONFIG_MOCK_BACKEND_MAX_VALUE_SIZE 256
#endif

/*---------------------------------------------------------------------------*/
/* Internal Structures                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mock backend entry structure
 */
typedef struct {
    char key[CONFIG_MOCK_BACKEND_MAX_KEY_LEN];
    uint8_t data[CONFIG_MOCK_BACKEND_MAX_VALUE_SIZE];
    size_t size;
    bool in_use;
} mock_backend_entry_t;

/**
 * \brief           Error injection configuration
 */
typedef struct {
    bool inject_read_error;
    bool inject_write_error;
    bool inject_erase_error;
    bool inject_commit_error;
    config_status_t read_error_code;
    config_status_t write_error_code;
    config_status_t erase_error_code;
    config_status_t commit_error_code;
    int read_fail_after_count;
    int write_fail_after_count;
    int erase_fail_after_count;
} mock_error_injection_t;

/**
 * \brief           Operation statistics
 */
typedef struct {
    uint32_t init_count;
    uint32_t deinit_count;
    uint32_t read_count;
    uint32_t write_count;
    uint32_t erase_count;
    uint32_t erase_all_count;
    uint32_t commit_count;
} mock_stats_t;

/**
 * \brief           Mock backend context structure
 */
typedef struct {
    bool initialized;
    mock_backend_entry_t entries[CONFIG_MOCK_BACKEND_MAX_ENTRIES];
    size_t entry_count;
    mock_error_injection_t error_injection;
    mock_stats_t stats;
} mock_backend_ctx_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Global Mock backend context
 */
static mock_backend_ctx_t g_mock_ctx;

/*---------------------------------------------------------------------------*/
/* Internal Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find entry by key
 * \param[in]       key: Key to find
 * \return          Pointer to entry if found, NULL otherwise
 */
static mock_backend_entry_t* mock_backend_find_entry(const char* key) {
    if (key == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < CONFIG_MOCK_BACKEND_MAX_ENTRIES; ++i) {
        if (g_mock_ctx.entries[i].in_use &&
            strncmp(g_mock_ctx.entries[i].key, key,
                    CONFIG_MOCK_BACKEND_MAX_KEY_LEN) == 0) {
            return &g_mock_ctx.entries[i];
        }
    }
    return NULL;
}

/**
 * \brief           Find free entry slot
 * \return          Pointer to free entry if available, NULL otherwise
 */
static mock_backend_entry_t* mock_backend_find_free_entry(void) {
    for (size_t i = 0; i < CONFIG_MOCK_BACKEND_MAX_ENTRIES; ++i) {
        if (!g_mock_ctx.entries[i].in_use) {
            return &g_mock_ctx.entries[i];
        }
    }
    return NULL;
}

/*---------------------------------------------------------------------------*/
/* Backend Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize Mock backend
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success
 */
static config_status_t mock_backend_init(void* ctx) {
    (void)ctx;

    /* Don't reset entries - allow pre-populated data for testing */
    g_mock_ctx.initialized = true;
    g_mock_ctx.stats.init_count++;

    return CONFIG_OK;
}

/**
 * \brief           Deinitialize Mock backend
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success
 */
static config_status_t mock_backend_deinit(void* ctx) {
    (void)ctx;

    g_mock_ctx.initialized = false;
    g_mock_ctx.stats.deinit_count++;

    return CONFIG_OK;
}

/**
 * \brief           Read data from Mock backend
 * \param[in]       ctx: Backend context (unused)
 * \param[in]       key: Key to read
 * \param[out]      data: Buffer to store data
 * \param[in,out]   size: Input: buffer size, Output: actual data size
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t mock_backend_read(void* ctx, const char* key, void* data,
                                         size_t* size) {
    (void)ctx;

    g_mock_ctx.stats.read_count++;

    /* Check for error injection */
    if (g_mock_ctx.error_injection.inject_read_error) {
        if (g_mock_ctx.error_injection.read_fail_after_count <= 0 ||
            (int)g_mock_ctx.stats.read_count >
                g_mock_ctx.error_injection.read_fail_after_count) {
            return g_mock_ctx.error_injection.read_error_code;
        }
    }

    if (!g_mock_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || size == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    mock_backend_entry_t* entry = mock_backend_find_entry(key);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Check buffer size */
    if (data != NULL && *size < entry->size) {
        *size = entry->size;
        return CONFIG_ERROR_BUFFER_TOO_SMALL;
    }

    /* Copy data */
    if (data != NULL) {
        memcpy(data, entry->data, entry->size);
    }
    *size = entry->size;

    return CONFIG_OK;
}

/**
 * \brief           Write data to Mock backend
 * \param[in]       ctx: Backend context (unused)
 * \param[in]       key: Key to write
 * \param[in]       data: Data to write
 * \param[in]       size: Data size
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t mock_backend_write(void* ctx, const char* key,
                                          const void* data, size_t size) {
    (void)ctx;

    g_mock_ctx.stats.write_count++;

    /* Check for error injection */
    if (g_mock_ctx.error_injection.inject_write_error) {
        if (g_mock_ctx.error_injection.write_fail_after_count <= 0 ||
            (int)g_mock_ctx.stats.write_count >
                g_mock_ctx.error_injection.write_fail_after_count) {
            return g_mock_ctx.error_injection.write_error_code;
        }
    }

    if (!g_mock_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || data == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    size_t key_len = strlen(key);
    if (key_len == 0 || key_len >= CONFIG_MOCK_BACKEND_MAX_KEY_LEN) {
        return CONFIG_ERROR_KEY_TOO_LONG;
    }

    if (size > CONFIG_MOCK_BACKEND_MAX_VALUE_SIZE) {
        return CONFIG_ERROR_VALUE_TOO_LARGE;
    }

    /* Find existing entry or allocate new one */
    mock_backend_entry_t* entry = mock_backend_find_entry(key);

    if (entry == NULL) {
        entry = mock_backend_find_free_entry();
        if (entry == NULL) {
            return CONFIG_ERROR_NO_SPACE;
        }
        g_mock_ctx.entry_count++;
    }

    /* Store the entry */
    memset(entry->key, 0, sizeof(entry->key));
    size_t copy_len = key_len < (CONFIG_MOCK_BACKEND_MAX_KEY_LEN - 1)
                          ? key_len
                          : (CONFIG_MOCK_BACKEND_MAX_KEY_LEN - 1);
    memcpy(entry->key, key, copy_len);
    entry->key[copy_len] = '\0';

    memcpy(entry->data, data, size);
    entry->size = size;
    entry->in_use = true;

    return CONFIG_OK;
}

/**
 * \brief           Erase data from Mock backend
 * \param[in]       ctx: Backend context (unused)
 * \param[in]       key: Key to erase
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t mock_backend_erase(void* ctx, const char* key) {
    (void)ctx;

    g_mock_ctx.stats.erase_count++;

    /* Check for error injection */
    if (g_mock_ctx.error_injection.inject_erase_error) {
        if (g_mock_ctx.error_injection.erase_fail_after_count <= 0 ||
            (int)g_mock_ctx.stats.erase_count >
                g_mock_ctx.error_injection.erase_fail_after_count) {
            return g_mock_ctx.error_injection.erase_error_code;
        }
    }

    if (!g_mock_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    mock_backend_entry_t* entry = mock_backend_find_entry(key);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Clear the entry */
    memset(entry, 0, sizeof(*entry));
    entry->in_use = false;
    g_mock_ctx.entry_count--;

    return CONFIG_OK;
}

/**
 * \brief           Erase all data from Mock backend
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success
 */
static config_status_t mock_backend_erase_all(void* ctx) {
    (void)ctx;

    g_mock_ctx.stats.erase_all_count++;

    if (!g_mock_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    for (size_t i = 0; i < CONFIG_MOCK_BACKEND_MAX_ENTRIES; ++i) {
        memset(&g_mock_ctx.entries[i], 0, sizeof(g_mock_ctx.entries[i]));
    }
    g_mock_ctx.entry_count = 0;

    return CONFIG_OK;
}

/**
 * \brief           Commit changes (mock implementation)
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success, or injected error
 */
static config_status_t mock_backend_commit(void* ctx) {
    (void)ctx;

    g_mock_ctx.stats.commit_count++;

    /* Check for error injection */
    if (g_mock_ctx.error_injection.inject_commit_error) {
        return g_mock_ctx.error_injection.commit_error_code;
    }

    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Backend Instance                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mock backend instance
 */
static const config_backend_t g_mock_backend = {.name = "mock",
                                                .init = mock_backend_init,
                                                .deinit = mock_backend_deinit,
                                                .read = mock_backend_read,
                                                .write = mock_backend_write,
                                                .erase = mock_backend_erase,
                                                .erase_all =
                                                    mock_backend_erase_all,
                                                .commit = mock_backend_commit,
                                                .ctx = NULL};

/*---------------------------------------------------------------------------*/
/* Public API                                                                */
/*---------------------------------------------------------------------------*/

const config_backend_t* config_backend_mock_get(void) {
    return &g_mock_backend;
}

void config_backend_mock_reset(void) {
    memset(&g_mock_ctx, 0, sizeof(g_mock_ctx));
}
