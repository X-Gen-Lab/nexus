/**
 * \file            log.h
 * \brief           Log Framework Core API
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \brief           Unified logging interface for Nexus embedded platform.
 *
 * The log framework provides:
 * - Multiple log levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
 * - Multiple output backends (Console, UART, Memory)
 * - Module-level filtering
 * - Customizable format patterns
 * - Synchronous and asynchronous modes
 * - Thread-safe operation
 *
 * Example usage:
 * \code{.c}
 * #define LOG_MODULE "app"
 * #include "log/log.h"
 *
 * void app_init(void) {
 *     log_init(NULL);  // Use default config
 *     LOG_INFO("Application started");
 *     LOG_DEBUG("Debug value: %d", 42);
 * }
 * \endcode
 */

#ifndef LOG_H
#define LOG_H

#include "log_backend.h"
#include "log_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        LOG Log Framework
 * \brief           Core logging functionality
 * \{
 */

/**
 * \brief           Log configuration structure
 */
typedef struct {
    log_level_t level;       /**< Global log level filter */
    const char* format;      /**< Format pattern (NULL for default) */
    bool async_mode;         /**< Enable asynchronous logging */
    size_t buffer_size;      /**< Async buffer size (0 for default) */
    size_t max_msg_len;      /**< Maximum message length (0 for default) */
    bool color_enabled;      /**< Enable ANSI color codes */
    size_t async_queue_size; /**< Async queue size (0 for default) */
    log_async_policy_t async_policy; /**< Async buffer full policy */
} log_config_t;

/**
 * \brief           Default configuration initializer
 */
#define LOG_CONFIG_DEFAULT                                                     \
    {.level = LOG_DEFAULT_LEVEL,                                               \
     .format = NULL,                                                           \
     .async_mode = false,                                                      \
     .buffer_size = 0,                                                         \
     .max_msg_len = 0,                                                         \
     .color_enabled = false,                                                   \
     .async_queue_size = 0,                                                    \
     .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST}

/**
 * \name            Initialization and Configuration
 * \{
 */

/**
 * \brief           Initialize the log system
 * \param[in]       config: Configuration structure, or NULL for defaults
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_init(const log_config_t* config);

/**
 * \brief           Deinitialize the log system
 * \return          LOG_OK on success, error code otherwise
 * \note            Flushes pending messages and releases resources
 */
log_status_t log_deinit(void);

/**
 * \brief           Check if log system is initialized
 * \return          true if initialized, false otherwise
 */
bool log_is_initialized(void);

/**
 * \}
 */

/**
 * \name            Level Management
 * \{
 */

/**
 * \brief           Set the global log level
 * \param[in]       level: New log level
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_set_level(log_level_t level);

/**
 * \brief           Get the current global log level
 * \return          Current log level
 */
log_level_t log_get_level(void);

/**
 * \brief           Set log level for a specific module
 * \param[in]       module: Module name (supports wildcards like "hal.*")
 * \param[in]       level: Log level for the module
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_module_set_level(const char* module, log_level_t level);

/**
 * \brief           Get log level for a specific module
 * \param[in]       module: Module name
 * \return          Module log level, or global level if not set
 */
log_level_t log_module_get_level(const char* module);

/**
 * \brief           Clear log level for a specific module
 * \param[in]       module: Module name pattern to clear
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_module_clear_level(const char* module);

/**
 * \brief           Clear all module-specific log levels
 */
void log_module_clear_all(void);

/**
 * \}
 */

/**
 * \name            Format Configuration
 * \{
 */

/**
 * \brief           Set the log format pattern
 * \param[in]       pattern: Format pattern string
 * \return          LOG_OK on success, error code otherwise
 *
 * Supported tokens:
 * - %T: Timestamp in milliseconds
 * - %t: Time in HH:MM:SS format
 * - %L: Level name (full)
 * - %l: Level name (short, single char)
 * - %M: Module name
 * - %F: File name
 * - %f: Function name
 * - %n: Line number
 * - %m: Message
 * - %c: Color code (ANSI)
 * - %C: Color reset (ANSI)
 * - %%: Literal percent sign
 */
log_status_t log_set_format(const char* pattern);

/**
 * \brief           Get the current log format pattern
 * \return          Current format pattern string
 */
const char* log_get_format(void);

/**
 * \brief           Set maximum message length
 * \param[in]       max_len: Maximum message length (0 for default)
 * \return          LOG_OK on success
 * \note            Messages exceeding this length will be truncated with "..."
 */
log_status_t log_set_max_msg_len(size_t max_len);

/**
 * \brief           Get maximum message length
 * \return          Current maximum message length
 */
size_t log_get_max_msg_len(void);

/**
 * \}
 */

/**
 * \name            Logging Functions
 * \{
 */

/**
 * \brief           Write a log message
 * \param[in]       level: Log level
 * \param[in]       module: Module name
 * \param[in]       file: Source file name
 * \param[in]       line: Source line number
 * \param[in]       func: Function name
 * \param[in]       fmt: Printf-style format string
 * \param[in]       ...: Format arguments
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_write(log_level_t level, const char* module, const char* file,
                       int line, const char* func, const char* fmt, ...);

/**
 * \brief           Write a raw message without formatting
 * \param[in]       msg: Message string
 * \param[in]       len: Message length
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_write_raw(const char* msg, size_t len);

/**
 * \}
 */

/**
 * \name            Async Control
 * \{
 */

/**
 * \brief           Flush all pending async messages
 * \return          LOG_OK on success, error code otherwise
 * \note            Blocks until all messages are processed
 */
log_status_t log_async_flush(void);

/**
 * \brief           Get number of pending async messages
 * \return          Number of pending messages
 */
size_t log_async_pending(void);

/**
 * \brief           Check if async mode is enabled
 * \return          true if async mode is enabled, false otherwise
 */
bool log_is_async_mode(void);

/**
 * \brief           Set async buffer full policy
 * \param[in]       policy: Policy to use when buffer is full
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_async_set_policy(log_async_policy_t policy);

/**
 * \brief           Get current async buffer full policy
 * \return          Current policy
 */
log_async_policy_t log_async_get_policy(void);

/**
 * \}
 */

/**
 * \}
 */

/**
 * \defgroup        LOG_MACROS Logging Macros
 * \brief           Convenience macros for logging
 * \{
 */

/**
 * \brief           Default module name if not defined
 */
#ifndef LOG_MODULE
#define LOG_MODULE "default"
#endif

/**
 * \brief           Internal logging macro
 */
#define LOG_WRITE(level, fmt, ...)                                             \
    log_write(level, LOG_MODULE, __FILE__, __LINE__, __func__, fmt,            \
              ##__VA_ARGS__)

/**
 * \name            Level-specific Logging Macros
 * \{
 */

#if LOG_COMPILE_LEVEL <= LOG_LEVEL_TRACE
/**
 * \brief           Log a trace message
 * \param[in]       fmt: Printf-style format string
 * \param[in]       ...: Format arguments
 */
#define LOG_TRACE(fmt, ...) LOG_WRITE(LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)
#else
#define LOG_TRACE(fmt, ...) ((void)0)
#endif

#if LOG_COMPILE_LEVEL <= LOG_LEVEL_DEBUG
/**
 * \brief           Log a debug message
 * \param[in]       fmt: Printf-style format string
 * \param[in]       ...: Format arguments
 */
#define LOG_DEBUG(fmt, ...) LOG_WRITE(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#if LOG_COMPILE_LEVEL <= LOG_LEVEL_INFO
/**
 * \brief           Log an info message
 * \param[in]       fmt: Printf-style format string
 * \param[in]       ...: Format arguments
 */
#define LOG_INFO(fmt, ...) LOG_WRITE(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...) ((void)0)
#endif

#if LOG_COMPILE_LEVEL <= LOG_LEVEL_WARN
/**
 * \brief           Log a warning message
 * \param[in]       fmt: Printf-style format string
 * \param[in]       ...: Format arguments
 */
#define LOG_WARN(fmt, ...) LOG_WRITE(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...) ((void)0)
#endif

#if LOG_COMPILE_LEVEL <= LOG_LEVEL_ERROR
/**
 * \brief           Log an error message
 * \param[in]       fmt: Printf-style format string
 * \param[in]       ...: Format arguments
 */
#define LOG_ERROR(fmt, ...) LOG_WRITE(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...) ((void)0)
#endif

#if LOG_COMPILE_LEVEL <= LOG_LEVEL_FATAL
/**
 * \brief           Log a fatal error message
 * \param[in]       fmt: Printf-style format string
 * \param[in]       ...: Format arguments
 */
#define LOG_FATAL(fmt, ...) LOG_WRITE(LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#else
#define LOG_FATAL(fmt, ...) ((void)0)
#endif

/**
 * \}
 */

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
