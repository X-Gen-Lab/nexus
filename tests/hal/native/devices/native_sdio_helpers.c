/**
 * \file            native_sdio_helpers.c
 * \brief           Native SDIO test helpers implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "native_sdio_helpers.h"
#include "../../../../platforms/native/src/sdio/nx_sdio_types.h"
#include "hal/nx_factory.h"
#include <string.h>

static nx_sdio_impl_t* get_sdio_impl(uint8_t index) {
    nx_sdio_t* sdio = nx_factory_sdio(index);
    return sdio ? (nx_sdio_impl_t*)sdio : NULL;
}

nx_status_t native_sdio_get_state(uint8_t index, bool* initialized,
                                  bool* suspended) {
    nx_sdio_impl_t* impl = get_sdio_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_PARAM;
    if (initialized)
        *initialized = impl->state->initialized;
    if (suspended)
        *suspended = impl->state->suspended;
    return NX_OK;
}

nx_status_t native_sdio_set_card_present(uint8_t index, bool present) {
    nx_sdio_impl_t* impl = get_sdio_impl(index);
    if (!impl || !impl->state)
        return NX_ERR_INVALID_PARAM;
    impl->state->card_present = present;
    return NX_OK;
}

bool native_sdio_is_card_present(uint8_t index) {
    nx_sdio_impl_t* impl = get_sdio_impl(index);
    return impl && impl->state ? impl->state->card_present : false;
}

nx_status_t native_sdio_get_block_data(uint8_t index, uint32_t block,
                                       uint8_t* data) {
    nx_sdio_impl_t* impl = get_sdio_impl(index);
    if (!impl || !impl->state || !data)
        return NX_ERR_INVALID_PARAM;
    if (block >= NX_SDIO_NUM_BLOCKS)
        return NX_ERR_INVALID_PARAM;
    if (!impl->state->blocks)
        return NX_ERR_INVALID_STATE;
    memcpy(data, impl->state->blocks[block].data, NX_SDIO_BLOCK_SIZE);
    return NX_OK;
}

void native_sdio_reset(uint8_t index) {
    nx_sdio_impl_t* impl = get_sdio_impl(index);
    if (!impl || !impl->state)
        return;

    /* Save blocks pointer and config before reset */
    nx_sdio_block_t* blocks = impl->state->blocks;
    nx_sdio_config_t config = impl->state->config;

    /* Reset state */
    memset(impl->state, 0, sizeof(*impl->state));

    /* Restore blocks pointer, config, and index */
    impl->state->blocks = blocks;
    impl->state->config = config;
    impl->state->index = index;

    /* Clear all blocks if allocated */
    if (blocks) {
        for (size_t i = 0; i < NX_SDIO_NUM_BLOCKS; i++) {
            memset(blocks[i].data, 0, NX_SDIO_BLOCK_SIZE);
        }
    }
}

void native_sdio_reset_all(void) {
    for (uint8_t i = 0; i < 4; i++) {
        native_sdio_reset(i);
    }
}
