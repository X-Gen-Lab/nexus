/**
 * \file            nx_uart_diagnostic.c
 * \brief           UART diagnostic interface for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements UART diagnostic operations for retrieving
 *                  statistics and error information.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_uart.h"
#include "hal/nx_status.h"
#include "nx_uart_helpers.h"
#include "nx_uart_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Diagnostic Interface Implementation                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get status implementation
 */
static nx_status_t uart_diagnostic_get_status(nx_diagnostic_t* self,
                                              void* status, size_t size) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, diagnostic);

    /* Parameter validation */
    if (!impl->state || !status) {
        return NX_ERR_NULL_PTR;
    }
    if (size < sizeof(nx_uart_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    /* Copy statistics to output */
    memcpy(status, &impl->state->stats, sizeof(nx_uart_stats_t));

    return NX_OK;
}

/**
 * \brief           Get statistics implementation
 */
static nx_status_t uart_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                  void* stats, size_t size) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, diagnostic);

    /* Parameter validation */
    if (!impl->state || !stats) {
        return NX_ERR_NULL_PTR;
    }
    if (size < sizeof(nx_uart_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    /* Copy statistics to output */
    memcpy(stats, &impl->state->stats, sizeof(nx_uart_stats_t));

    return NX_OK;
}

/**
 * \brief           Clear statistics implementation
 */
static nx_status_t uart_diagnostic_clear_statistics(nx_diagnostic_t* self) {
    nx_uart_impl_t* impl = NX_CONTAINER_OF(self, nx_uart_impl_t, diagnostic);

    /* Parameter validation */
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_uart_stats_t));

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize diagnostic interface
 */
void uart_init_diagnostic(nx_diagnostic_t* diagnostic) {
    diagnostic->get_status = uart_diagnostic_get_status;
    diagnostic->get_statistics = uart_diagnostic_get_statistics;
    diagnostic->clear_statistics = uart_diagnostic_clear_statistics;
}
