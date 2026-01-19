/**
 * \file            native_crc_helpers.h
 * \brief           Native CRC test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_CRC_HELPERS_H
#define NATIVE_CRC_HELPERS_H

#include "hal/interface/nx_crc.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* CRC Test Helpers                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get CRC device state
 * \param[in]       index: CRC index
 * \param[out]      initialized: Initialization state (can be NULL)
 * \param[out]      suspended: Suspend state (can be NULL)
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t native_crc_get_state(uint8_t index, bool* initialized,
                                 bool* suspended);

/**
 * \brief           Reset all CRC instances to initial state
 * \note            This is useful for test isolation
 */
void native_crc_reset_all(void);

/*---------------------------------------------------------------------------*/
/* Usage Example                                                             */
/*---------------------------------------------------------------------------*/

/*
 * // Get device using nx_factory
 * nx_crc_t* crc = nx_factory_crc(0);
 * ASSERT_NE(crc, nullptr);
 *
 * // Initialize
 * nx_lifecycle_t* lifecycle = crc->get_lifecycle(crc);
 * ASSERT_EQ(lifecycle->init(lifecycle), NX_OK);
 *
 * // Use test helper to check state
 * bool initialized, suspended;
 * native_crc_get_state(0, &initialized, &suspended);
 * ASSERT_TRUE(initialized);
 * ASSERT_FALSE(suspended);
 */

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_CRC_HELPERS_H */
