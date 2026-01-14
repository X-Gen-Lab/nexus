/**
 * \file            log_backend_uart.c
 * \brief           Log UART Backend Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         UART backend for log framework. Outputs log messages
 *                  to a UART peripheral using the HAL UART interface.
 *                  Requirements: 3.5
 */

#include "hal/hal_uart.h"
#include "log/log_backend.h"
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* UART Backend Context                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART backend context structure
 */
typedef struct {
    hal_uart_instance_t uart; /**< HAL UART instance */
    bool initialized;         /**< Initialization flag */
    uint32_t timeout_ms;      /**< Transmit timeout in milliseconds */
} uart_backend_ctx_t;

/**
 * \brief           Default UART transmit timeout (milliseconds)
 */
#define UART_BACKEND_DEFAULT_TIMEOUT_MS 1000

/*---------------------------------------------------------------------------*/
/* UART Backend Functions                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART backend initialization function
 * \param[in]       ctx: Backend context
 * \return          LOG_OK on success, error code otherwise
 */
static log_status_t uart_backend_init(void* ctx) {
    uart_backend_ctx_t* uart_ctx = (uart_backend_ctx_t*)ctx;

    if (uart_ctx == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    uart_ctx->initialized = true;
    return LOG_OK;
}

/**
 * \brief           UART backend write function
 * \param[in]       ctx: Backend context
 * \param[in]       msg: Message to write
 * \param[in]       len: Message length
 * \return          LOG_OK on success, error code otherwise
 */
static log_status_t uart_backend_write(void* ctx, const char* msg, size_t len) {
    uart_backend_ctx_t* uart_ctx = (uart_backend_ctx_t*)ctx;

    if (uart_ctx == NULL || msg == NULL || len == 0) {
        return LOG_ERROR_INVALID_PARAM;
    }

    if (!uart_ctx->initialized) {
        return LOG_ERROR_NOT_INIT;
    }

    /* Transmit data via HAL UART */
    hal_status_t status = hal_uart_transmit(uart_ctx->uart, (const uint8_t*)msg,
                                            len, uart_ctx->timeout_ms);

    if (status != HAL_OK) {
        return LOG_ERROR_BACKEND;
    }

    return LOG_OK;
}

/**
 * \brief           UART backend flush function
 * \param[in]       ctx: Backend context
 * \return          LOG_OK on success, error code otherwise
 * \note            UART is typically unbuffered, so flush is a no-op
 */
static log_status_t uart_backend_flush(void* ctx) {
    uart_backend_ctx_t* uart_ctx = (uart_backend_ctx_t*)ctx;

    if (uart_ctx == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    if (!uart_ctx->initialized) {
        return LOG_ERROR_NOT_INIT;
    }

    /* UART transmit is typically blocking, so no explicit flush needed */
    return LOG_OK;
}

/**
 * \brief           UART backend deinitialization function
 * \param[in]       ctx: Backend context
 * \return          LOG_OK on success, error code otherwise
 */
static log_status_t uart_backend_deinit(void* ctx) {
    uart_backend_ctx_t* uart_ctx = (uart_backend_ctx_t*)ctx;

    if (uart_ctx == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    uart_ctx->initialized = false;
    return LOG_OK;
}

/*---------------------------------------------------------------------------*/
/* Public API                                                                */
/*---------------------------------------------------------------------------*/

log_backend_t* log_backend_uart_create(hal_uart_instance_t uart) {
    /* Validate UART instance */
    if (uart >= HAL_UART_MAX) {
        return NULL;
    }

    /* Allocate backend structure */
    log_backend_t* backend = (log_backend_t*)malloc(sizeof(log_backend_t));
    if (backend == NULL) {
        return NULL;
    }

    /* Allocate context */
    uart_backend_ctx_t* ctx =
        (uart_backend_ctx_t*)malloc(sizeof(uart_backend_ctx_t));
    if (ctx == NULL) {
        free(backend);
        return NULL;
    }

    /* Initialize context */
    ctx->uart = uart;
    ctx->initialized = false;
    ctx->timeout_ms = UART_BACKEND_DEFAULT_TIMEOUT_MS;

    /* Set up backend */
    backend->name = "uart";
    backend->init = uart_backend_init;
    backend->write = uart_backend_write;
    backend->flush = uart_backend_flush;
    backend->deinit = uart_backend_deinit;
    backend->ctx = ctx;
    backend->min_level = LOG_LEVEL_TRACE;
    backend->enabled = true;

    return backend;
}

void log_backend_uart_destroy(log_backend_t* backend) {
    if (backend == NULL) {
        return;
    }

    /* Free context */
    if (backend->ctx != NULL) {
        free(backend->ctx);
    }

    /* Free backend */
    free(backend);
}

/**
 * \brief           Set UART backend transmit timeout
 * \param[in]       backend: Pointer to UART backend
 * \param[in]       timeout_ms: Timeout in milliseconds
 * \return          LOG_OK on success, error code otherwise
 */
log_status_t log_backend_uart_set_timeout(log_backend_t* backend,
                                          uint32_t timeout_ms) {
    if (backend == NULL || backend->ctx == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    uart_backend_ctx_t* ctx = (uart_backend_ctx_t*)backend->ctx;
    ctx->timeout_ms = timeout_ms;

    return LOG_OK;
}

/**
 * \brief           Get UART instance from backend
 * \param[in]       backend: Pointer to UART backend
 * \return          UART instance, or HAL_UART_MAX on error
 */
hal_uart_instance_t log_backend_uart_get_instance(log_backend_t* backend) {
    if (backend == NULL || backend->ctx == NULL) {
        return HAL_UART_MAX;
    }

    uart_backend_ctx_t* ctx = (uart_backend_ctx_t*)backend->ctx;
    return ctx->uart;
}
