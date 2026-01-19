/**
 * \file            nx_flash_helpers.c
 * \brief           Native Flash helper functions implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements flash storage simulation including sector erase,
 *                  write with erase check, read operations, and persistence.
 */

#include "nx_flash_helpers.h"
#include "hal/nx_types.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Flash Storage Operations                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Erase flash sector
 */
nx_status_t flash_erase_sector(nx_flash_state_t* state, uint32_t sector) {
    NX_ASSERT(state != NULL);

    if (sector >= NX_FLASH_NUM_SECTORS) {
        return NX_ERR_INVALID_PARAM;
    }

    if (state->locked) {
        return NX_ERR_PERMISSION;
    }

    /* Set all bytes to erased value */
    memset(state->sectors[sector].data, NX_FLASH_ERASED_BYTE,
           NX_FLASH_SECTOR_SIZE);
    state->sectors[sector].erased = true;

    return NX_OK;
}

/**
 * \brief           Write data to flash
 */
nx_status_t flash_write(nx_flash_state_t* state, uint32_t addr,
                        const uint8_t* data, size_t len) {
    NX_ASSERT(state != NULL);
    NX_ASSERT(data != NULL);

    if (!flash_is_valid_address(addr, len)) {
        return NX_ERR_INVALID_PARAM;
    }

    if (!flash_is_aligned(addr, len)) {
        return NX_ERR_INVALID_PARAM;
    }

    if (state->locked) {
        return NX_ERR_PERMISSION;
    }

    /* Check if area is erased before writing */
    if (!flash_is_erased(state, addr, len)) {
        return NX_ERR_INVALID_STATE;
    }

    /* Write data */
    uint32_t current_addr = addr;
    size_t remaining = len;
    size_t offset = 0;

    while (remaining > 0) {
        uint32_t sector = current_addr / NX_FLASH_SECTOR_SIZE;
        uint32_t sector_offset = current_addr % NX_FLASH_SECTOR_SIZE;
        size_t chunk_size = NX_FLASH_SECTOR_SIZE - sector_offset;

        if (chunk_size > remaining) {
            chunk_size = remaining;
        }

        memcpy(&state->sectors[sector].data[sector_offset], &data[offset],
               chunk_size);
        state->sectors[sector].erased = false;

        current_addr += (uint32_t)chunk_size;
        offset += chunk_size;
        remaining -= chunk_size;
    }

    return NX_OK;
}

/**
 * \brief           Read data from flash
 */
nx_status_t flash_read(nx_flash_state_t* state, uint32_t addr, uint8_t* data,
                       size_t len) {
    NX_ASSERT(state != NULL);
    NX_ASSERT(data != NULL);

    if (!flash_is_valid_address(addr, len)) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Read data */
    uint32_t current_addr = addr;
    size_t remaining = len;
    size_t offset = 0;

    while (remaining > 0) {
        uint32_t sector = current_addr / NX_FLASH_SECTOR_SIZE;
        uint32_t sector_offset = current_addr % NX_FLASH_SECTOR_SIZE;
        size_t chunk_size = NX_FLASH_SECTOR_SIZE - sector_offset;

        if (chunk_size > remaining) {
            chunk_size = remaining;
        }

        memcpy(&data[offset], &state->sectors[sector].data[sector_offset],
               chunk_size);

        current_addr += (uint32_t)chunk_size;
        offset += chunk_size;
        remaining -= chunk_size;
    }

    return NX_OK;
}

/**
 * \brief           Check if address range is erased
 */
bool flash_is_erased(nx_flash_state_t* state, uint32_t addr, size_t len) {
    NX_ASSERT(state != NULL);

    if (!flash_is_valid_address(addr, len)) {
        return false;
    }

    uint32_t current_addr = addr;
    size_t remaining = len;

    while (remaining > 0) {
        uint32_t sector = current_addr / NX_FLASH_SECTOR_SIZE;
        uint32_t sector_offset = current_addr % NX_FLASH_SECTOR_SIZE;
        size_t chunk_size = NX_FLASH_SECTOR_SIZE - sector_offset;

        if (chunk_size > remaining) {
            chunk_size = remaining;
        }

        /* Check if bytes are erased */
        for (size_t i = 0; i < chunk_size; i++) {
            if (state->sectors[sector].data[sector_offset + i] !=
                NX_FLASH_ERASED_BYTE) {
                return false;
            }
        }

        current_addr += (uint32_t)chunk_size;
        remaining -= chunk_size;
    }

    return true;
}

