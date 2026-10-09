/**
 * \file            nx_sdio_helpers.c
 * \brief           SDIO helper functions implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements SDIO helper functions for block storage
 *                  operations, card simulation, and state management.
 */

#include "nx_sdio_helpers.h"
#include "hal/nx_status.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Block Storage Operations                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Read blocks from storage
 */
nx_status_t sdio_read_blocks(nx_sdio_state_t* state, uint32_t block,
                             uint8_t* data, size_t block_count) {
    if (!state || !data) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->card_present) {
        return NX_ERR_INVALID_STATE;
    }

    /* Check block range */
    if (block + block_count > NX_SDIO_NUM_BLOCKS) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Copy data from blocks */
    for (size_t i = 0; i < block_count; i++) {
        memcpy(data + (i * NX_SDIO_BLOCK_SIZE), state->blocks[block + i].data,
               NX_SDIO_BLOCK_SIZE);
    }

    state->stats.read_count++;
    return NX_OK;
}

/**
 * \brief           Write blocks to storage
 */
nx_status_t sdio_write_blocks(nx_sdio_state_t* state, uint32_t block,
                              const uint8_t* data, size_t block_count) {
    if (!state || !data) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->card_present) {
        return NX_ERR_INVALID_STATE;
    }

    /* Check block range */
    if (block + block_count > NX_SDIO_NUM_BLOCKS) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Copy data to blocks */
    for (size_t i = 0; i < block_count; i++) {
        memcpy(state->blocks[block + i].data, data + (i * NX_SDIO_BLOCK_SIZE),
               NX_SDIO_BLOCK_SIZE);
    }

    state->stats.write_count++;
    return NX_OK;
}

/**
 * \brief           Erase blocks
 */
nx_status_t sdio_erase_blocks(nx_sdio_state_t* state, uint32_t start_block,
                              size_t block_count) {
    if (!state) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    if (!state->card_present) {
        return NX_ERR_INVALID_STATE;
    }

    /* Check block range */
    if (start_block + block_count > NX_SDIO_NUM_BLOCKS) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Erase blocks (fill with 0xFF) */
    for (size_t i = 0; i < block_count; i++) {
        memset(state->blocks[start_block + i].data, 0xFF, NX_SDIO_BLOCK_SIZE);
    }

    state->stats.erase_count++;
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Card Simulation                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simulate card detection
 */
bool sdio_is_card_present(nx_sdio_state_t* state) {
    if (!state) {
        return false;
    }
    return state->card_present;
}

/**
 * \brief           Initialize card simulation
 */
nx_status_t sdio_init_card(nx_sdio_state_t* state) {
    if (!state) {
        return NX_ERR_NULL_PTR;
    }

    /* Simulate card initialization */
    if (!state->card_present) {
        return NX_ERR_INVALID_STATE;
    }

    /* Initialize all blocks to erased state (0xFF) */
    for (size_t i = 0; i < NX_SDIO_NUM_BLOCKS; i++) {
        memset(state->blocks[i].data, 0xFF, NX_SDIO_BLOCK_SIZE);
    }

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* State Management                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset SDIO state for testing
 */
void sdio_reset_state(nx_sdio_state_t* state) {
    if (!state) {
        return;
    }

    state->initialized = false;
    state->suspended = false;
    state->card_present = false;
    memset(&state->stats, 0, sizeof(nx_sdio_stats_t));

    /* Clear all blocks */
    if (state->blocks) {
        for (size_t i = 0; i < NX_SDIO_NUM_BLOCKS; i++) {
            memset(state->blocks[i].data, 0, NX_SDIO_BLOCK_SIZE);
        }
    }
}
