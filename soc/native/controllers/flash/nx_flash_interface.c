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
#include "arch/nx_arch.h"
#include "osal/osal.h"
#include <limits.h>
#include <string.h>

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

    /* Exact physical erase blocks only, including the private legacy bridge.
     * Subtraction checks reject host size_t values that do not fit uint32_t. */
    if (!size || !flash_is_valid_address(addr, size) ||
        addr % NX_FLASH_SECTOR_SIZE || size % NX_FLASH_SECTOR_SIZE)
        return NX_ERR_INVALID_PARAM;
    uint32_t first = addr / NX_FLASH_SECTOR_SIZE;
    uint32_t count = (uint32_t)(size / NX_FLASH_SECTOR_SIZE);
    for (uint32_t sector = first; sector < first + count; ++sector) {
        nx_status_t status = flash_erase_sector(state, sector);
        if (status != NX_OK)
            return status;
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

/** The Native provider models a fixed 512 KiB host backing image. */
static nx_flash_impl_t* from_operations(nx_flash_operations_t* self) {
    return self ? NX_CONTAINER_OF(self, nx_flash_impl_t, operations) : NULL;
}
static nx_status_t typed_ready(nx_flash_impl_t* impl) {
    if (nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERR_INVALID_STATE;
    return !impl || !impl->state || !impl->state->initialized ? NX_ERR_NOT_INIT
           : impl->state->suspended ? NX_ERR_INVALID_STATE
                                    : NX_OK;
}
static nx_status_t geometry(nx_flash_operations_t* self,
                            nx_flash_geometry_t* out) {
    if (!out)
        return NX_ERR_NULL_PTR;
    *out = (nx_flash_geometry_t){0};
    nx_status_t r = typed_ready(from_operations(self));
    if (r == NX_OK)
        *out = (nx_flash_geometry_t){.base_address = 0,
                                     .size_bytes = NX_FLASH_TOTAL_SIZE,
                                     .program_alignment = NX_FLASH_WRITE_UNIT,
                                     .block_count = NX_FLASH_NUM_SECTORS,
                                     .flags = 0,
                                     .erased_value = NX_FLASH_ERASED_BYTE};
    return r;
}
static nx_status_t block(nx_flash_operations_t* self, uint32_t offset,
                         nx_flash_block_t* out) {
    if (!out)
        return NX_ERR_NULL_PTR;
    *out = (nx_flash_block_t){0};
    nx_status_t r = typed_ready(from_operations(self));
    if (r != NX_OK)
        return r;
    if (offset >= NX_FLASH_TOTAL_SIZE)
        return NX_ERR_INVALID_PARAM;
    *out = (nx_flash_block_t){.offset = offset / NX_FLASH_SECTOR_SIZE *
                                        NX_FLASH_SECTOR_SIZE,
                              .size = NX_FLASH_SECTOR_SIZE,
                              .index = offset / NX_FLASH_SECTOR_SIZE};
    return NX_OK;
}
static nx_status_t typed_read(nx_flash_operations_t* self, uint32_t offset,
                              uint8_t* data, size_t length) {
    nx_flash_impl_t* impl = from_operations(self);
    nx_status_t r = typed_ready(impl);
    if (r != NX_OK)
        return r;
    if (!length)
        return NX_ERR_INVALID_PARAM;
    return flash_read_impl(&impl->base, offset, data, length);
}
static nx_status_t budget_start(nx_flash_impl_t* impl, uint32_t budget,
                                uint32_t* started) {
    nx_status_t r = typed_ready(impl);
    if (r != NX_OK)
        return r;
    if (!budget)
        return NX_ERR_TIMEOUT;
    if (budget > INT32_MAX && budget != UINT32_MAX)
        return NX_ERR_INVALID_PARAM;
    return osal_get_time_ms(started) == OSAL_OK ? NX_OK : NX_ERR_NOT_INIT;
}
static nx_status_t budget_finish(nx_status_t status, uint32_t started,
                                 uint32_t budget) {
    if (status != NX_OK)
        return status;
    uint32_t ended = started;
    if (osal_get_time_ms(&ended) != OSAL_OK)
        return NX_ERR_IO;
    return budget != UINT32_MAX && (uint32_t)(ended - started) >= budget
               ? NX_ERR_TIMEOUT
               : NX_OK;
}
static nx_status_t typed_program(nx_flash_operations_t* self, uint32_t offset,
                                 const uint8_t* data, size_t length,
                                 uint32_t budget) {
    nx_flash_impl_t* impl = from_operations(self);
    uint32_t started = 0;
    nx_status_t r = budget_start(impl, budget, &started);
    if (r != NX_OK)
        return r;
    if (!length)
        return NX_ERR_INVALID_PARAM;
    return budget_finish(flash_write_impl(&impl->base, offset, data, length),
                         started, budget);
}
static nx_status_t typed_erase(nx_flash_operations_t* self, uint32_t offset,
                               size_t length, uint32_t budget) {
    nx_flash_impl_t* impl = from_operations(self);
    uint32_t started = 0;
    nx_status_t r = budget_start(impl, budget, &started);
    if (r != NX_OK)
        return r;
    return budget_finish(flash_erase_impl(&impl->base, offset, length), started,
                         budget);
}
static nx_status_t typed_sync(nx_flash_operations_t* self, uint32_t budget) {
    nx_flash_impl_t* impl = from_operations(self);
    uint32_t started = 0;
    nx_status_t r = budget_start(impl, budget, &started);
    if (r != NX_OK)
        return r;
    /* File IO can exceed a requested deadline; it still completes/settles
     * before return, then reports TIMEOUT. This is not an MCU timing model. */
    return budget_finish(flash_save_to_file(impl->state), started, budget);
}
static nx_flash_operations_t* get_operations(nx_internal_flash_t* self) {
    return self ? &NX_CONTAINER_OF(self, nx_flash_impl_t, base)->operations
                : NULL;
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
    nx_flash_impl_t* impl = NX_CONTAINER_OF(flash, nx_flash_impl_t, base);
    impl->operations = (nx_flash_operations_t){.get_geometry = geometry,
                                               .get_block = block,
                                               .read = typed_read,
                                               .program = typed_program,
                                               .erase = typed_erase,
                                               .sync = typed_sync};
    flash->get_operations = get_operations;
}