/*---------------------------------------------------------------------------*/
/* Flash Validation                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate flash address
 */
bool flash_is_valid_address(uint32_t addr, size_t len) {
    if (addr >= NX_FLASH_TOTAL_SIZE) {
        return false;
    }

    if (addr + len > NX_FLASH_TOTAL_SIZE) {
        return false;
    }

    return true;
}

/**
 * \brief           Validate write alignment
 */
bool flash_is_aligned(uint32_t addr, size_t len) {
    if (addr % NX_FLASH_WRITE_UNIT != 0) {
        return false;
    }

    if (len % NX_FLASH_WRITE_UNIT != 0) {
        return false;
    }

    return true;
}

/*---------------------------------------------------------------------------*/
/* Flash Persistence                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Save flash contents to file
 */
nx_status_t flash_save_to_file(nx_flash_state_t* state) {
    NX_ASSERT(state != NULL);

    if (state->backing_file[0] == '\0') {
        return NX_ERR_INVALID_PARAM;
    }

    FILE* file = fopen(state->backing_file, "wb");
    if (file == NULL) {
        return NX_ERR_IO;
    }

    /* Write all sectors to file */
    for (uint32_t i = 0; i < NX_FLASH_NUM_SECTORS; i++) {
        size_t written =
            fwrite(state->sectors[i].data, 1, NX_FLASH_SECTOR_SIZE, file);
        if (written != NX_FLASH_SECTOR_SIZE) {
            fclose(file);
            return NX_ERR_IO;
        }
    }

    fclose(file);
    return NX_OK;
}

/**
 * \brief           Load flash contents from file
 */
nx_status_t flash_load_from_file(nx_flash_state_t* state) {
    NX_ASSERT(state != NULL);

    if (state->backing_file[0] == '\0') {
        return NX_ERR_INVALID_PARAM;
    }

    FILE* file = fopen(state->backing_file, "rb");
    if (file == NULL) {
        /* File doesn't exist, initialize with erased state */
        for (uint32_t i = 0; i < NX_FLASH_NUM_SECTORS; i++) {
            memset(state->sectors[i].data, NX_FLASH_ERASED_BYTE,
                   NX_FLASH_SECTOR_SIZE);
            state->sectors[i].erased = true;
        }
        return NX_OK;
    }

    /* Read all sectors from file */
    for (uint32_t i = 0; i < NX_FLASH_NUM_SECTORS; i++) {
        size_t read_bytes =
            fread(state->sectors[i].data, 1, NX_FLASH_SECTOR_SIZE, file);
        if (read_bytes != NX_FLASH_SECTOR_SIZE) {
            /* Partial read or error, initialize remaining with erased state */
            if (read_bytes > 0) {
                memset(&state->sectors[i].data[read_bytes],
                       NX_FLASH_ERASED_BYTE, NX_FLASH_SECTOR_SIZE - read_bytes);
            }
            state->sectors[i].erased = false;

            /* Initialize remaining sectors */
            for (uint32_t j = i + 1; j < NX_FLASH_NUM_SECTORS; j++) {
                memset(state->sectors[j].data, NX_FLASH_ERASED_BYTE,
                       NX_FLASH_SECTOR_SIZE);
                state->sectors[j].erased = true;
            }
            break;
        }

        /* Check if sector is erased */
        state->sectors[i].erased = true;
        for (size_t j = 0; j < NX_FLASH_SECTOR_SIZE; j++) {
            if (state->sectors[i].data[j] != NX_FLASH_ERASED_BYTE) {
                state->sectors[i].erased = false;
                break;
            }
        }
    }

    fclose(file);
    return NX_OK;
}
