/**
 * \file            log_backend.h
 * \brief           Log Backend Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef LOG_BACKEND_H
#define LOG_BACKEND_H

#include "log_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        LOG_BACKEND Log Backend
 * \brief           Log backend interface and built-in backends
 * \{
 */

/**
 * \brief           Forward declaration of backend structure
 */
typedef struct log_backend log_backend_t;

/**
 * \brief           Backend initialization function type
 * \param[in]       ctx: Backend context
 * \return          LOG_OK on success, error code otherwise
 */
typedef log_status_t (*log_backend_init_fn)(void* ctx);

/**
 * \brief           Backend write function type
 * \param[in]       ctx: Backend context
 * \param[in]       msg: Message to write
 * \param[in]       len: Message length
 * \return          LOG_OK on success, error code otherwise
 */
typedef log_status_t (*log_backend_write_fn)(void* ctx, const char* msg,
                                             size_t len);

/**
 * \brief           Backend flush function type
 * \param[in]       ctx: Backend context
 * \return          LOG_OK on success, error code otherwise
 */
typedef log_status_t (*log_backend_flush_fn)(void* ctx);

/**
 * \brief           Backend deinitialization function type
 * \param[in]       ctx: Backend context
 * \return          LOG_OK on success, error code otherwise
 */
typedef log_status_t (*log_backend_deinit_fn)(void* ctx);

/**
 * \brief           Log backend structure
 *
 * Defines the interface for log output backends.
 * Each backend must implement at least the write function.
 */
struct log_backend {
    const char* name;             /**< Backend name (must be unique) */
    log_backend_init_fn init;     /**< Initialization function (optional) */
    log_backend_write_fn write;   /**< Write function (required) */
    log_backend_flush_fn flush;   /**< Flush function (optional) */
    log_backend_deinit_fn deinit; /**< Deinitialization function (optional) */
    void* ctx;                    /**< Backend-specific context */
    log_level_t min_level;        /**< Minimum level for this backend */
    bool enabled;                 /**< Whether backend is enabled */
};

/**
 * \brief           Register a backend with the log system
 * \param[in]       backend: Pointer to backend structure
 * \return          LOG_OK on success, error code otherwise
 * \note            The backend structure must remain valid for the lifetime
 *                  of the registration
 */
log_status_t log_backend_register(log_backend_t* backend);

/**
 * \brief           Unregister a backend from the log system
 * \param[in]       name: Name of the backend to unregister
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_backend_unregister(const char* name);

/**
 * \brief           Enable or disable a backend
 * \param[in]       name: Name of the backend
 * \param[in]       enable: true to enable, false to disable
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_backend_enable(const char* name, bool enable);

/**
 * \brief           Get a registered backend by name
 * \param[in]       name: Name of the backend
 * \return          Pointer to backend, or NULL if not found
 */
log_backend_t* log_backend_get(const char* name);

/**
 * \}
 */

/**
 * \defgroup        LOG_BACKEND_CONSOLE Console Backend
 * \brief           Console output backend (stdout)
 * \{
 */

/**
 * \brief           Create a console backend
 * \return          Pointer to backend structure, or NULL on failure
 * \note            Uses stdout for output (Native platform)
 */
log_backend_t* log_backend_console_create(void);

/**
 * \brief           Destroy a console backend
 * \param[in]       backend: Pointer to backend to destroy
 */
void log_backend_console_destroy(log_backend_t* backend);

/**
 * \}
 */

/**
 * \defgroup        LOG_BACKEND_MEMORY Memory Backend
 * \brief           Memory buffer backend for testing and debugging
 * \{
 */

/**
 * \brief           Create a memory backend
 * \param[in]       size: Buffer size in bytes
 * \return          Pointer to backend structure, or NULL on failure
 */
log_backend_t* log_backend_memory_create(size_t size);

/**
 * \brief           Destroy a memory backend
 * \param[in]       backend: Pointer to backend to destroy
 */
void log_backend_memory_destroy(log_backend_t* backend);

/**
 * \brief           Read data from memory backend buffer
 * \param[in]       backend: Pointer to memory backend
 * \param[out]      buf: Buffer to read into
 * \param[in]       len: Maximum bytes to read
 * \return          Number of bytes read
 */
size_t log_backend_memory_read(log_backend_t* backend, char* buf, size_t len);

/**
 * \brief           Clear the memory backend buffer
 * \param[in]       backend: Pointer to memory backend
 */
void log_backend_memory_clear(log_backend_t* backend);

/**
 * \brief           Get the number of bytes in the memory backend buffer
 * \param[in]       backend: Pointer to memory backend
 * \return          Number of bytes in buffer
 */
size_t log_backend_memory_size(log_backend_t* backend);

/**
 * \}
 */

/**
 * \defgroup        LOG_BACKEND_UART UART Backend
 * \brief           UART output backend
 * \{
 */

#include "hal/interface/nx_uart.h"

/**
 * \brief           Create a UART backend
 * \param[in]       uart: UART interface pointer (nx_uart_t*)
 * \return          Pointer to backend structure, or NULL on failure
 */
log_backend_t* log_backend_uart_create(nx_uart_t* uart);

/**
 * \brief           Destroy a UART backend
 * \param[in]       backend: Pointer to backend to destroy
 */
void log_backend_uart_destroy(log_backend_t* backend);

/**
 * \brief           Set UART backend transmit timeout
 * \param[in]       backend: Pointer to UART backend
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_backend_uart_set_timeout(log_backend_t* backend,
                                          uint32_t timeout_ms);

/**
 * \brief           Get UART interface from backend
 * \param[in]       backend: Pointer to UART backend
 * \return          UART interface pointer, or NULL on error
 */
nx_uart_t* log_backend_uart_get_interface(log_backend_t* backend);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* LOG_BACKEND_H */
