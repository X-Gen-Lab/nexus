/**
 * \file            nx_flash_helpers.h
 * \brief           Native Flash helper functions
 * \author          Nexus Team
 */

#ifndef NX_FLASH_HELPERS_H
#define NX_FLASH_HELPERS_H

#include "hal/nx_status.h"
#include "nx_flash_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Flash Storage Operations                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Erase flash sector
 */
nx_status_t flash_erase_sector(nx_flash_state_t* state, uint32_t sector);

/**
 * \brief           Write data to flash
 */
nx_status_t flash_write(nx_flash_state_t* state, uint32_t addr,
                        const uint8_t* data, size_t len);

/**
 * \brief           Read data from flash
 */
nx_status_t flash_read(nx_flash_state_t* state, uint32_t addr, uint8_t* data,
                       size_t len);

/**
 * \brief           Check if address range is erased
 */
bool flash_is_erased(nx_flash_state_t* state, uint32_t addr, size_t len);

/*---------------------------------------------------------------------------*/
/* Flash Persistence                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Save flash contents to file
 */
nx_status_t flash_save_to_file(nx_flash_state_t* state);

/**
 * \brief           Load flash contents from file
 */
nx_status_t flash_load_from_file(nx_flash_state_t* state);

/*---------------------------------------------------------------------------*/
/* Flash Validation                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate flash address
 */
bool flash_is_valid_address(uint32_t addr, size_t len);

/**
 * \brief           Validate write alignment
 */
bool flash_is_aligned(uint32_t addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* NX_FLASH_HELPERS_H */
