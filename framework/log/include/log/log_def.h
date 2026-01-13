/**
 * \file            log_def.h
 * \brief           Log Framework Common Definitions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef LOG_DEF_H
#define LOG_DEF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        LOG_DEF Log Definitions
 * \brief           Common definitions for Log framework
 * \{
 */

/**
 * \brief           Log status codes
 */
typedef enum {
    LOG_OK = 0,                  /**< Operation successful */
    LOG_ERROR = 1,               /**< Generic error */
    LOG_ERROR_INVALID_PARAM = 2, /**< Invalid parameter */
    LOG_ERROR_NOT_INIT = 3,      /**< Not initialized */
    LOG_ERROR_NO_MEMORY = 4,     /**< Out of memory */
    LOG_ERROR_FULL = 5,          /**< Buffer full */
    LOG_ERROR_BACKEND = 6,       /**< Backend error */
    LOG_ERROR_ALREADY_INIT = 7,  /**< Already initialized */
} log_status_t;

/**
 * \brief           Log levels
 *
 * Log levels are ordered from most verbose (TRACE) to least verbose (FATAL).
 * LOG_LEVEL_NONE disables all logging.
 */
typedef enum {
    LOG_LEVEL_TRACE = 0, /**< Most detailed tracing information */
    LOG_LEVEL_DEBUG = 1, /**< Debug information */
    LOG_LEVEL_INFO = 2,  /**< General information */
    LOG_LEVEL_WARN = 3,  /**< Warning messages */
    LOG_LEVEL_ERROR = 4, /**< Error messages */
    LOG_LEVEL_FATAL = 5, /**< Fatal error messages */
    LOG_LEVEL_NONE = 6,  /**< Disable all logging */
} log_level_t;

/**
 * \brief           Default configuration values
 * \{
 */
#ifndef LOG_DEFAULT_LEVEL
#define LOG_DEFAULT_LEVEL LOG_LEVEL_INFO
#endif

#ifndef LOG_MAX_MSG_LEN
#define LOG_MAX_MSG_LEN 128
#endif

#ifndef LOG_MAX_BACKENDS
#define LOG_MAX_BACKENDS 4
#endif

#ifndef LOG_MAX_MODULE_FILTERS
#define LOG_MAX_MODULE_FILTERS 16
#endif

#ifndef LOG_MODULE_NAME_LEN
#define LOG_MODULE_NAME_LEN 32
#endif

#ifndef LOG_DEFAULT_FORMAT
#define LOG_DEFAULT_FORMAT "[%T] [%L] [%M] %m"
#endif

#ifndef LOG_ASYNC_BUFFER_SIZE
#define LOG_ASYNC_BUFFER_SIZE 1024
#endif

#ifndef LOG_ASYNC_QUEUE_SIZE
#define LOG_ASYNC_QUEUE_SIZE 32
#endif

#ifndef LOG_ASYNC_TASK_STACK_SIZE
#define LOG_ASYNC_TASK_STACK_SIZE 2048
#endif

#ifndef LOG_ASYNC_TASK_PRIORITY
#define LOG_ASYNC_TASK_PRIORITY 8
#endif

/**
 * \brief           Async buffer full policy
 */
typedef enum {
    LOG_ASYNC_POLICY_DROP_OLDEST = 0, /**< Drop oldest message when full */
    LOG_ASYNC_POLICY_DROP_NEWEST = 1, /**< Drop newest message when full */
    LOG_ASYNC_POLICY_BLOCK = 2,       /**< Block until space available */
} log_async_policy_t;
/** \} */

/**
 * \brief           Compile-time log level
 *
 * Messages below this level will be compiled out entirely.
 * Set to LOG_LEVEL_NONE to disable all logging at compile time.
 * This reduces code size by eliminating log calls at compile time.
 *
 * Example usage in build system:
 * - CMake: add_definitions(-DLOG_COMPILE_LEVEL=LOG_LEVEL_INFO)
 * - GCC: -DLOG_COMPILE_LEVEL=LOG_LEVEL_INFO
 *
 * Requirements: 7.2, 7.3
 */
#ifndef LOG_COMPILE_LEVEL
#define LOG_COMPILE_LEVEL LOG_LEVEL_TRACE
#endif

/**
 * \brief           Static allocation mode
 *
 * When enabled (set to 1), the log framework uses static allocation
 * instead of dynamic memory allocation (malloc/free).
 * This is useful for embedded systems where dynamic allocation is
 * not available or not desired.
 *
 * When static allocation is enabled:
 * - Backend structures are allocated from a static pool
 * - Memory backend uses a static buffer
 * - No malloc/free calls are made
 *
 * Requirements: 7.4
 */
#ifndef LOG_USE_STATIC_ALLOC
#define LOG_USE_STATIC_ALLOC 0
#endif

/**
 * \brief           Maximum number of statically allocated backends
 * \note            Only used when LOG_USE_STATIC_ALLOC is enabled
 */
#ifndef LOG_STATIC_BACKEND_COUNT
#define LOG_STATIC_BACKEND_COUNT LOG_MAX_BACKENDS
#endif

/**
 * \brief           Static memory backend buffer size
 * \note            Only used when LOG_USE_STATIC_ALLOC is enabled
 */
#ifndef LOG_STATIC_MEMORY_BUFFER_SIZE
#define LOG_STATIC_MEMORY_BUFFER_SIZE 1024
#endif

/**
 * \brief           Check if status is OK
 * \param[in]       status: Status to check
 * \return          true if OK, false otherwise
 */
#define LOG_IS_OK(status) ((status) == LOG_OK)

/**
 * \brief           Check if status is error
 * \param[in]       status: Status to check
 * \return          true if error, false otherwise
 */
#define LOG_IS_ERROR(status) ((status) != LOG_OK)

/**
 * \brief           Return if status is error
 * \param[in]       status: Status to check
 */
#define LOG_RETURN_IF_ERROR(status)                                            \
    do {                                                                       \
        log_status_t __status = (status);                                      \
        if (LOG_IS_ERROR(__status)) {                                          \
            return __status;                                                   \
        }                                                                      \
    } while (0)

/**
 * \brief           Unused parameter macro
 * \param[in]       x: Parameter to mark as unused
 */
#define LOG_UNUSED(x) ((void)(x))

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* LOG_DEF_H */
