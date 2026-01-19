/**
 * \file            native_usb_helpers.h
 * \brief           Native USB test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_USB_HELPERS_H
#define NATIVE_USB_HELPERS_H

#include "hal/interface/nx_usb.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

nx_status_t native_usb_get_state(uint8_t index, bool* initialized,
                                 bool* suspended);
nx_status_t native_usb_inject_rx(uint8_t index, const uint8_t* data,
                                 size_t len);
nx_status_t native_usb_simulate_connect(uint8_t index);
nx_status_t native_usb_simulate_disconnect(uint8_t index);
nx_status_t native_usb_simulate_suspend(uint8_t index);
nx_status_t native_usb_simulate_resume(uint8_t index);
void native_usb_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_USB_HELPERS_H */
