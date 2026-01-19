/**
 * \file            nx_sdio_helpers.h
 * \brief           SDIO helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_SDIO_HELPERS_H
#define NX_SDIO_HELPERS_H

#include "hal/interface/nx_sdio.h"
#include "hal/nx_status.h"
#include "nx_sdio_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get SDIO implementation from base interface
 * \param[in]       self: SDIO interface pointer
 * \return          Implementation pointer or NULL
 * \note            Optimized as inline for performance
 */
static inline nx_sdio_impl_t* sdio_get_impl(nx_sdio_t* self) {
    return self ? (nx_sdio_impl_t*)self : NULL;
}

/**
 * \brief           Read blocks from storage
 * \param[in]       state: SDIO state pointer
 * \param[in]       block: Starting block number
 * \param[out]      data: Buffer to store read data
 * \param[in]       block_count: Number of blocks to read
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t sdio_read_blocks(nx_sdio_state_t* state, uint32_t block,
                             uint8_t* data, size_t block_count);

/**
 * \brief           Write blocks to storage
 * \param[in]       state: SDIO state pointer
 * \param[in]       block: Starting block number
 * \param[in]       data: Data buffer to write
 * \param[in]       block_count: Number of blocks to write
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t sdio_write_blocks(nx_sdio_state_t* state, uint32_t block,
                              const uint8_t* data, size_t block_count);

/**
 * \brief           Erase blocks
 * \param[in]       state: SDIO state pointer
 * \param[in]       start_block: Starting block number
 * \param[in]       block_count: Number of blocks to erase
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t sdio_erase_blocks(nx_sdio_state_t* state, uint32_t start_block,
                              size_t block_count);

/**
 * \brief           Simulate card detection
 * \param[in]       state: SDIO state pointer
 * \return          true if card is present, false otherwise
 */
bool sdio_is_card_present(nx_sdio_state_t* state);

/**
 * \brief           Initialize card simulation
 * \param[in]       state: SDIO state pointer
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t sdio_init_card(nx_sdio_state_t* state);

/**
 * \brief           Reset SDIO state for testing
 * \param[in]       state: SDIO state pointer
 * \note            This function is for testing purposes only
 */
void sdio_reset_state(nx_sdio_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NX_SDIO_HELPERS_H */
