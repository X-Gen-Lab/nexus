/**
 * \file            log.c
 * \brief           Log Framework Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
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
    osal_queue_handle_t barrier;        /**< Flush result token, or NULL */
} log_async_entry_t;

/**
 * \brief           Async state structure
 */
typedef struct {
    osal_queue_handle_t queue;     /**< Message queue handle */
    osal_task_handle_t task;       /**< Background task handle */
    log_async_policy_t policy;     /**< Buffer full policy */
    size_t queue_size;             /**< Queue size */
    size_t in_flight;              /**< Callback currently executing */
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
/* The short OSAL critical gate protects this mutex's lifetime. A caller pins
 * the mutex before it can block on it. Shutdown rejects new pins, then joins
 * the worker and releases resources only after all old pins have returned. */
static bool s_log_initializing;
static bool s_log_closing;
static bool s_log_deinit_active;
static size_t s_log_lock_users;
static unsigned s_callback_depth; /* Protected by the recursive log mutex. */

#ifndef LOG_OPERATION_TIMEOUT_MS
#define LOG_OPERATION_TIMEOUT_MS 1000u
#endif

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
static log_module_filter_t s_module_filters[LOG_MAX_MODULE_FILTERS] = {0};

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
    osal_mutex_handle_t mutex; /**< Serializes independent ring-buffer access */
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
    uint32_t milliseconds = 0;
    (void)osal_get_time_ms(&milliseconds);
    return milliseconds;
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
                    written = snprintf(buf + pos, buf_size - pos, "%lu",
                                       (unsigned long)log_get_timestamp_ms());
                    break;

                case 't': /* Time in HH:MM:SS format */
                {
                    time_t now = time(NULL);
                    struct tm time_info;
#ifdef _WIN32
                    struct tm* tm_info = localtime_s(&time_info, &now) == 0 ? &time_info : NULL;
#else
                    struct tm* tm_info = localtime_r(&now, &time_info);
#endif
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
                size_t available = buf_size - pos - 1;
                pos += (size_t)written < available ? (size_t)written : available;
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
static bool log_pin(bool worker) {
    if (osal_is_isr()) return false;
    osal_enter_critical();
    bool available = s_log_mutex && !s_log_initializing &&
                     (!s_log_closing || worker);
    if (available) ++s_log_lock_users;
    osal_exit_critical();
    return available;
}

static void log_unpin(void) {
    osal_enter_critical();
    --s_log_lock_users;
    osal_exit_critical();
}

static bool log_lock_internal(bool worker) {
    if (!log_pin(worker)) return false;
    if (osal_mutex_lock(s_log_mutex, LOG_OPERATION_TIMEOUT_MS) != OSAL_OK) {
        log_unpin();
        return false;
    }
    return true;
}

static bool log_lock(void) { return log_lock_internal(false); }

static void log_unlock(void) {
    (void)osal_mutex_unlock(s_log_mutex);
    log_unpin();
}

static log_status_t log_unavailable(void) {
    if (osal_is_isr()) return LOG_ERROR_ISR;
    osal_enter_critical();
    bool busy = s_log_initializing || s_log_closing || s_log_state.initialized;
    osal_exit_critical();
    return busy ? LOG_ERROR_BUSY : LOG_ERROR_NOT_INIT;
}

/*---------------------------------------------------------------------------*/
/* Initialization and Configuration                                          */
/*---------------------------------------------------------------------------*/

log_status_t log_init(const log_config_t* config) {
    if (osal_is_isr()) return LOG_ERROR_ISR;
    if (config && (config->level < LOG_LEVEL_TRACE || config->level > LOG_LEVEL_NONE ||
                   config->async_policy < LOG_ASYNC_POLICY_DROP_OLDEST ||
                   config->async_policy > LOG_ASYNC_POLICY_BLOCK))
        return LOG_ERROR_INVALID_PARAM;
    osal_enter_critical();
    if (s_log_initializing || s_log_closing) {
        osal_exit_critical();
        return LOG_ERROR_BUSY;
    }
    if (s_log_state.initialized) {
        osal_exit_critical();
        return LOG_ERROR_ALREADY_INIT;
    }
    s_log_initializing = true;
    osal_exit_critical();

    osal_mutex_handle_t mutex = NULL;
    log_status_t result = osal_mutex_create(&mutex) == OSAL_OK ? LOG_OK : LOG_ERROR_NO_MEMORY;
    if (result == LOG_OK) {
        osal_enter_critical();
        s_log_mutex = mutex;
        osal_exit_critical();
        s_log_state = (log_state_t){
            .level = config ? config->level : LOG_DEFAULT_LEVEL,
            .format = config && config->format ? config->format : LOG_DEFAULT_FORMAT,
            .async_mode = config && config->async_mode,
            .buffer_size = config && config->buffer_size ? config->buffer_size : LOG_ASYNC_BUFFER_SIZE,
            .max_msg_len = config && config->max_msg_len ? config->max_msg_len : LOG_MAX_MSG_LEN,
            .color_enabled = config && config->color_enabled,
        };
        if (s_log_state.async_mode)
            result = log_async_init_internal(config->async_queue_size, config->async_policy);
        if (result != LOG_OK) {
            (void)osal_mutex_delete(mutex);
            osal_enter_critical();
            s_log_mutex = NULL;
            osal_exit_critical();
        }
    }
    osal_enter_critical();
    s_log_state.initialized = result == LOG_OK;
    s_log_initializing = false;
    osal_exit_critical();
    return result;
}

log_status_t log_deinit(void) {
    if (osal_is_isr()) return LOG_ERROR_ISR;
    /* Acquiring the recursive mutex first detects a callback attempting to
     * destroy the object whose callback is currently executing. */
    if (log_lock()) {
        if (s_callback_depth) { log_unlock(); return LOG_ERROR_BUSY; }
        osal_enter_critical();
        bool claimed = !s_log_deinit_active;
        if (claimed) s_log_closing = s_log_deinit_active = true;
        osal_exit_critical();
        log_unlock();
        if (!claimed) return LOG_ERROR_BUSY;
    } else {
        osal_enter_critical();
        bool retry = s_log_closing && !s_log_deinit_active;
        if (retry) s_log_deinit_active = true;
        osal_exit_critical();
        if (!retry) return log_unavailable();
    }

    uint32_t started = log_get_timestamp_ms();
    log_status_t result = LOG_OK;
    for (;;) {
        osal_enter_critical();
        size_t users = s_log_lock_users;
        osal_exit_critical();
        if (!users) break;
        if ((uint32_t)(log_get_timestamp_ms() - started) >= LOG_OPERATION_TIMEOUT_MS) {
            result = LOG_ERROR_TIMEOUT;
            break;
        }
        if (osal_task_delay(1) != OSAL_OK) { result = LOG_ERROR_BUSY; break; }
    }
    if (result == LOG_OK && s_log_state.async_mode)
        result = log_async_deinit_internal();
    if (result != LOG_OK) {
        /* Retain all handles and backend ownership. A management task retries
         * shutdown; a timeout is never advertised as successful reclamation. */
        osal_enter_critical();
        s_log_deinit_active = false;
        osal_exit_critical();
        return result;
    }
    for (size_t i = 0; i < s_backend_count; ++i) {
        log_backend_t* backend = s_backends[i];
        if (backend && backend->flush && backend->flush(backend->ctx) != LOG_OK) {
            osal_enter_critical(); s_log_deinit_active = false; osal_exit_critical();
            return LOG_ERROR_BACKEND;
        }
    }
    for (size_t i = 0; i < s_backend_count; ++i) {
        log_backend_t* backend = s_backends[i];
        if (backend && backend->deinit && backend->deinit(backend->ctx) != LOG_OK) {
            osal_enter_critical(); s_log_deinit_active = false; osal_exit_critical();
            return LOG_ERROR_BACKEND;
        }
        s_backends[i] = NULL;
    }
    s_backend_count = 0;
    memset(s_module_filters, 0, sizeof(s_module_filters));
    s_module_filter_count = 0;
    osal_mutex_handle_t mutex = s_log_mutex;
    if (osal_mutex_delete(mutex) != OSAL_OK) {
        osal_enter_critical(); s_log_deinit_active = false; osal_exit_critical();
        return LOG_ERROR_BUSY;
    }
    osal_enter_critical();
    s_log_mutex = NULL;
    s_log_state = (log_state_t){.level = LOG_DEFAULT_LEVEL, .format = LOG_DEFAULT_FORMAT,
        .buffer_size = LOG_ASYNC_BUFFER_SIZE, .max_msg_len = LOG_MAX_MSG_LEN};
    s_log_closing = s_log_deinit_active = false;
    osal_exit_critical();
    return LOG_OK;
}

bool log_is_initialized(void) {
    osal_enter_critical();
    bool initialized = s_log_state.initialized;
    osal_exit_critical();
    return initialized;
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
    if (!log_lock()) return log_unavailable();
    s_log_state.level = level;
    log_unlock();

    return LOG_OK;
}

log_level_t log_get_level(void) {
    /* Thread-safe level read */
    if (!log_lock()) return LOG_DEFAULT_LEVEL;
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
    if (!log_lock()) return log_unavailable();

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
        if (!log_lock()) return LOG_DEFAULT_LEVEL;
        log_level_t level = s_log_state.level;
        log_unlock();
        return level;
    }

    /* Lock for thread-safe module filter access */
    if (!log_lock()) return LOG_DEFAULT_LEVEL;

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
    if (!log_lock()) return log_unavailable();

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
    if (!log_lock()) return;
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
    if (!log_lock()) return LOG_DEFAULT_FORMAT;
    const char* pattern = s_log_state.format;
    log_unlock();
    return pattern;
}

log_status_t log_set_format(const char* pattern) {
    if (!pattern) return LOG_ERROR_INVALID_PARAM;
    if (!log_lock()) return log_unavailable();
    s_log_state.format = pattern;
    log_unlock();
    return LOG_OK;
}

log_status_t log_set_max_msg_len(size_t max_len) {
    if (!log_lock()) return log_unavailable();
    s_log_state.max_msg_len = max_len ? max_len : LOG_MAX_MSG_LEN;
    log_unlock();
    return LOG_OK;
}

size_t log_get_max_msg_len(void) {
    if (!log_lock()) return LOG_MAX_MSG_LEN;
    size_t length = s_log_state.max_msg_len;
    log_unlock();
    return length;
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

    log_status_t result = LOG_ERROR_BACKEND;
    bool any_success = false;
    bool attempted = false;

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
            attempted = true;
            ++s_callback_depth;
            log_status_t status = backend->write(backend->ctx, msg, len);
            --s_callback_depth;
            if (status == LOG_OK) {
                any_success = true;
            }
            /* Continue with other backends even if one fails (isolation) */
        }
    }

    /* If no backends registered, still return OK */
    if (!attempted) return LOG_OK;

    /* Return OK if at least one backend succeeded */
    return any_success ? LOG_OK : result;
}

