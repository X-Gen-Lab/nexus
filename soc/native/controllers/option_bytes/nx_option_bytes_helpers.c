/**
 * \file            nx_option_bytes_helpers.c
 * \brief           Native Option Bytes helper functions implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-19
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements helper functions for option bytes operations
 *                  including read/write of user data, protection levels,
 *                  and validation logic.
 */

#include "nx_option_bytes_helpers.h"
#include "hal/nx_status.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Option Bytes Operations                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Read user data from option bytes
 */
nx_status_t option_bytes_read_user_data(nx_option_bytes_state_t* state,
                                        uint8_t* data, size_t len) {
    if (state == NULL || data == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (len == 0 || len > NX_OPTION_BYTES_USER_DATA_SIZE) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Copy user data */
    memcpy(data, state->data.user_data, len);

    return NX_OK;
}

/**
 * \brief           Write user data to option bytes (pending)
 */
nx_status_t option_bytes_write_user_data(nx_option_bytes_state_t* state,
                                         const uint8_t* data, size_t len) {
    if (state == NULL || data == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (len == 0 || len > NX_OPTION_BYTES_USER_DATA_SIZE) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check write protection */
    if (!option_bytes_is_write_allowed(state)) {
        return NX_ERR_PERMISSION;
    }

    /* Write to pending buffer */
    memcpy(state->pending.user_data, data, len);
    state->pending.pending_changes = true;

    return NX_OK;
}

/**
 * \brief           Get read protection level
 */
uint8_t option_bytes_get_read_protection(nx_option_bytes_state_t* state) {
    if (state == NULL) {
        return 0;
    }

    return state->data.read_protection;
}

/**
 * \brief           Set read protection level (pending)
 */
nx_status_t option_bytes_set_read_protection(nx_option_bytes_state_t* state,
                                             uint8_t level) {
    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    /* Validate protection level */
    if (!option_bytes_is_valid_protection_level(level)) {
        return NX_ERR_INVALID_PARAM;
    }

    /* Check write protection */
    if (!option_bytes_is_write_allowed(state)) {
        return NX_ERR_PERMISSION;
    }

    /* Set pending protection level */
    state->pending.read_protection = level;
    state->pending.pending_changes = true;

    return NX_OK;
}

/**
 * \brief           Apply pending changes
 */
nx_status_t option_bytes_apply(nx_option_bytes_state_t* state) {
    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    /* Check if there are pending changes */
    if (!state->pending.pending_changes) {
        return NX_OK; /* Nothing to apply */
    }

    /* Check write protection */
    if (!option_bytes_is_write_allowed(state)) {
        return NX_ERR_PERMISSION;
    }

    /* Apply pending changes */
    memcpy(&state->data, &state->pending, sizeof(nx_option_bytes_data_t));
    state->pending.pending_changes = false;

    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Option Bytes Validation                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate read protection level
 */
bool option_bytes_is_valid_protection_level(uint8_t level) {
    /* Valid levels: 0 (none), 1 (level1), 2 (level2) */
    return (level <= 2);
}

/**
 * \brief           Check if write is allowed
 */
bool option_bytes_is_write_allowed(nx_option_bytes_state_t* state) {
    if (state == NULL) {
        return false;
    }

    /* Write not allowed if write protected */
    return !state->data.write_protected;
}
