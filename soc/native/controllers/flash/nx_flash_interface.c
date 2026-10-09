/**
 * \file            nx_flash_interface.c
 * \brief           Flash interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements the nx_internal_flash_t interface functions
 *                  for the Native platform flash simulation.
 */

#include "nx_flash_helpers.h"
#include "nx_flash_types.h"

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get flash state from interface
 */
static nx_flash_state_t* get_flash_state(nx_internal_flash_t* self) {
    if (self == NULL) {
        return NULL;
    }

    nx_flash_impl_t* impl =
        (nx_flash_impl_t*)((uint8_t*)self - offsetof(nx_flash_impl_t, base));
    return impl->state;
}

/*---------------------------------------------------------------------------*/
/* Flash Interface Implementation                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Read data from flash
 */
static nx_status_t flash_read_impl(nx_internal_flash_t* self, uint32_t addr,
                                   uint8_t* data, size_t len) {
    nx_flash_state_t* state = get_flash_state(self);
    if (state == NULL || data == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    return flash_read(state, addr, data, len);
}

/**
 * \brief           Write data to flash
 */
static nx_status_t flash_write_impl(nx_internal_flash_t* self, uint32_t addr,
                                    const uint8_t* data, size_t len) {
    nx_flash_state_t* state = get_flash_state(self);
    if (state == NULL || data == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    return flash_write(state, addr, data, len);
}

/**
 * \brief           Erase flash pages
 */
static nx_status_t flash_erase_impl(nx_internal_flash_t* self, uint32_t addr,
                                    size_t size) {
    nx_flash_state_t* state = get_flash_state(self);
    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (state->suspended) {
        return NX_ERR_INVALID_STATE;
    }

    if (state->locked) {
        return NX_ERR_PERMISSION;
    }

    /* Calculate sector range */
    uint32_t start_sector = addr / NX_FLASH_SECTOR_SIZE;
    uint32_t end_addr = addr + (uint32_t)size;
    uint32_t end_sector =
        (end_addr + NX_FLASH_SECTOR_SIZE - 1) / NX_FLASH_SECTOR_SIZE;

    /* Erase sectors */
    for (uint32_t sector = start_sector; sector < end_sector; sector++) {
        nx_status_t status = flash_erase_sector(state, sector);
        if (status != NX_OK) {
            return status;
        }
    }

    return NX_OK;
}

/**
 * \brief           Get flash page size
 */
static size_t flash_get_page_size_impl(nx_internal_flash_t* self) {
    (void)self;
    return NX_FLASH_SECTOR_SIZE;
}

/**
 * \brief           Get minimum write unit size
 */
static size_t flash_get_write_unit_impl(nx_internal_flash_t* self) {
    (void)self;
    return NX_FLASH_WRITE_UNIT;
}

/**
 * \brief           Lock flash for write protection
 */
static nx_status_t flash_lock_impl(nx_internal_flash_t* self) {
    nx_flash_state_t* state = get_flash_state(self);
    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->locked = true;
    return NX_OK;
}

/**
 * \brief           Unlock flash for write/erase operations
 */
static nx_status_t flash_unlock_impl(nx_internal_flash_t* self) {
    nx_flash_state_t* state = get_flash_state(self);
    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    state->locked = false;
    return NX_OK;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* flash_get_lifecycle_impl(nx_internal_flash_t* self) {
    if (self == NULL) {
        return NULL;
    }

    nx_flash_impl_t* impl =
        (nx_flash_impl_t*)((uint8_t*)self - offsetof(nx_flash_impl_t, base));
    return &impl->lifecycle;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize flash interface
 */
void flash_init_interface(nx_internal_flash_t* flash) {
    if (flash == NULL) {
        return;
    }

    NX_INIT_INTERNAL_FLASH(flash, flash_read_impl, flash_write_impl,
                           flash_erase_impl, flash_get_page_size_impl,
                           flash_get_write_unit_impl, flash_lock_impl,
                           flash_unlock_impl, flash_get_lifecycle_impl);
}