log_status_t log_write(log_level_t level, const char* module, const char* file,
                       int line, const char* func, const char* fmt, ...) {
    if (!fmt || level < LOG_LEVEL_TRACE || level > LOG_LEVEL_NONE)
        return LOG_ERROR_INVALID_PARAM;
    if (!log_lock()) return log_unavailable();
    if (s_callback_depth) { log_unlock(); return LOG_ERROR_BUSY; }
    if (!log_should_output(level, module)) { log_unlock(); return LOG_OK; }

    char user_msg[LOG_MAX_MSG_LEN];
    va_list args;
    va_start(args, fmt);
    (void)log_format_user_message(user_msg, sizeof(user_msg), fmt, args);
    va_end(args);
    (void)log_apply_truncation(user_msg, sizeof(user_msg), s_log_state.max_msg_len);
    char formatted_msg[LOG_MAX_MSG_LEN * 2];
    size_t formatted_len = log_format_with_pattern(formatted_msg, sizeof(formatted_msg),
        level, module, file, line, func, user_msg);
    if (formatted_len && formatted_len < sizeof(formatted_msg) - 1 &&
        formatted_msg[formatted_len - 1] != '\n') {
        formatted_msg[formatted_len++] = '\n';
        formatted_msg[formatted_len] = '\0';
    }
    if (s_log_state.async_mode)
        return log_async_queue_message(formatted_msg, formatted_len, level);
    log_status_t result = log_output_to_backends(formatted_msg, formatted_len, level);
    log_unlock();
    return result;
}

