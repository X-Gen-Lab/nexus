/**
 * \file            log.c
 * \brief           Log Framework Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "log/log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Include OSAL for async mode */
#include "osal/osal.h"

/*---------------------------------------------------------------------------*/
/* Internal State                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async log entry structure for queue
 */
typedef struct {
    log_level_t level;                 /**< Log level */
    char message[LOG_MAX_MSG_LEN * 2]; /**< Formatted message */
    size_t length;                     /**< Message length */
} log_async_entry_t;

/**
 * \brief           Async state structure
 */
typedef struct {
    osal_queue_handle_t queue;     /**< Message queue handle */
    osal_task_handle_t task;       /**< Background task handle */
    osal_mutex_handle_t mutex;     /**< Mutex for thread safety */
    volatile bool running;         /**< Task running flag */
    volatile bool flush_requested; /**< Flush request flag */
    volatile size_t pending_count; /**< Number of pending messages */
    log_async_policy_t policy;     /**< Buffer full policy */
    size_t queue_size;             /**< Queue size */
} log_async_state_t;

/**
 * \brief           Log system internal state
 */
typedef struct {
    bool initialized;   /**< Initialization flag */
    log_level_t level;  /**< Global log level */
    const char* format; /**< Format pattern */
    bool async_mode;    /**< Async mode enabled */
    size_t buffer_size; /**< Async buffer size */
    size_t max_msg_len; /**< Maximum message length */
    bool color_enabled; /**< Color output enabled */
} log_state_t;

/**
 * \brief           Default log state
 */
static log_state_t s_log_state = {.initialized = false,
                                  .level = LOG_DEFAULT_LEVEL,
                                  .format = LOG_DEFAULT_FORMAT,
                                  .async_mode = false,
                                  .buffer_size = LOG_ASYNC_BUFFER_SIZE,
                                  .max_msg_len = LOG_MAX_MSG_LEN,
                                  .color_enabled = false};

/**
 * \brief           Async state (initialized when async mode enabled)
 */
static log_async_state_t s_async_state = {.queue = NULL,
                                          .task = NULL,
                                          .mutex = NULL,
                                          .running = false,
                                          .flush_requested = false,
                                          .pending_count = 0,
                                          .policy =
                                              LOG_ASYNC_POLICY_DROP_OLDEST,
                                          .queue_size = LOG_ASYNC_QUEUE_SIZE};

/*---------------------------------------------------------------------------*/
/* Thread Safety State                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Global mutex for thread-safe synchronous logging
 * \details         Protects shared state during log operations.
 *                  Requirements: 6.1, 6.3, 6.5
 */
static osal_mutex_handle_t s_log_mutex = NULL;

/**
 * \brief           Flag indicating if thread safety is enabled
 */
static bool s_thread_safe_enabled = false;

/*---------------------------------------------------------------------------*/
/* Backend Management State                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Registered backends array
 */
static log_backend_t* s_backends[LOG_MAX_BACKENDS] = {NULL};

/**
 * \brief           Number of registered backends
 */
static size_t s_backend_count = 0;

/*---------------------------------------------------------------------------*/
/* Module-Level Filtering State                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Module filter entry structure
 */
typedef struct {
    char pattern[LOG_MODULE_NAME_LEN]; /**< Module name or wildcard pattern */
    log_level_t level;                 /**< Log level for this module */
    bool active;                       /**< Whether this entry is in use */
} log_module_filter_t;

/**
 * \brief           Module filters array
 */
static log_module_filter_t s_module_filters[LOG_MAX_MODULE_FILTERS] = {{0}};

/**
 * \brief           Number of active module filters
 */
static size_t s_module_filter_count = 0;

/*---------------------------------------------------------------------------*/
/* Backend Context Type Definitions                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Console backend context
 */
typedef struct {
    bool initialized;
} console_backend_ctx_t;

/**
 * \brief           Memory backend context (ring buffer)
 */
typedef struct {
    char* buffer;     /**< Ring buffer storage */
    size_t size;      /**< Total buffer size */
    size_t head;      /**< Write position */
    size_t tail;      /**< Read position */
    size_t count;     /**< Number of bytes in buffer */
    bool initialized; /**< Initialization flag */
} mem_backend_ctx_t;

/*---------------------------------------------------------------------------*/
/* Static Allocation Pools (when LOG_USE_STATIC_ALLOC is enabled)            */
/* Requirements: 7.4                                                         */
/*---------------------------------------------------------------------------*/

#if LOG_USE_STATIC_ALLOC

/**
 * \brief           Static backend structure pool
 */
static log_backend_t s_static_backends[LOG_STATIC_BACKEND_COUNT];

/**
 * \brief           Static backend allocation flags
 */
static bool s_static_backend_used[LOG_STATIC_BACKEND_COUNT] = {false};

/**
 * \brief           Static console backend context
 */
static console_backend_ctx_t s_static_console_ctx;
static bool s_static_console_ctx_used = false;

/**
 * \brief           Static memory backend context and buffer
 */
static mem_backend_ctx_t s_static_mem_ctx;
static char s_static_mem_buffer[LOG_STATIC_MEMORY_BUFFER_SIZE];
static bool s_static_mem_ctx_used = false;

/**
 * \brief           Allocate a backend from static pool
 * \return          Pointer to backend, or NULL if pool exhausted
 */
static log_backend_t* log_static_alloc_backend(void) {
    for (size_t i = 0; i < LOG_STATIC_BACKEND_COUNT; ++i) {
        if (!s_static_backend_used[i]) {
            s_static_backend_used[i] = true;
            memset(&s_static_backends[i], 0, sizeof(log_backend_t));
            return &s_static_backends[i];
        }
    }
    return NULL;
}

/**
 * \brief           Free a backend back to static pool
 * \param[in]       backend: Backend to free
 */
static void log_static_free_backend(log_backend_t* backend) {
    for (size_t i = 0; i < LOG_STATIC_BACKEND_COUNT; ++i) {
        if (&s_static_backends[i] == backend) {
            s_static_backend_used[i] = false;
            return;
        }
    }
}

#endif /* LOG_USE_STATIC_ALLOC */

/*---------------------------------------------------------------------------*/
/* Level Name Tables                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Full level names
 */
static const char* const s_level_names[] = {"TRACE", "DEBUG", "INFO", "WARN",
                                            "ERROR", "FATAL", "NONE"};

/**
 * \brief           Short level names (single character)
 */
static const char s_level_short[] = {'T', 'D', 'I', 'W', 'E', 'F', 'N'};

