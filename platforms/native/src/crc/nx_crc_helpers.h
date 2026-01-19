/**
 * \file            nx_crc_helpers.h
 * \brief           CRC helper functions for Native platform
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NX_CRC_HELPERS_H
#define NX_CRC_HELPERS_H

#include "hal/nx_status.h"
#include "nx_crc_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get CRC implementation from base interface
 * \param[in]       self: CRC interface pointer
 * \return          Implementation pointer or NULL
 * \note            Optimized as inline for performance
 */
static inline nx_crc_impl_t* crc_get_impl(nx_crc_t* self) {
    return self ? (nx_crc_impl_t*)self : NULL;
}

/**
 * \brief           Calculate CRC-32 (IEEE 802.3)
 * \param[in]       data: Data buffer
 * \param[in]       len: Data length
 * \param[in]       init: Initial CRC value
 * \return          CRC-32 value
 */
uint32_t crc32_calculate(const uint8_t* data, size_t len, uint32_t init);

/**
 * \brief           Calculate CRC-16 (CCITT)
 * \param[in]       data: Data buffer
 * \param[in]       len: Data length
 * \param[in]       init: Initial CRC value
 * \return          CRC-16 value
 */
uint16_t crc16_calculate(const uint8_t* data, size_t len, uint16_t init);

/**
 * \brief           Reset CRC state for testing
 * \param[in]       state: CRC state pointer
 * \note            This function is for testing purposes only
 */
void crc_reset_state(nx_crc_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NX_CRC_HELPERS_H */