log_status_t log_write_raw(const char* msg, size_t len) {
    if (!msg || !len) return LOG_ERROR_INVALID_PARAM;
    if (!log_lock()) return log_unavailable();
    if (s_callback_depth) { log_unlock(); return LOG_ERROR_BUSY; }
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
    osal_queue_handle_t queue = arg;
    log_async_entry_t entry;
    /* Accepted entries are drained before cooperative return. The manager
     * first closes producer pins, so no producer can enqueue after this test. */
    while (!osal_task_should_stop() || !osal_queue_is_empty(queue)) {
        if (osal_queue_receive(queue, &entry, 10) != OSAL_OK) continue;
        while (!log_lock_internal(true)) (void)osal_task_yield();
        s_async_state.in_flight = 1;
        log_status_t result = LOG_OK;
        if (entry.barrier) {
            for (size_t i = 0; i < s_backend_count; ++i) {
                log_backend_t* backend = s_backends[i];
                if (backend && backend->flush) {
                    ++s_callback_depth;
                    if (backend->flush(backend->ctx) != LOG_OK) result = LOG_ERROR_BACKEND;
                    --s_callback_depth;
                }
            }
        } else {
            (void)log_output_to_backends(entry.message, entry.length, entry.level);
        }
        s_async_state.in_flight = 0;
        log_unlock();
        if (entry.barrier) (void)osal_queue_send(entry.barrier, &result, OSAL_NO_WAIT);
    }
}

