/**
 * \file            nx_adc_diagnostic.c
 * \brief           ADC diagnostic interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements ADC diagnostic operations for retrieving
 *                  statistics and error information.
 */

#include "nx_adc_helpers.h"
#include "nx_adc_types.h"
#include <stddef.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC implementation from diagnostic interface
 */
static nx_adc_impl_t* adc_get_impl_from_diagnostic(nx_diagnostic_t* self) {
    if (!self) {
        return NULL;
    }
    return (nx_adc_impl_t*)((char*)self - offsetof(nx_adc_impl_t, diagnostic));
}

/*---------------------------------------------------------------------------*/
/* Diagnostic Operations                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get ADC status
 */
static nx_status_t adc_diagnostic_get_status(nx_diagnostic_t* self,
                                             void* status, size_t size) {
    nx_adc_impl_t* impl = adc_get_impl_from_diagnostic(self);
    if (!impl || !impl->state || !status) {
        return NX_ERR_NULL_PTR;
    }
    if (size < sizeof(nx_adc_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    memcpy(status, &impl->state->stats, sizeof(nx_adc_stats_t));
    return NX_OK;
}

/**
 * \brief           Get ADC statistics
 */
static nx_status_t adc_diagnostic_get_statistics(nx_diagnostic_t* self,
                                                 void* stats, size_t size) {
    nx_adc_impl_t* impl = adc_get_impl_from_diagnostic(self);
    if (!impl || !impl->state || !stats) {
        return NX_ERR_NULL_PTR;
    }
    if (size < sizeof(nx_adc_stats_t)) {
        return NX_ERR_DATA_SIZE;
    }

    memcpy(stats, &impl->state->stats, sizeof(nx_adc_stats_t));
    return NX_OK;
}

/**
 * \brief           Clear ADC statistics
 */
static nx_status_t adc_diagnostic_clear_statistics(nx_diagnostic_t* self) {
    nx_adc_impl_t* impl = adc_get_impl_from_diagnostic(self);
    if (!impl || !impl->state) {
        return NX_ERR_NULL_PTR;
    }

    memset(&impl->state->stats, 0, sizeof(nx_adc_stats_t));
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize diagnostic interface
 */
void adc_init_diagnostic(nx_diagnostic_t* diagnostic) {
    diagnostic->get_status = adc_diagnostic_get_status;
    diagnostic->get_statistics = adc_diagnostic_get_statistics;
    diagnostic->clear_statistics = adc_diagnostic_clear_statistics;
}
