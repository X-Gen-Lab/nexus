/**
 * \file            nx_crc_interface.c
 * \brief           CRC interface implementation for Native platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-18
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements CRC interface functions (reset, update,
 *                  get_result, calculate, set_polynomial, get_lifecycle).
 */

#include "nx_crc_helpers.h"
#include "nx_crc_types.h"

/*---------------------------------------------------------------------------*/
/* CRC Interface Implementation                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset CRC calculation
 */
static void crc_reset(nx_crc_t* self) {
    if (self == NULL) {
        return;
    }

    nx_crc_impl_t* impl = crc_get_impl(self);
    nx_crc_state_t* state = impl->state;

    if (state == NULL || !state->initialized) {
        return;
    }

    /* Reset CRC to initial value */
    state->current_crc = state->config.init_value;

    /* Update statistics */
    state->stats.reset_count++;
}

/**
 * \brief           Update CRC with data
 */
static void crc_update(nx_crc_t* self, const uint8_t* data, size_t len) {
    if (self == NULL || data == NULL || len == 0) {
        return;
    }

    nx_crc_impl_t* impl = crc_get_impl(self);
    nx_crc_state_t* state = impl->state;

    if (state == NULL || !state->initialized) {
        return;
    }

    /* Calculate CRC based on algorithm */
    if (state->config.algorithm == NX_CRC_ALGO_CRC32) {
        state->current_crc = crc32_calculate(data, len, state->current_crc);
    } else if (state->config.algorithm == NX_CRC_ALGO_CRC16) {
        state->current_crc =
            crc16_calculate(data, len, (uint16_t)state->current_crc);
    }

    /* Update statistics */
    state->stats.update_count++;
    state->stats.bytes_processed += (uint32_t)len;
}

/**
 * \brief           Get CRC result
 */
static uint32_t crc_get_result(nx_crc_t* self) {
    if (self == NULL) {
        return 0;
    }

    nx_crc_impl_t* impl = crc_get_impl(self);
    nx_crc_state_t* state = impl->state;

    if (state == NULL || !state->initialized) {
        return 0;
    }

    /* Apply final XOR */
    return state->current_crc ^ state->config.final_xor;
}

/**
 * \brief           Calculate CRC in one shot
 */
static uint32_t crc_calculate(nx_crc_t* self, const uint8_t* data, size_t len) {
    if (self == NULL || data == NULL || len == 0) {
        return 0;
    }

    nx_crc_impl_t* impl = crc_get_impl(self);
    nx_crc_state_t* state = impl->state;

    if (state == NULL || !state->initialized) {
        return 0;
    }

    uint32_t result;

    /* Calculate CRC based on algorithm */
    if (state->config.algorithm == NX_CRC_ALGO_CRC32) {
        result = crc32_calculate(data, len, state->config.init_value);
    } else if (state->config.algorithm == NX_CRC_ALGO_CRC16) {
        result = crc16_calculate(data, len, (uint16_t)state->config.init_value);
    } else {
        return 0;
    }

    /* Apply final XOR */
    result ^= state->config.final_xor;

    /* Update statistics */
    state->stats.calculate_count++;
    state->stats.bytes_processed += (uint32_t)len;

    return result;
}

/**
 * \brief           Set CRC polynomial
 */
static nx_status_t crc_set_polynomial(nx_crc_t* self, uint32_t polynomial) {
    if (self == NULL) {
        return NX_ERR_NULL_PTR;
    }

    nx_crc_impl_t* impl = crc_get_impl(self);
    nx_crc_state_t* state = impl->state;

    if (state == NULL) {
        return NX_ERR_NULL_PTR;
    }

    if (!state->initialized) {
        return NX_ERR_NOT_INIT;
    }

    /* Update polynomial (note: software implementation uses lookup tables) */
    state->config.polynomial = polynomial;

    /* Reset CRC to initial value when polynomial changes */
    state->current_crc = state->config.init_value;

    return NX_OK;
}

/**
 * \brief           Get lifecycle interface
 */
static nx_lifecycle_t* crc_get_lifecycle(nx_crc_t* self) {
    if (self == NULL) {
        return NULL;
    }

    nx_crc_impl_t* impl = crc_get_impl(self);
    return &impl->lifecycle;
}

/*---------------------------------------------------------------------------*/
/* CRC Interface Initialization                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize CRC interface
 */
void crc_init_interface(nx_crc_t* crc) {
    if (crc == NULL) {
        return;
    }

    crc->reset = crc_reset;
    crc->update = crc_update;
    crc->get_result = crc_get_result;
    crc->calculate = crc_calculate;
    crc->set_polynomial = crc_set_polynomial;
    crc->get_lifecycle = crc_get_lifecycle;
}
