/**
 * \file            nx_i2c_diagnostic.c
 * \brief           I2C diagnostic interface for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements I2C diagnostic operations for retrieving
 *                  statistics and error information.
 */

#include "hal/base/nx_device.h"
#include "hal/interface/nx_i2c.h"
#include "hal/nx_status.h"
#include "nx_i2c_helpers.h"
#include "nx_i2c_types.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Diagnostic Interface Implementation                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get status implementation
 */
static nx_status_t i2c_diagnostic_get_status(nx_diagnostic_t* self,
                                             void* status, size_t size) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, diagnostic);

    /* Parameter validation */
    if (!impl->state || !status) {
        return NX_ERR_NULL_PTR;
    }
    if (size < sizeof(nx_i2c_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    /* Copy statistics to output */
    memcpy(status, &impl->state->stats, sizeof(nx_i2c_stats_t));

    return NX_OK;
}

/**
 * \brief           Get statistics implementation
 */
static nx_status_t i2c_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                 void* stats, size_t size) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, diagnostic);

    /* Parameter validation */
    if (!impl->state || !stats) {
        return NX_ERR_NULL_PTR;
    }
    if (size < sizeof(nx_i2c_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    /* Copy statistics to output */
    memcpy(stats, &impl->state->stats, sizeof(nx_i2c_stats_t));

    return NX_OK;
}

/**
 * \brief           Clear statistics implementation
 */
static nx_status_t i2c_diagnostic_clear_statistics(nx_diagnostic_t* self) {
    nx_i2c_impl_t* impl = NX_CONTAINER_OF(self, nx_i2c_impl_t, diagnostic);

    /* Parameter validation */
    if (!impl->state) {
        return NX_ERR_NULL_PTR;
    }

    /* Clear statistics */
    memset(&impl->state->stats, 0, sizeof(nx_i2c_stats_t));

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize diagnostic interface
 */
void i2c_init_diagnostic(nx_diagnostic_t* diagnostic) {
    diagnostic->get_status = i2c_diagnostic_get_status;
    diagnostic->get_statistics = i2c_diagnostic_get_statistics;
    diagnostic->clear_statistics = i2c_diagnostic_clear_statistics;
}
