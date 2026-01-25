/**
 * \file            native_flash_helpers.c
 * \brief           Native Flash test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_flash_helpers.h"
#include "hal/nx_factory.h"

/* Include platform-specific types */
#include "../../../../platforms/native/src/flash/nx_flash_types.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* Internal Helper - Get Flash Implementation                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Flash implementation structure from device
 */
static nx_flash_impl_t* get_flash_impl(uint8_t index) {
    nx_internal_flash_t* flash = nx_factory_flash(index);
    if (flash == NULL) {
        return NULL;
    }

    /* The impl structure contains the base as first member */
    return (nx_flash_impl_t*)flash;
}

/*---------------------------------------------------------------------------*/
/* Test Helper Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get Flash device state
 */
nx_status_t native_flash_get_state(uint8_t index, bool* initialized,
                                   bool* suspended) {
    nx_flash_impl_t* impl = get_flash_impl(index);
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
 * \brief           Get Flash lock status
 */
nx_status_t native_flash_get_lock_status(uint8_t index, bool* locked) {
    nx_flash_impl_t* impl = get_flash_impl(index);
    if (impl == NULL || impl->state == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    if (locked != NULL) {
        *locked = impl->state->locked;
    }

    return NX_OK;
}

/**
 * \brief           Check if Flash region is erased
 */
bool native_flash_is_erased(uint8_t index, uint32_t address, uint32_t length) {
    nx_flash_impl_t* impl = get_flash_impl(index);
    if (impl == NULL || impl->state == NULL) {
        return false;
    }

    /* Check if address and length are valid */
    if (address + length > NX_FLASH_TOTAL_SIZE) {
        return false;
    }

    /* Check each byte in the range */
    for (uint32_t i = 0; i < length; i++) {
        uint32_t addr = address + i;
        uint32_t sector_idx = addr / NX_FLASH_SECTOR_SIZE;
        uint32_t byte_idx = addr % NX_FLASH_SECTOR_SIZE;

        if (impl->state->sectors[sector_idx].data[byte_idx] !=
            NX_FLASH_ERASED_BYTE) {
            return false;
        }
    }

    return true;
}

/**
 * \brief           Reset all Flash instances to initial state
 */
void native_flash_reset_all(void) {
    /* Iterate through all possible Flash instances */
    for (uint8_t i = 0; i < 4; i++) {
        nx_flash_impl_t* impl = get_flash_impl(i);
        if (impl == NULL || impl->state == NULL) {
            continue;
        }

        /* Reset state */
        impl->state->initialized = false;
        impl->state->suspended = false;
        impl->state->locked = true;

        /* Initialize all sectors as erased */
        for (uint32_t j = 0; j < NX_FLASH_NUM_SECTORS; j++) {
            memset(impl->state->sectors[j].data, NX_FLASH_ERASED_BYTE,
                   NX_FLASH_SECTOR_SIZE);
            impl->state->sectors[j].erased = true;
        }
    }
}