static log_status_t log_async_init_internal(size_t queue_size,
                                            log_async_policy_t policy) {
    if (!queue_size) queue_size = LOG_ASYNC_QUEUE_SIZE;
    s_async_state.queue_size = queue_size;
    s_async_state.policy = policy;
    s_async_state.in_flight = 0;
    osal_status_t status = osal_queue_create(sizeof(log_async_entry_t), queue_size,
                                            &s_async_state.queue);
    if (status != OSAL_OK)
        return status == OSAL_ERROR_INVALID_PARAM ? LOG_ERROR_INVALID_PARAM : LOG_ERROR_NO_MEMORY;
    osal_task_config_t config = {.name = "log_async", .func = log_async_task,
        .arg = s_async_state.queue, .priority = LOG_ASYNC_TASK_PRIORITY,
        .stack_size = LOG_ASYNC_TASK_STACK_SIZE};
    status = osal_task_create(&config, &s_async_state.task);
    if (status != OSAL_OK) {
        (void)osal_queue_delete(s_async_state.queue);
        s_async_state.queue = NULL;
        return LOG_ERROR_NO_MEMORY;
    }
    return LOG_OK;
}

static log_status_t log_async_deinit_internal(void) {
    if (s_async_state.task) {
        if (osal_task_request_stop(s_async_state.task) != OSAL_OK) return LOG_ERROR;
        if (osal_task_join(s_async_state.task, LOG_OPERATION_TIMEOUT_MS) != OSAL_OK)
            return LOG_ERROR_TIMEOUT;
        if (osal_task_delete(s_async_state.task) != OSAL_OK) return LOG_ERROR_BUSY;
        s_async_state.task = NULL;
    }
    if (s_async_state.queue) {
        if (osal_queue_delete(s_async_state.queue) != OSAL_OK) return LOG_ERROR_BUSY;
        s_async_state.queue = NULL;
    }
    return LOG_OK;
}

/* Called with the log mutex held and a lifecycle pin. Keep the pin while
 * enqueueing, but release the mutex so the consumer can run even when full. */