/**
 * \brief           ANSI color codes for each level
 */
static const char* const s_level_colors[] = {
    "\033[37m", /* TRACE - white */
    "\033[36m", /* DEBUG - cyan */
    "\033[32m", /* INFO - green */
    "\033[33m", /* WARN - yellow */
    "\033[31m", /* ERROR - red */
    "\033[35m", /* FATAL - magenta */
    "\033[0m"   /* NONE - reset */
};

/**
 * \brief           ANSI color reset code
 */
static const char* const s_color_reset = "\033[0m";

/*---------------------------------------------------------------------------*/
/* Formatting Helper Functions                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get level name string
 * \param[in]       level: Log level
 * \return          Level name string
 */
static const char* log_level_name(log_level_t level) {
    if (level > LOG_LEVEL_NONE) {
        return "UNKNOWN";
    }
    return s_level_names[level];
}

/**
 * \brief           Get short level name (single char)
 * \param[in]       level: Log level
 * \return          Short level character
 */
static char log_level_short(log_level_t level) {
    if (level > LOG_LEVEL_NONE) {
        return '?';
    }
    return s_level_short[level];
}

/**
 * \brief           Get color code for level
 * \param[in]       level: Log level
 * \return          ANSI color code string
 */
static const char* log_level_color(log_level_t level) {
    if (level > LOG_LEVEL_NONE) {
        return s_color_reset;
    }
    return s_level_colors[level];
}

/**
 * \brief           Get current timestamp in milliseconds
 * \return          Timestamp in milliseconds
 */
static uint32_t log_get_timestamp_ms(void) {
    /* Use clock() for portable timestamp */
    return (uint32_t)((clock() * 1000) / CLOCKS_PER_SEC);
}

/**
 * \brief           Extract filename from full path
 * \param[in]       path: Full file path
 * \return          Pointer to filename portion
 */
static const char* log_extract_filename(const char* path) {
    if (path == NULL) {
        return "unknown";
    }

    const char* filename = path;
    const char* p = path;

    while (*p != '\0') {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
        p++;
    }

    return filename;
}

/**
 * \brief           Format message using pattern
 * \param[out]      buf: Output buffer
 * \param[in]       buf_size: Buffer size
 * \param[in]       level: Log level
 * \param[in]       module: Module name
 * \param[in]       file: Source file
 * \param[in]       line: Source line
 * \param[in]       func: Function name
 * \param[in]       user_msg: User message (already formatted)
 * \return          Number of characters written (excluding null terminator)
 */
static size_t log_format_with_pattern(char* buf, size_t buf_size,
                                      log_level_t level, const char* module,
                                      const char* file, int line,
                                      const char* func, const char* user_msg) {
    if (buf == NULL || buf_size == 0) {
        return 0;
    }

    const char* pattern = s_log_state.format;
    if (pattern == NULL) {
        pattern = LOG_DEFAULT_FORMAT;
    }

    size_t pos = 0;
    const char* p = pattern;

    while (*p != '\0' && pos < buf_size - 1) {
        if (*p == '%' && *(p + 1) != '\0') {
            char token = *(p + 1);
            int written = 0;

            switch (token) {
                case 'T': /* Timestamp in milliseconds */
                    written = snprintf(buf + pos, buf_size - pos, "%u",
                                       log_get_timestamp_ms());
                    break;

                case 't': /* Time in HH:MM:SS format */
                {
                    time_t now = time(NULL);
                    struct tm* tm_info = localtime(&now);
                    if (tm_info != NULL) {
                        written = snprintf(buf + pos, buf_size - pos,
                                           "%02d:%02d:%02d", tm_info->tm_hour,
                                           tm_info->tm_min, tm_info->tm_sec);
                    }
                } break;

                case 'L': /* Level name (full) */
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       log_level_name(level));
                    break;

                case 'l': /* Level name (short) */
                    if (pos < buf_size - 1) {
                        buf[pos] = log_level_short(level);
                        written = 1;
                    }
                    break;

                case 'M': /* Module name */
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       module ? module : "default");
                    break;

                case 'F': /* File name */
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       log_extract_filename(file));
                    break;

                case 'f': /* Function name */
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       func ? func : "unknown");
                    break;

                case 'n': /* Line number */
                    written = snprintf(buf + pos, buf_size - pos, "%d", line);
                    break;

                case 'm': /* Message */
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       user_msg ? user_msg : "");
                    break;

                case 'c': /* Color code */
                    if (s_log_state.color_enabled) {
                        written = snprintf(buf + pos, buf_size - pos, "%s",
                                           log_level_color(level));
                    }
                    break;

                case 'C': /* Color reset */
                    if (s_log_state.color_enabled) {
                        written = snprintf(buf + pos, buf_size - pos, "%s",
                                           s_color_reset);
                    }
                    break;

                case '%': /* Literal percent */
                    if (pos < buf_size - 1) {
                        buf[pos] = '%';
                        written = 1;
                    }
                    break;

                default: /* Unknown token, copy as-is */
                    if (pos < buf_size - 2) {
                        buf[pos] = '%';
                        buf[pos + 1] = token;
                        written = 2;
                    }
                    break;
            }

            if (written > 0) {
                pos += (size_t)written;
            }
            p += 2; /* Skip % and token */
        } else {
            /* Copy regular character */
            buf[pos++] = *p++;
        }
    }

    /* Null terminate */
    buf[pos] = '\0';

    return pos;
}

/**
 * \brief           Format user message with printf-style arguments
 * \param[out]      buf: Output buffer
 * \param[in]       buf_size: Buffer size
 * \param[in]       fmt: Format string
 * \param[in]       args: Variable arguments
 * \return          Number of characters written (excluding null terminator)
 */
static size_t log_format_user_message(char* buf, size_t buf_size,
                                      const char* fmt, va_list args) {
    if (buf == NULL || buf_size == 0 || fmt == NULL) {
        return 0;
    }

    int written = vsnprintf(buf, buf_size, fmt, args);

    if (written < 0) {
        buf[0] = '\0';
        return 0;
    }

    return (size_t)written < buf_size ? (size_t)written : buf_size - 1;
}

/**
 * \brief           Apply message truncation if needed
 * \param[in,out]   buf: Message buffer
 * \param[in]       buf_size: Buffer size
 * \param[in]       max_len: Maximum message length
 * \return          Final message length
 */
