/**
 * \file            native_crc_helpers.c
 * \brief           Native CRC test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_crc_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/crc/nx_crc_types.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* Internal Helper - Get CRC Implementation                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get CRC implementation structure from device
 */
static nx_crc_impl_t* get_crc_impl(uint8_t index) {
    nx_crc_t* crc = nx_factory_crc(index);
    if (crc == NULL) {
        return NULL;
    }

    /* The impl structure contains the base as first member */
    /* So we can cast back from base to impl */
    return (nx_crc_impl_t*)crc;
}

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get CRC device state
 */
nx_status_t native_crc_get_state(uint8_t index, bool* initialized,
                                 bool* suspended) {
    nx_crc_impl_t* impl = get_crc_impl(index);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    if (initialized != NULL) {
        *initialized = impl->state->initialized;
    }

    if (suspended != NULL) {
        *suspended = impl->state->suspended;
    }

    return NX_OK;
}

/**
 * \brief           Reset all CRC instances to initial state
 */
void native_crc_reset_all(void) {
    /* Iterate through all possible CRC instances */
    for (uint8_t i = 0; i < 4; i++) {
        nx_crc_impl_t* impl = get_crc_impl(i);
        if (impl == NULL || impl->state == NULL) {
            continue;
        }

        /* Reset state */
        impl->state->initialized = false;
        impl->state->suspended = false;
        impl->state->current_crc = impl->state->config.init_value;

        /* Clear statistics */
        memset(&impl->state->stats, 0, sizeof(impl->state->stats));
    }
}