static log_status_t log_async_queue_message(const char* msg, size_t len,
                                            log_level_t level) {
    log_async_entry_t entry = {.level = level};
    entry.length = len < sizeof(entry.message) - 1 ? len : sizeof(entry.message) - 1;
    memcpy(entry.message, msg, entry.length);
    entry.message[entry.length] = '\0';
    osal_queue_handle_t queue = s_async_state.queue;
    log_async_policy_t policy = s_async_state.policy;
    (void)osal_mutex_unlock(s_log_mutex);
    osal_status_t status = policy == LOG_ASYNC_POLICY_DROP_OLDEST ?
        osal_queue_send_overwrite(queue, &entry) :
        osal_queue_send(queue, &entry, policy == LOG_ASYNC_POLICY_BLOCK ? LOG_OPERATION_TIMEOUT_MS : 0);
    log_unpin();
    return status == OSAL_OK ? LOG_OK :
           status == OSAL_ERROR_TIMEOUT && policy == LOG_ASYNC_POLICY_BLOCK ? LOG_ERROR_TIMEOUT :
           LOG_ERROR_FULL;
}

log_status_t log_async_flush(void) {
    if (osal_is_isr()) return LOG_ERROR_ISR;
    uint32_t start = log_get_timestamp_ms();
    if (!log_lock()) return log_unavailable();
    if (s_callback_depth) { log_unlock(); return LOG_ERROR_BUSY; }
    if (!s_log_state.async_mode) { log_unlock(); return LOG_OK; }
    osal_queue_handle_t queue = s_async_state.queue;
    (void)osal_mutex_unlock(s_log_mutex);
    log_async_entry_t barrier = {0};
    log_status_t result = LOG_ERROR_NO_MEMORY;
    if (osal_queue_create(sizeof(log_status_t), 1, &barrier.barrier) == OSAL_OK) {
        result = LOG_ERROR_TIMEOUT;
        uint32_t elapsed = (uint32_t)(log_get_timestamp_ms() - start);
        uint32_t remaining = elapsed < LOG_OPERATION_TIMEOUT_MS ? LOG_OPERATION_TIMEOUT_MS - elapsed : 0;
        if (remaining && osal_queue_send(queue, &barrier, remaining) == OSAL_OK) {
            elapsed = (uint32_t)(log_get_timestamp_ms() - start);
            remaining = elapsed < LOG_OPERATION_TIMEOUT_MS ? LOG_OPERATION_TIMEOUT_MS - elapsed : 0;
            log_status_t response;
            if (osal_queue_receive(barrier.barrier, &response, remaining) == OSAL_OK) result = response;
        }
        /* The worker only carries an opaque token; after timeout a stale give
         * safely fails. No caller stack pointer enters the queue. */
        /* Nonblocking OSAL sends publish and release their pin atomically
         * before waking a receiver. Only this caller owns the reply token. */
        if (osal_queue_delete(barrier.barrier) != OSAL_OK) result = LOG_ERROR_BUSY;
    }
    log_unpin();
    return result;
}

size_t log_async_pending(void) {
    if (!log_lock()) return 0;
    size_t pending = s_log_state.async_mode ?
        osal_queue_get_count(s_async_state.queue) + s_async_state.in_flight : 0;
    log_unlock();
    return pending;
}

bool log_is_async_mode(void) {
    if (!log_lock()) return false;
    bool enabled = s_log_state.async_mode;
    log_unlock();
    return enabled;
}

log_status_t log_async_set_policy(log_async_policy_t policy) {
    if (policy < LOG_ASYNC_POLICY_DROP_OLDEST || policy > LOG_ASYNC_POLICY_BLOCK)
        return LOG_ERROR_INVALID_PARAM;
    if (!log_lock()) return log_unavailable();
    s_async_state.policy = policy;
    log_unlock();
    return LOG_OK;
}