static size_t log_apply_truncation(char* buf, size_t buf_size, size_t max_len) {
    if (buf == NULL || buf_size == 0) {
        return 0;
    }

    size_t len = strlen(buf);

    /* If max_len is 0, use buf_size as limit */
    if (max_len == 0) {
        max_len = buf_size;
    }

    /* Check if truncation is needed */
    if (len > max_len && max_len > 3) {
        /* Truncate and add "..." indicator */
        buf[max_len - 3] = '.';
        buf[max_len - 2] = '.';
        buf[max_len - 1] = '.';
        buf[max_len] = '\0';
        return max_len;
    }

    return len;
}

/*---------------------------------------------------------------------------*/
/* Forward Declarations for Async Functions                                  */
/*---------------------------------------------------------------------------*/

static log_status_t log_async_init_internal(size_t queue_size,
                                            log_async_policy_t policy);
static log_status_t log_async_deinit_internal(void);
static log_status_t log_async_queue_message(const char* msg, size_t len,
                                            log_level_t level);

/*---------------------------------------------------------------------------*/
/* Thread Safety Helper Functions                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize thread safety mutex
 * \return          LOG_OK on success, error code otherwise
 * \details         Requirements: 6.1, 6.3
 */
static log_status_t log_mutex_init(void) {
    if (s_log_mutex != NULL) {
        return LOG_OK; /* Already initialized */
    }

    osal_status_t status = osal_mutex_create(&s_log_mutex);
    if (status != OSAL_OK) {
        return LOG_ERROR_NO_MEMORY;
    }

    s_thread_safe_enabled = true;
    return LOG_OK;
}

/**
 * \brief           Deinitialize thread safety mutex
 * \return          LOG_OK on success, error code otherwise
 */
static log_status_t log_mutex_deinit(void) {
    if (s_log_mutex == NULL) {
        return LOG_OK; /* Already deinitialized */
    }

    osal_status_t status = osal_mutex_delete(s_log_mutex);
    s_log_mutex = NULL;
    s_thread_safe_enabled = false;

    return (status == OSAL_OK) ? LOG_OK : LOG_ERROR;
}

/**
 * \brief           Lock the log mutex for thread-safe access
 * \details         Requirements: 6.1, 6.3, 6.5 - Minimizes lock hold time
 *                  by only locking when necessary and using timeout.
 *                  If in ISR context, uses critical section instead.
 */
static void log_lock(void) {
    if (!s_thread_safe_enabled || s_log_mutex == NULL) {
        return;
    }

    /* Check if in ISR context - use critical section instead */
    if (osal_is_isr()) {
        osal_enter_critical();
        return;
    }

    /* Lock with timeout to prevent deadlock */
    osal_mutex_lock(s_log_mutex, OSAL_WAIT_FOREVER);
}

/**
 * \brief           Unlock the log mutex
 * \details         Requirements: 6.5 - Releases lock as soon as possible
 */
static void log_unlock(void) {
    if (!s_thread_safe_enabled || s_log_mutex == NULL) {
        return;
    }

    /* Check if in ISR context - exit critical section */
    if (osal_is_isr()) {
        osal_exit_critical();
        return;
    }

    osal_mutex_unlock(s_log_mutex);
}

/*---------------------------------------------------------------------------*/
/* Initialization and Configuration                                          */
/*---------------------------------------------------------------------------*/

log_status_t log_init(const log_config_t* config) {
    /* Check if already initialized */
    if (s_log_state.initialized) {
        return LOG_ERROR_ALREADY_INIT;
    }

    /* Initialize thread safety mutex first */
    log_status_t mutex_status = log_mutex_init();
    if (mutex_status != LOG_OK) {
        return mutex_status;
    }

    /* Apply configuration or use defaults */
    if (config != NULL) {
        /* Validate level */
        if (config->level > LOG_LEVEL_NONE) {
            log_mutex_deinit();
            return LOG_ERROR_INVALID_PARAM;
        }

        s_log_state.level = config->level;
        s_log_state.format =
            (config->format != NULL) ? config->format : LOG_DEFAULT_FORMAT;
        s_log_state.async_mode = config->async_mode;
        s_log_state.buffer_size = (config->buffer_size > 0)
                                      ? config->buffer_size
                                      : LOG_ASYNC_BUFFER_SIZE;
        s_log_state.max_msg_len =
            (config->max_msg_len > 0) ? config->max_msg_len : LOG_MAX_MSG_LEN;
        s_log_state.color_enabled = config->color_enabled;

        /* Initialize async mode if requested */
        if (config->async_mode) {
            size_t queue_size = (config->async_queue_size > 0)
                                    ? config->async_queue_size
                                    : LOG_ASYNC_QUEUE_SIZE;
            log_status_t async_status =
                log_async_init_internal(queue_size, config->async_policy);
            if (async_status != LOG_OK) {
                log_mutex_deinit();
                return async_status;
            }
        }
    } else {
        /* Use default configuration */
        s_log_state.level = LOG_DEFAULT_LEVEL;
        s_log_state.format = LOG_DEFAULT_FORMAT;
        s_log_state.async_mode = false;
        s_log_state.buffer_size = LOG_ASYNC_BUFFER_SIZE;
        s_log_state.max_msg_len = LOG_MAX_MSG_LEN;
        s_log_state.color_enabled = false;
    }

    s_log_state.initialized = true;
    return LOG_OK;
}

log_status_t log_deinit(void) {
    /* Check if initialized */
    if (!s_log_state.initialized) {
        return LOG_ERROR_NOT_INIT;
    }

    /* Flush and deinitialize async subsystem if enabled */
    if (s_log_state.async_mode) {
        log_async_flush();
        log_async_deinit_internal();
    }

    /* Flush all backends */
    for (size_t i = 0; i < s_backend_count; ++i) {
        if (s_backends[i] != NULL && s_backends[i]->flush != NULL) {
            s_backends[i]->flush(s_backends[i]->ctx);
        }
    }

    /* Deinitialize all backends */
    for (size_t i = 0; i < s_backend_count; ++i) {
        if (s_backends[i] != NULL && s_backends[i]->deinit != NULL) {
            s_backends[i]->deinit(s_backends[i]->ctx);
        }
        s_backends[i] = NULL;
    }
    s_backend_count = 0;

    /* Clear all module filters */
    memset(s_module_filters, 0, sizeof(s_module_filters));
    s_module_filter_count = 0;

    /* Reset state to defaults */
    s_log_state.initialized = false;
    s_log_state.level = LOG_DEFAULT_LEVEL;
    s_log_state.format = LOG_DEFAULT_FORMAT;
    s_log_state.async_mode = false;
    s_log_state.buffer_size = LOG_ASYNC_BUFFER_SIZE;
    s_log_state.max_msg_len = LOG_MAX_MSG_LEN;
    s_log_state.color_enabled = false;

    /* Deinitialize thread safety mutex last */
    log_mutex_deinit();

    return LOG_OK;
}

