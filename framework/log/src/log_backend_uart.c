/**
 * \file            log_backend_uart.c
 * \brief           Log UART Backend Implementation
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         UART backend for log framework. Outputs log messages
 *                  to a UART peripheral using the new nx_uart_t interface.
 *                  Requirements: 3.5
 */

#include "hal/interface/nx_uart.h"
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
    nx_uart_t* uart;      /**< UART interface pointer */
    bool initialized;     /**< Initialization flag */
    uint32_t timeout_ms;  /**< Transmit timeout in milliseconds */
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

    if (uart_ctx->uart == NULL) {
        return LOG_ERROR_BACKEND;
    }

    /* Get synchronous TX interface */
    nx_tx_sync_t* tx_sync = uart_ctx->uart->get_tx_sync(uart_ctx->uart);
    if (tx_sync == NULL) {
        return LOG_ERROR_BACKEND;
    }

    /* Transmit data via UART */
    nx_status_t status = tx_sync->send(tx_sync, (const uint8_t*)msg, len,
                                       uart_ctx->timeout_ms);

    if (status != NX_OK) {
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

log_backend_t* log_backend_uart_create(nx_uart_t* uart) {
    /* Validate UART interface */
    if (uart == NULL) {
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

log_status_t log_backend_uart_set_timeout(log_backend_t* backend,
                                          uint32_t timeout_ms) {
    if (backend == NULL || backend->ctx == NULL) {
        return LOG_ERROR_INVALID_PARAM;
    }

    uart_backend_ctx_t* ctx = (uart_backend_ctx_t*)backend->ctx;
    ctx->timeout_ms = timeout_ms;

    return LOG_OK;
}

nx_uart_t* log_backend_uart_get_interface(log_backend_t* backend) {
    if (backend == NULL || backend->ctx == NULL) {
        return NULL;
    }

    uart_backend_ctx_t* ctx = (uart_backend_ctx_t*)backend->ctx;
    return ctx->uart;
}