log_async_policy_t log_async_get_policy(void) {
    if (!log_lock()) return LOG_ASYNC_POLICY_DROP_OLDEST;
    log_async_policy_t policy = s_async_state.policy;
    log_unlock();
    return policy;
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
    if (!log_lock()) return log_unavailable();
    if (s_callback_depth) { log_unlock(); return LOG_ERROR_BUSY; }

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
        ++s_callback_depth;
        log_status_t status = backend->init(backend->ctx);
        --s_callback_depth;
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
    if (!log_lock()) return log_unavailable();
    if (s_callback_depth) { log_unlock(); return LOG_ERROR_BUSY; }

    /* Find backend by name */
    for (size_t i = 0; i < s_backend_count; ++i) {
        if (s_backends[i] != NULL && s_backends[i]->name != NULL &&
            strcmp(s_backends[i]->name, name) == 0) {
            /* Found the backend */
            log_backend_t* backend = s_backends[i];

            /* Call deinit if provided */
            if (backend->deinit != NULL) {
                ++s_callback_depth;
                log_status_t status = backend->deinit(backend->ctx);
                --s_callback_depth;
                if (status != LOG_OK) { log_unlock(); return LOG_ERROR_BACKEND; }
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
    if (!log_lock()) return log_unavailable();

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
    if (!log_lock()) return NULL;

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
    if (osal_is_isr()) return NULL;
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
    log_backend_t* backend = (log_backend_t*)osal_mem_alloc(sizeof(log_backend_t));
    if (backend == NULL) {
        return NULL;
    }

    /* Allocate context */
    console_backend_ctx_t* ctx =
        (console_backend_ctx_t*)osal_mem_alloc(sizeof(console_backend_ctx_t));
    if (ctx == NULL) {
        osal_mem_free(backend);
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

static log_status_t log_backend_destroy_check(log_backend_t* backend) {
    if (osal_is_isr()) return LOG_ERROR_ISR;
    if (!backend) return LOG_ERROR_INVALID_PARAM;
    if (!log_lock()) {
        log_status_t unavailable = log_unavailable();
        return unavailable == LOG_ERROR_NOT_INIT ? LOG_OK : unavailable;
    }
    bool busy = s_callback_depth != 0;
    for (size_t i = 0; i < s_backend_count; ++i)
        if (s_backends[i] == backend) busy = true;
    log_unlock();
    return busy ? LOG_ERROR_BUSY : LOG_OK;
}

log_status_t log_backend_console_destroy(log_backend_t* backend) {
    log_status_t result = log_backend_destroy_check(backend);
    if (result != LOG_OK) return result;
#if LOG_USE_STATIC_ALLOC
    if (backend->ctx == &s_static_console_ctx) s_static_console_ctx_used = false;
    log_static_free_backend(backend);
#else
    osal_mem_free(backend->ctx);
    osal_mem_free(backend);
#endif
    return LOG_OK;
}

/*---------------------------------------------------------------------------*/
/* Memory Backend                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Memory backend write function
 */
static bool memory_backend_lock(mem_backend_ctx_t* ctx) {
    return ctx && !osal_is_isr() && ctx->mutex &&
           osal_mutex_lock(ctx->mutex, LOG_OPERATION_TIMEOUT_MS) == OSAL_OK;
}

static log_status_t memory_backend_write(void* context, const char* msg, size_t len) {
    mem_backend_ctx_t* ctx = context;
    if (!msg || !len) return LOG_ERROR_INVALID_PARAM;
    if (!memory_backend_lock(ctx)) return LOG_ERROR_BUSY;
    if (!ctx->initialized || !ctx->buffer) {
        (void)osal_mutex_unlock(ctx->mutex);
        return LOG_ERROR_NOT_INIT;
    }
    for (size_t i = 0; i < len; ++i) {
        ctx->buffer[ctx->head] = msg[i];
        ctx->head = (ctx->head + 1) % ctx->size;
        if (ctx->count < ctx->size) ++ctx->count;
        else ctx->tail = (ctx->tail + 1) % ctx->size;
    }
    (void)osal_mutex_unlock(ctx->mutex);
    return LOG_OK;
}

static log_status_t memory_backend_flush(void* ctx) {
    LOG_UNUSED(ctx);
    return LOG_OK;
}

static log_status_t memory_backend_init(void* context) {
    mem_backend_ctx_t* ctx = context;
    if (!memory_backend_lock(ctx)) return LOG_ERROR_BUSY;
    ctx->initialized = true;
    (void)osal_mutex_unlock(ctx->mutex);
    return LOG_OK;
}

static log_status_t memory_backend_deinit(void* context) {
    mem_backend_ctx_t* ctx = context;
    if (!memory_backend_lock(ctx)) return LOG_ERROR_BUSY;
    ctx->initialized = false;
    (void)osal_mutex_unlock(ctx->mutex);
    return LOG_OK;
}

log_backend_t* log_backend_memory_create(size_t size) {
    if (osal_is_isr() || size == 0) {
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
    log_backend_t* backend = (log_backend_t*)osal_mem_alloc(sizeof(log_backend_t));
    if (backend == NULL) {
        return NULL;
    }

    /* Allocate context */
    mem_backend_ctx_t* ctx =
        (mem_backend_ctx_t*)osal_mem_alloc(sizeof(mem_backend_ctx_t));
    if (ctx == NULL) {
        osal_mem_free(backend);
        return NULL;
    }

    /* Allocate buffer */
    ctx->buffer = (char*)osal_mem_alloc(size);
    if (ctx->buffer == NULL) {
        osal_mem_free(ctx);
        osal_mem_free(backend);
        return NULL;
    }
#endif

    ctx->mutex = NULL;
    if (osal_mutex_create(&ctx->mutex) != OSAL_OK) {
#if LOG_USE_STATIC_ALLOC
        s_static_mem_ctx_used = false;
        log_static_free_backend(backend);
#else
        osal_mem_free(ctx->buffer);
        osal_mem_free(ctx);
        osal_mem_free(backend);
#endif
        return NULL;
    }

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

log_status_t log_backend_memory_destroy(log_backend_t* backend) {
    log_status_t result = log_backend_destroy_check(backend);
    if (result != LOG_OK) return result;
    mem_backend_ctx_t* ctx = backend->ctx;
    if (ctx && osal_mutex_delete(ctx->mutex) != OSAL_OK) return LOG_ERROR_BUSY;
#if LOG_USE_STATIC_ALLOC
    if (ctx == &s_static_mem_ctx) s_static_mem_ctx_used = false;
    log_static_free_backend(backend);
#else
    if (ctx) osal_mem_free(ctx->buffer);
    osal_mem_free(ctx);
    osal_mem_free(backend);
#endif
    return LOG_OK;
}

size_t log_backend_memory_read(log_backend_t* backend, char* buf, size_t len) {
    if (!backend || !buf || !len) return 0;
    mem_backend_ctx_t* ctx = backend->ctx;
    if (!memory_backend_lock(ctx)) return 0;
    size_t count = len < ctx->count ? len : ctx->count;
    for (size_t i = 0; i < count; ++i) {
        buf[i] = ctx->buffer[ctx->tail];
        ctx->tail = (ctx->tail + 1) % ctx->size;
    }
    ctx->count -= count;
    if (count < len) buf[count] = '\0';
    (void)osal_mutex_unlock(ctx->mutex);
    return count;
}

void log_backend_memory_clear(log_backend_t* backend) {
    if (!backend) return;
    mem_backend_ctx_t* ctx = backend->ctx;
    if (!memory_backend_lock(ctx)) return;
    ctx->head = ctx->tail = ctx->count = 0;
    if (ctx->buffer) memset(ctx->buffer, 0, ctx->size);
    (void)osal_mutex_unlock(ctx->mutex);
}

size_t log_backend_memory_size(log_backend_t* backend) {
    if (!backend) return 0;
    mem_backend_ctx_t* ctx = backend->ctx;
    if (!memory_backend_lock(ctx)) return 0;
    size_t count = ctx->count;
    (void)osal_mutex_unlock(ctx->mutex);
    return count;
}
