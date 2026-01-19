/**
 * \file            nx_option_bytes_helpers.h
 * \brief           Native Option Bytes helper functions
 * \author          Nexus Team
 */

#ifndef NX_OPTION_BYTES_HELPERS_H
#define NX_OPTION_BYTES_HELPERS_H

#include "hal/nx_status.h"
#include "nx_option_bytes_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Option Bytes Operations                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Read user data from option bytes
 */
nx_status_t option_bytes_read_user_data(nx_option_bytes_state_t* state,
                                        uint8_t* data, size_t len);

/**
 * \brief           Write user data to option bytes (pending)
 */
nx_status_t option_bytes_write_user_data(nx_option_bytes_state_t* state,
                                         const uint8_t* data, size_t len);

/**
 * \brief           Get read protection level
 */
uint8_t option_bytes_get_read_protection(nx_option_bytes_state_t* state);

/**
 * \brief           Set read protection level (pending)
 */
nx_status_t option_bytes_set_read_protection(nx_option_bytes_state_t* state,
                                             uint8_t level);

/**
 * \brief           Apply pending changes
 */
nx_status_t option_bytes_apply(nx_option_bytes_state_t* state);

/*---------------------------------------------------------------------------*/
/* Option Bytes Validation                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate read protection level
 */
bool option_bytes_is_valid_protection_level(uint8_t level);

/**
 * \brief           Check if write is allowed
 */
bool option_bytes_is_write_allowed(nx_option_bytes_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NX_OPTION_BYTES_HELPERS_H */