bool log_is_initialized(void) {
    return s_log_state.initialized;
}

/*---------------------------------------------------------------------------*/
/* Level Management                                                          */
/*---------------------------------------------------------------------------*/

log_status_t log_set_level(log_level_t level) {
    /* Validate level */
    if (level > LOG_LEVEL_NONE) {
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Thread-safe level update */
    log_lock();
    s_log_state.level = level;
    log_unlock();

    return LOG_OK;
}

log_level_t log_get_level(void) {
    /* Thread-safe level read */
    log_lock();
    log_level_t level = s_log_state.level;
    log_unlock();

    return level;
}

/**
 * \brief           Check if a message at given level should be logged
 * \param[in]       level: Message level
 * \param[in]       module: Module name (for module-specific filtering)
 * \return          true if message should be logged, false otherwise
 */
static bool log_should_output(log_level_t level, const char* module) {
    /* If not initialized, don't log */
    if (!s_log_state.initialized) {
        return false;
    }

    /* Get effective level (module-specific or global) */
    log_level_t effective_level = log_module_get_level(module);

    /* Filter based on level */
    return (level >= effective_level);
}

/*---------------------------------------------------------------------------*/
/* Module-Level Filtering                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Check if a pattern matches a module name using wildcards
 * \param[in]       pattern: Pattern string (may contain '*' wildcard)
 * \param[in]       module: Module name to match
 * \return          true if pattern matches module, false otherwise
 *
 * Supports:
 * - Exact match: "hal.gpio" matches "hal.gpio"
 * - Prefix wildcard: "hal.*" matches "hal.gpio", "hal.uart", etc.
 * - Single wildcard: "*" matches everything
 */
static bool log_pattern_matches(const char* pattern, const char* module) {
    if (pattern == NULL || module == NULL) {
        return false;
    }

    const char* p = pattern;
    const char* m = module;

    while (*p != '\0' && *m != '\0') {
        if (*p == '*') {
            /* Wildcard found */
            p++;
            if (*p == '\0') {
                /* Pattern ends with '*', matches rest of module */
                return true;
            }
            /* Find next occurrence of character after '*' in module */
            while (*m != '\0') {
                if (log_pattern_matches(p, m)) {
                    return true;
                }
                m++;
            }
            return false;
        } else if (*p == *m) {
            /* Characters match, continue */
            p++;
            m++;
        } else {
            /* Characters don't match */
            return false;
        }
    }

    /* Check if both strings ended */
    if (*p == '\0' && *m == '\0') {
        return true;
    }

    /* Handle trailing '*' in pattern */
    if (*p == '*' && *(p + 1) == '\0') {
        return true;
    }

    return false;
}

/**
 * \brief           Find a module filter by exact pattern match
 * \param[in]       pattern: Pattern to find
 * \return          Index of filter, or -1 if not found
 */
static int log_find_module_filter(const char* pattern) {
    if (pattern == NULL) {
        return -1;
    }

    for (size_t i = 0; i < LOG_MAX_MODULE_FILTERS; ++i) {
        if (s_module_filters[i].active &&
            strcmp(s_module_filters[i].pattern, pattern) == 0) {
            return (int)i;
        }
    }

    return -1;
}

/**
 * \brief           Find an empty slot in the module filters array
 * \return          Index of empty slot, or -1 if full
 */
static int log_find_empty_filter_slot(void) {
    for (size_t i = 0; i < LOG_MAX_MODULE_FILTERS; ++i) {
        if (!s_module_filters[i].active) {
            return (int)i;
        }
    }
    return -1;
}

log_status_t log_module_set_level(const char* module, log_level_t level) {
    /* Validate parameters */
    if (module == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    if (level > LOG_LEVEL_NONE) {
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Check module name length */
    size_t module_len = strlen(module);
    if (module_len == 0 || module_len >= LOG_MODULE_NAME_LEN) {
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Lock for thread-safe module filter modification */
    log_lock();

    /* Check if filter for this pattern already exists */
    int existing_idx = log_find_module_filter(module);
    if (existing_idx >= 0) {
        /* Update existing filter */
        s_module_filters[existing_idx].level = level;
        log_unlock();
        return LOG_OK;
    }

    /* Find an empty slot */
    int empty_idx = log_find_empty_filter_slot();
    if (empty_idx < 0) {
        log_unlock();
        return LOG_ERROR_FULL;
    }

    /* Add new filter - use memcpy for safety */
    size_t copy_len = module_len < (LOG_MODULE_NAME_LEN - 1)
                          ? module_len
                          : (LOG_MODULE_NAME_LEN - 1);
    memcpy(s_module_filters[empty_idx].pattern, module, copy_len);
    s_module_filters[empty_idx].pattern[copy_len] = '\0';
    s_module_filters[empty_idx].level = level;
    s_module_filters[empty_idx].active = true;
    s_module_filter_count++;

    log_unlock();
    return LOG_OK;
}

log_level_t log_module_get_level(const char* module) {
    /* If module is NULL, return global level */
    if (module == NULL) {
        log_lock();
        log_level_t level = s_log_state.level;
        log_unlock();
        return level;
    }

    /* Lock for thread-safe module filter access */
    log_lock();

    /* First, try exact match */
    int exact_idx = log_find_module_filter(module);
    if (exact_idx >= 0) {
        log_level_t level = s_module_filters[exact_idx].level;
        log_unlock();
        return level;
    }

    /* Then, try wildcard patterns */
    /* We iterate through all filters and find the best (most specific) match */
    int best_match_idx = -1;
    size_t best_match_len = 0;

    for (size_t i = 0; i < LOG_MAX_MODULE_FILTERS; ++i) {
        if (!s_module_filters[i].active) {
            continue;
        }

        /* Check if this pattern matches the module */
        if (log_pattern_matches(s_module_filters[i].pattern, module)) {
            /* Calculate pattern specificity (longer patterns are more specific)
             */
            size_t pattern_len = strlen(s_module_filters[i].pattern);

            /* Prefer longer patterns (more specific) */
            if (best_match_idx < 0 || pattern_len > best_match_len) {
                best_match_idx = (int)i;
                best_match_len = pattern_len;
            }
        }
    }

    if (best_match_idx >= 0) {
        log_level_t level = s_module_filters[best_match_idx].level;
        log_unlock();
        return level;
    }

    /* No module-specific level found, return global level */
    log_level_t level = s_log_state.level;
    log_unlock();
    return level;
}

log_status_t log_module_clear_level(const char* module) {
    if (module == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Lock for thread-safe module filter modification */
    log_lock();

    int idx = log_find_module_filter(module);
    if (idx < 0) {
        log_unlock();
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Clear the filter */
    memset(&s_module_filters[idx], 0, sizeof(log_module_filter_t));
    s_module_filter_count--;

    log_unlock();
    return LOG_OK;
}

/**
 * \brief           Clear all module filters
 */
void log_module_clear_all(void) {
    log_lock();
    memset(s_module_filters, 0, sizeof(s_module_filters));
    s_module_filter_count = 0;
    log_unlock();
}

/*---------------------------------------------------------------------------*/
/* Format Configuration                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get current format pattern
 * \return          Current format pattern string
 */
const char* log_get_format(void) {
    return s_log_state.format ? s_log_state.format : LOG_DEFAULT_FORMAT;
}

log_status_t log_set_format(const char* pattern) {
    if (pattern == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Validate pattern - check for valid tokens */
    const char* p = pattern;
    while (*p != '\0') {
        if (*p == '%' && *(p + 1) != '\0') {
            char token = *(p + 1);
            /* Valid tokens: T, t, L, l, M, F, f, n, m, c, C, % */
            if (token != 'T' && token != 't' && token != 'L' && token != 'l' &&
                token != 'M' && token != 'F' && token != 'f' && token != 'n' &&
                token != 'm' && token != 'c' && token != 'C' && token != '%') {
                /* Unknown token - still accept but will be copied as-is */
            }
            p += 2;
        } else {
            p++;
        }
    }

    s_log_state.format = pattern;
    return LOG_OK;
}

log_status_t log_set_max_msg_len(size_t max_len) {
    if (max_len == 0) {
        s_log_state.max_msg_len = LOG_MAX_MSG_LEN;
    } else {
        s_log_state.max_msg_len = max_len;
    }
    return LOG_OK;
}

/**
 * \brief           Get maximum message length
 * \return          Current maximum message length
 */
size_t log_get_max_msg_len(void) {
    return s_log_state.max_msg_len;
}

/*---------------------------------------------------------------------------*/
/* Logging Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Output formatted message to all backends
 * \param[in]       msg: Formatted message
 * \param[in]       len: Message length
 * \param[in]       level: Log level of the message
 * \return          LOG_OK on success, error code otherwise
 */
static log_status_t log_output_to_backends(const char* msg, size_t len,
                                           log_level_t level) {
    if (msg == NULL || len == 0) {
        return LOG_ERROR_INVALID_PARAM;
    }

    log_status_t result = LOG_OK;
    bool any_success = false;

    /* Iterate through all registered backends */
    for (size_t i = 0; i < s_backend_count; ++i) {
        log_backend_t* backend = s_backends[i];
        if (backend == NULL) {
            continue;
        }

        /* Check if backend is enabled */
        if (!backend->enabled) {
            continue;
        }

        /* Check if message level meets backend's minimum level */
        if (level < backend->min_level) {
            continue;
        }

        /* Write to backend */
        if (backend->write != NULL) {
            log_status_t status = backend->write(backend->ctx, msg, len);
            if (status == LOG_OK) {
                any_success = true;
            }
            /* Continue with other backends even if one fails (isolation) */
        }
    }

    /* If no backends registered, still return OK */
    if (s_backend_count == 0) {
        return LOG_OK;
    }

    /* Return OK if at least one backend succeeded */
    return any_success ? LOG_OK : result;
}

log_status_t log_write(log_level_t level, const char* module, const char* file,
                       int line, const char* func, const char* fmt, ...) {
    /* Check if should output based on level filtering */
    if (!log_should_output(level, module)) {
        return LOG_OK; /* Silently discard filtered messages */
    }

    /* Format user message with printf-style arguments (no lock needed) */
    char user_msg[LOG_MAX_MSG_LEN];
    va_list args;
    va_start(args, fmt);
    size_t user_len =
        log_format_user_message(user_msg, sizeof(user_msg), fmt, args);
    va_end(args);

    /* Apply truncation to user message if needed (no lock needed) */
    user_len = log_apply_truncation(user_msg, sizeof(user_msg),
                                    s_log_state.max_msg_len);
    LOG_UNUSED(user_len);

    /* Format complete message with pattern (no lock needed) */
    char formatted_msg[LOG_MAX_MSG_LEN * 2]; /* Extra space for metadata */
    size_t formatted_len =
        log_format_with_pattern(formatted_msg, sizeof(formatted_msg), level,
                                module, file, line, func, user_msg);

    /* Add newline if not present */
    if (formatted_len > 0 && formatted_len < sizeof(formatted_msg) - 1 &&
        formatted_msg[formatted_len - 1] != '\n') {
        formatted_msg[formatted_len++] = '\n';
        formatted_msg[formatted_len] = '\0';
    }

    /* In async mode, queue the message for background processing */
    if (s_log_state.async_mode && s_async_state.queue != NULL) {
        return log_async_queue_message(formatted_msg, formatted_len, level);
    }

    /* Synchronous mode - lock only during backend output
     * Requirements: 6.1, 6.2, 6.5 - Thread-safe with minimal lock time */
    log_lock();
    log_status_t result =
        log_output_to_backends(formatted_msg, formatted_len, level);
    log_unlock();

    return result;
}

log_status_t log_write_raw(const char* msg, size_t len) {
    /* Check if initialized */
    if (!s_log_state.initialized) {
        return LOG_ERROR_NOT_INIT;
    }

    if (msg == NULL || len == 0) {
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Lock during backend output for thread safety */
    log_lock();
    log_status_t result = log_output_to_backends(msg, len, LOG_LEVEL_INFO);
    log_unlock();

    return result;
}

/*---------------------------------------------------------------------------*/
/* Async Logging Implementation                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async logging background task
 * \param[in]       arg: Task argument (unused)
 */
static void log_async_task(void* arg) {
    LOG_UNUSED(arg);
    log_async_entry_t entry;

    while (s_async_state.running) {
        /* Wait for message from queue with timeout */
        osal_status_t status =
            osal_queue_receive(s_async_state.queue, &entry, 100);

        if (status == OSAL_OK) {
            /* Process the message - output to backends */
            log_output_to_backends(entry.message, entry.length, entry.level);

            /* Decrement pending count */
            if (s_async_state.pending_count > 0) {
                s_async_state.pending_count--;
            }
        }

        /* Check if flush was requested and queue is empty */
        if (s_async_state.flush_requested &&
            osal_queue_is_empty(s_async_state.queue)) {
            s_async_state.flush_requested = false;
        }
    }
}

/**
 * \brief           Initialize async logging subsystem
 * \param[in]       queue_size: Size of the message queue
 * \param[in]       policy: Buffer full policy
 * \return          LOG_OK on success, error code otherwise
 */
static log_status_t log_async_init_internal(size_t queue_size,
                                            log_async_policy_t policy) {
    osal_status_t status;

    /* Use default queue size if not specified */
    if (queue_size == 0) {
        queue_size = LOG_ASYNC_QUEUE_SIZE;
    }

    s_async_state.queue_size = queue_size;
    s_async_state.policy = policy;

    /* Create mutex for thread safety */
    status = osal_mutex_create(&s_async_state.mutex);
    if (status != OSAL_OK) {
        return LOG_ERROR_NO_MEMORY;
    }

    /* Create message queue */
    status = osal_queue_create(sizeof(log_async_entry_t), queue_size,
                               &s_async_state.queue);
    if (status != OSAL_OK) {
        osal_mutex_delete(s_async_state.mutex);
        s_async_state.mutex = NULL;
        return LOG_ERROR_NO_MEMORY;
    }

    /* Create background task */
    osal_task_config_t task_config = {.name = "log_async",
                                      .func = log_async_task,
                                      .arg = NULL,
                                      .priority = LOG_ASYNC_TASK_PRIORITY,
                                      .stack_size = LOG_ASYNC_TASK_STACK_SIZE};

    s_async_state.running = true;
    status = osal_task_create(&task_config, &s_async_state.task);
    if (status != OSAL_OK) {
        s_async_state.running = false;
        osal_queue_delete(s_async_state.queue);
        osal_mutex_delete(s_async_state.mutex);
        s_async_state.queue = NULL;
        s_async_state.mutex = NULL;
        return LOG_ERROR_NO_MEMORY;
    }

    s_async_state.pending_count = 0;
    s_async_state.flush_requested = false;

    return LOG_OK;
}

/**
 * \brief           Deinitialize async logging subsystem
 * \return          LOG_OK on success, error code otherwise
 */
static log_status_t log_async_deinit_internal(void) {
    /* Signal task to stop */
    s_async_state.running = false;

    /* Wait a bit for task to finish processing */
    if (s_async_state.task != NULL) {
        osal_task_delay(200);
        osal_task_delete(s_async_state.task);
        s_async_state.task = NULL;
    }

    /* Delete queue */
    if (s_async_state.queue != NULL) {
        osal_queue_delete(s_async_state.queue);
        s_async_state.queue = NULL;
    }

    /* Delete mutex */
    if (s_async_state.mutex != NULL) {
        osal_mutex_delete(s_async_state.mutex);
        s_async_state.mutex = NULL;
    }

    s_async_state.pending_count = 0;
    s_async_state.flush_requested = false;

    return LOG_OK;
}

/**
 * \brief           Queue a message for async processing
 * \param[in]       msg: Formatted message
 * \param[in]       len: Message length
 * \param[in]       level: Log level
 * \return          LOG_OK on success, error code otherwise
 */
static log_status_t log_async_queue_message(const char* msg, size_t len,
                                            log_level_t level) {
    if (s_async_state.queue == NULL) {
        return LOG_ERROR_NOT_INIT;
    }

    /* Prepare entry */
    log_async_entry_t entry;
    entry.level = level;
    entry.length =
        (len < sizeof(entry.message) - 1) ? len : sizeof(entry.message) - 1;
    memcpy(entry.message, msg, entry.length);
    entry.message[entry.length] = '\0';

    /* Check if queue is full */
    if (osal_queue_is_full(s_async_state.queue)) {
        switch (s_async_state.policy) {
            case LOG_ASYNC_POLICY_DROP_NEWEST:
                /* Drop this message */
                return LOG_ERROR_FULL;

            case LOG_ASYNC_POLICY_DROP_OLDEST:
                /* Remove oldest message from queue */
                {
                    log_async_entry_t discard;
                    osal_queue_receive(s_async_state.queue, &discard, 0);
                    if (s_async_state.pending_count > 0) {
                        s_async_state.pending_count--;
                    }
                }
                break;

            case LOG_ASYNC_POLICY_BLOCK:
                /* Will block in osal_queue_send below */
                break;
        }
    }

    /* Send to queue */
    uint32_t timeout = (s_async_state.policy == LOG_ASYNC_POLICY_BLOCK)
                           ? OSAL_WAIT_FOREVER
                           : 0;
    osal_status_t status =
        osal_queue_send(s_async_state.queue, &entry, timeout);

    if (status == OSAL_OK) {
        s_async_state.pending_count++;
        return LOG_OK;
    }

    return LOG_ERROR_FULL;
}

/*---------------------------------------------------------------------------*/
/* Async Control API                                                         */
/*---------------------------------------------------------------------------*/

log_status_t log_async_flush(void) {
    /* Check if async mode is enabled */
    if (!s_log_state.async_mode || s_async_state.queue == NULL) {
        return LOG_OK;
    }

    /* Set flush request flag */
    s_async_state.flush_requested = true;

    /* Wait until queue is empty or timeout */
    int timeout_count = 0;
    const int max_timeout = 100; /* 10 seconds max */

    while (!osal_queue_is_empty(s_async_state.queue) &&
           timeout_count < max_timeout) {
        osal_task_delay(100);
        timeout_count++;
    }

    s_async_state.flush_requested = false;

    if (timeout_count >= max_timeout) {
        return LOG_ERROR;
    }

    return LOG_OK;
}

size_t log_async_pending(void) {
    if (!s_log_state.async_mode || s_async_state.queue == NULL) {
        return 0;
    }

    return osal_queue_get_count(s_async_state.queue);
}

bool log_is_async_mode(void) {
    return s_log_state.async_mode;
}

log_status_t log_async_set_policy(log_async_policy_t policy) {
    if (policy > LOG_ASYNC_POLICY_BLOCK) {
        return LOG_ERROR_INVALID_PARAM;
    }

    s_async_state.policy = policy;
    return LOG_OK;
}

log_async_policy_t log_async_get_policy(void) {
    return s_async_state.policy;
}

/*---------------------------------------------------------------------------*/
/* Backend Management                                                        */
/*---------------------------------------------------------------------------*/

log_status_t log_backend_register(log_backend_t* backend) {
    /* Validate parameters */
    if (backend == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    if (backend->name == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    if (backend->write == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Lock for thread-safe backend array modification */
    log_lock();

    /* Check if we have room for another backend */
    if (s_backend_count >= LOG_MAX_BACKENDS) {
        log_unlock();
        return LOG_ERROR_FULL;
    }

    /* Check if backend with same name already exists */
    for (size_t i = 0; i < s_backend_count; ++i) {
        if (s_backends[i] != NULL && s_backends[i]->name != NULL &&
            strcmp(s_backends[i]->name, backend->name) == 0) {
            log_unlock();
            return LOG_ERROR_INVALID_PARAM; /* Duplicate name */
        }
    }

    /* Initialize backend if init function provided */
    if (backend->init != NULL) {
        log_status_t status = backend->init(backend->ctx);
        if (status != LOG_OK) {
            log_unlock();
            return LOG_ERROR_BACKEND;
        }
    }

    /* Add backend to array */
    s_backends[s_backend_count++] = backend;

    log_unlock();
    return LOG_OK;
}

log_status_t log_backend_unregister(const char* name) {
    /* Validate parameters */
    if (name == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Lock for thread-safe backend array modification */
    log_lock();

    /* Find backend by name */
    for (size_t i = 0; i < s_backend_count; ++i) {
        if (s_backends[i] != NULL && s_backends[i]->name != NULL &&
            strcmp(s_backends[i]->name, name) == 0) {
            /* Found the backend */
            log_backend_t* backend = s_backends[i];

            /* Call deinit if provided */
            if (backend->deinit != NULL) {
                backend->deinit(backend->ctx);
            }

            /* Remove from array by shifting remaining elements */
            for (size_t j = i; j < s_backend_count - 1; ++j) {
                s_backends[j] = s_backends[j + 1];
            }
            s_backends[--s_backend_count] = NULL;

            log_unlock();
            return LOG_OK;
        }
    }

    log_unlock();
    /* Backend not found */
    return LOG_ERROR_INVALID_PARAM;
}

log_status_t log_backend_enable(const char* name, bool enable) {
    /* Validate parameters */
    if (name == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Lock for thread-safe backend modification */
    log_lock();

    /* Find backend by name */
    for (size_t i = 0; i < s_backend_count; ++i) {
        if (s_backends[i] != NULL && s_backends[i]->name != NULL &&
            strcmp(s_backends[i]->name, name) == 0) {
            s_backends[i]->enabled = enable;
            log_unlock();
            return LOG_OK;
        }
    }

    log_unlock();
    /* Backend not found */
    return LOG_ERROR_INVALID_PARAM;
}

log_backend_t* log_backend_get(const char* name) {
    /* Validate parameters */
    if (name == NULL) {
        return NULL;
    }

    /* Lock for thread-safe backend access */
    log_lock();

    /* Find backend by name */
    for (size_t i = 0; i < s_backend_count; ++i) {
        if (s_backends[i] != NULL && s_backends[i]->name != NULL &&
            strcmp(s_backends[i]->name, name) == 0) {
            log_backend_t* result = s_backends[i];
            log_unlock();
            return result;
        }
    }

    log_unlock();
    return NULL;
}

/*---------------------------------------------------------------------------*/
/* Console Backend                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Console backend write function
 */
static log_status_t console_backend_write(void* ctx, const char* msg,
                                          size_t len) {
    LOG_UNUSED(ctx);

    if (msg == NULL || len == 0) {
        return LOG_ERROR_INVALID_PARAM;
    }

    /* Write to stdout */
    size_t written = fwrite(msg, 1, len, stdout);
    if (written != len) {
        return LOG_ERROR_BACKEND;
    }

    return LOG_OK;
}

/**
 * \brief           Console backend flush function
 */
static log_status_t console_backend_flush(void* ctx) {
    LOG_UNUSED(ctx);
    fflush(stdout);
    return LOG_OK;
}

/**
 * \brief           Console backend init function
 */
static log_status_t console_backend_init(void* ctx) {
    console_backend_ctx_t* console_ctx = (console_backend_ctx_t*)ctx;
    if (console_ctx != NULL) {
        console_ctx->initialized = true;
    }
    return LOG_OK;
}

/**
 * \brief           Console backend deinit function
 */
static log_status_t console_backend_deinit(void* ctx) {
    console_backend_ctx_t* console_ctx = (console_backend_ctx_t*)ctx;
    if (console_ctx != NULL) {
        console_ctx->initialized = false;
    }
    return LOG_OK;
}

log_backend_t* log_backend_console_create(void) {
#if LOG_USE_STATIC_ALLOC
    /* Use static allocation */
    if (s_static_console_ctx_used) {
        return NULL; /* Only one console backend allowed in static mode */
    }

    log_backend_t* backend = log_static_alloc_backend();
    if (backend == NULL) {
        return NULL;
    }

    /* Use static context */
    s_static_console_ctx_used = true;
    memset(&s_static_console_ctx, 0, sizeof(console_backend_ctx_t));
    console_backend_ctx_t* ctx = &s_static_console_ctx;
#else
    /* Allocate backend structure */
    log_backend_t* backend = (log_backend_t*)malloc(sizeof(log_backend_t));
    if (backend == NULL) {
        return NULL;
    }

    /* Allocate context */
    console_backend_ctx_t* ctx =
        (console_backend_ctx_t*)malloc(sizeof(console_backend_ctx_t));
    if (ctx == NULL) {
        free(backend);
        return NULL;
    }
#endif

    /* Initialize context */
    ctx->initialized = false;

    /* Set up backend */
    backend->name = "console";
    backend->init = console_backend_init;
    backend->write = console_backend_write;
    backend->flush = console_backend_flush;
    backend->deinit = console_backend_deinit;
    backend->ctx = ctx;
    backend->min_level = LOG_LEVEL_TRACE;
    backend->enabled = true;

    return backend;
}

void log_backend_console_destroy(log_backend_t* backend) {
    if (backend == NULL) {
        return;
    }

#if LOG_USE_STATIC_ALLOC
    /* Mark static resources as free */
    if (backend->ctx == &s_static_console_ctx) {
        s_static_console_ctx_used = false;
    }
    log_static_free_backend(backend);
#else
    /* Free context */
    if (backend->ctx != NULL) {
        free(backend->ctx);
    }

    /* Free backend */
    free(backend);
#endif
}

/*---------------------------------------------------------------------------*/
/* Memory Backend                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Memory backend write function
 */
static log_status_t memory_backend_write(void* ctx, const char* msg,
                                         size_t len) {
    mem_backend_ctx_t* mem_ctx = (mem_backend_ctx_t*)ctx;

    if (mem_ctx == NULL || msg == NULL || len == 0) {
        return LOG_ERROR_INVALID_PARAM;
    }

    if (!mem_ctx->initialized || mem_ctx->buffer == NULL) {
        return LOG_ERROR_NOT_INIT;
    }

    /* Write data to ring buffer */
    for (size_t i = 0; i < len; ++i) {
        mem_ctx->buffer[mem_ctx->head] = msg[i];
        mem_ctx->head = (mem_ctx->head + 1) % mem_ctx->size;

        if (mem_ctx->count < mem_ctx->size) {
            mem_ctx->count++;
        } else {
            /* Buffer full, advance tail (overwrite oldest data) */
            mem_ctx->tail = (mem_ctx->tail + 1) % mem_ctx->size;
        }
    }

    return LOG_OK;
}

/**
 * \brief           Memory backend flush function
 */
static log_status_t memory_backend_flush(void* ctx) {
    LOG_UNUSED(ctx);
    /* Memory backend doesn't need flushing */
    return LOG_OK;
}

/**
 * \brief           Memory backend init function
 */
static log_status_t memory_backend_init(void* ctx) {
    mem_backend_ctx_t* mem_ctx = (mem_backend_ctx_t*)ctx;
    if (mem_ctx != NULL) {
        mem_ctx->initialized = true;
    }
    return LOG_OK;
}

/**
 * \brief           Memory backend deinit function
 */
static log_status_t memory_backend_deinit(void* ctx) {
    mem_backend_ctx_t* mem_ctx = (mem_backend_ctx_t*)ctx;
    if (mem_ctx != NULL) {
        mem_ctx->initialized = false;
    }
    return LOG_OK;
}

log_backend_t* log_backend_memory_create(size_t size) {
    if (size == 0) {
        return NULL;
    }

#if LOG_USE_STATIC_ALLOC
    /* Use static allocation */
    if (s_static_mem_ctx_used) {
        return NULL; /* Only one memory backend allowed in static mode */
    }

    /* In static mode, use the static buffer size */
    if (size > LOG_STATIC_MEMORY_BUFFER_SIZE) {
        size = LOG_STATIC_MEMORY_BUFFER_SIZE;
    }

    log_backend_t* backend = log_static_alloc_backend();
    if (backend == NULL) {
        return NULL;
    }

    /* Use static context and buffer */
    s_static_mem_ctx_used = true;
    memset(&s_static_mem_ctx, 0, sizeof(mem_backend_ctx_t));
    mem_backend_ctx_t* ctx = &s_static_mem_ctx;
    ctx->buffer = s_static_mem_buffer;
#else
    /* Allocate backend structure */
    log_backend_t* backend = (log_backend_t*)malloc(sizeof(log_backend_t));
    if (backend == NULL) {
        return NULL;
    }

    /* Allocate context */
    mem_backend_ctx_t* ctx =
        (mem_backend_ctx_t*)malloc(sizeof(mem_backend_ctx_t));
    if (ctx == NULL) {
        free(backend);
        return NULL;
    }

    /* Allocate buffer */
    ctx->buffer = (char*)malloc(size);
    if (ctx->buffer == NULL) {
        free(ctx);
        free(backend);
        return NULL;
    }
#endif

    /* Initialize context */
    ctx->size = size;
    ctx->head = 0;
    ctx->tail = 0;
    ctx->count = 0;
    ctx->initialized = false;
    memset(ctx->buffer, 0, size);

    /* Set up backend */
    backend->name = "memory";
    backend->init = memory_backend_init;
    backend->write = memory_backend_write;
    backend->flush = memory_backend_flush;
    backend->deinit = memory_backend_deinit;
    backend->ctx = ctx;
    backend->min_level = LOG_LEVEL_TRACE;
    backend->enabled = true;

    return backend;
}

void log_backend_memory_destroy(log_backend_t* backend) {
    if (backend == NULL) {
        return;
    }

#if LOG_USE_STATIC_ALLOC
    /* Mark static resources as free */
    if (backend->ctx == &s_static_mem_ctx) {
        s_static_mem_ctx_used = false;
    }
    log_static_free_backend(backend);
#else
    /* Free buffer and context */
    if (backend->ctx != NULL) {
        mem_backend_ctx_t* ctx = (mem_backend_ctx_t*)backend->ctx;
        if (ctx->buffer != NULL) {
            free(ctx->buffer);
        }
        free(ctx);
    }

    /* Free backend */
    free(backend);
#endif
}

size_t log_backend_memory_read(log_backend_t* backend, char* buf, size_t len) {
    if (backend == NULL || buf == NULL || len == 0) {
        return 0;
    }

    mem_backend_ctx_t* ctx = (mem_backend_ctx_t*)backend->ctx;
    if (ctx == NULL || ctx->buffer == NULL) {
        return 0;
    }

    /* Read data from ring buffer */
    size_t bytes_to_read = (len < ctx->count) ? len : ctx->count;
    size_t bytes_read = 0;

    for (size_t i = 0; i < bytes_to_read; ++i) {
        buf[i] = ctx->buffer[ctx->tail];
        ctx->tail = (ctx->tail + 1) % ctx->size;
        ctx->count--;
        bytes_read++;
    }

    /* Null terminate if there's room */
    if (bytes_read < len) {
        buf[bytes_read] = '\0';
    }

    return bytes_read;
}

void log_backend_memory_clear(log_backend_t* backend) {
    if (backend == NULL) {
        return;
    }

    mem_backend_ctx_t* ctx = (mem_backend_ctx_t*)backend->ctx;
    if (ctx == NULL) {
        return;
    }

    /* Reset ring buffer pointers */
    ctx->head = 0;
    ctx->tail = 0;
    ctx->count = 0;

    /* Clear buffer content */
    if (ctx->buffer != NULL) {
        memset(ctx->buffer, 0, ctx->size);
    }
}

size_t log_backend_memory_size(log_backend_t* backend) {
    if (backend == NULL) {
        return 0;
    }

    mem_backend_ctx_t* ctx = (mem_backend_ctx_t*)backend->ctx;
    if (ctx == NULL) {
        return 0;
    }

    return ctx->count;
}
