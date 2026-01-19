/**
 * \file            nx_spi_diagnostic.c
 * \brief           SPI diagnostic interface for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements SPI diagnostic operations for retrieving
 *                  statistics and error information.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_spi.h"
#include "hal/nx_status.h"
#include "nx_spi_helpers.h"
#include "nx_spi_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Diagnostic Interface Implementation                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get status implementation
 */
static nx_status_t spi_diagnostic_get_status(nx_diagnostic_t* self,
                                             void* status, size_t size) {
    nx_spi_impl_t* impl = NX_CONTAINER_OF(self, nx_spi_impl_t, diagnostic);

    /* Parameter validation */
    if (!impl->state || !status) {
        return NX_ERR_NULL_PTR;
    }
    if (size < sizeof(nx_spi_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    /* Copy statistics to output */
    memcpy(status, &impl->state->stats, sizeof(nx_spi_stats_t));

    return NX_OK;
}

/**
 * \brief           Get statistics implementation
 */
static nx_status_t spi_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                 void* stats, size_t size) {
    nx_spi_impl_t* impl = NX_CONTAINER_OF(self, nx_spi_impl_t, diagnostic);

    /* Parameter validation */
    if (!impl->state || !stats) {
        return NX_ERR_NULL_PTR;
    }
    if (size < sizeof(nx_spi_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    /* Copy statistics to output */
    memcpy(stats, &impl->state->stats, sizeof(nx_spi_stats_t));

    return NX_OK;
}

/**
 * \brief           Clear statistics implementation
 */
static nx_status_t spi_diagnostic_clear_statistics(nx_diagnostic_t* self) {
    nx_spi_impl_t* impl = NX_CONTAINER_OF(self, nx_spi_impl_t, diagnostic);

    /* Parameter validation */
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_spi_stats_t));

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize diagnostic interface
 */
void spi_init_diagnostic(nx_diagnostic_t* diagnostic) {
    diagnostic->get_status = spi_diagnostic_get_status;
    diagnostic->get_statistics = spi_diagnostic_get_statistics;
    diagnostic->clear_statistics = spi_diagnostic_clear_statistics;
}
