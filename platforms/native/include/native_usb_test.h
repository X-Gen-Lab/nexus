/**
 * \file            native_usb_test.h
 * \brief           Native USB Test Helpers
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef NATIVE_USB_TEST_H
#define NATIVE_USB_TEST_H

#include "hal/interface/nx_usb.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Factory Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get USB instance
 * \param[in]       index: USB instance index
 * \return          USB interface pointer or NULL
 */
nx_usb_t* nx_usb_native_get(uint8_t index);

/*---------------------------------------------------------------------------*/
/* Reset Functions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Reset all USB instances
 */
void nx_usb_native_reset_all(void);

/*---------------------------------------------------------------------------*/
/* State Query Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get USB state
 * \param[in]       index: USB instance index
 * \param[out]      initialized: Initialization flag
 * \param[out]      suspended: Suspend flag
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_usb_native_get_state(uint8_t index, bool* initialized,
                                    bool* suspended);

/*---------------------------------------------------------------------------*/
/* USB-Specific Test Helpers                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Inject data into RX buffer (for testing)
 * \param[in]       index: USB instance index
 * \param[in]       data: Data to inject
 * \param[in]       len: Length of data
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_usb_native_inject_rx(uint8_t index, const uint8_t* data,
                                    size_t len);

/**
 * \brief           Simulate USB connect event
 * \param[in]       index: USB instance index
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_usb_native_simulate_connect(uint8_t index);

/**
 * \brief           Simulate USB disconnect event
 * \param[in]       index: USB instance index
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_usb_native_simulate_disconnect(uint8_t index);

/**
 * \brief           Simulate USB suspend event
 * \param[in]       index: USB instance index
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_usb_native_simulate_suspend(uint8_t index);

/**
 * \brief           Simulate USB resume event
 * \param[in]       index: USB instance index
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_usb_native_simulate_resume(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_USB_TEST_H */
