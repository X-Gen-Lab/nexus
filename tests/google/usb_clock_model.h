/**
 * \file            usb_clock_model.h
 * \brief           Host-only clock store refusal and PLL ready observation
 * \author          Nexus Team
 */
#ifndef NEXUS_USB_CLOCK_MODEL_H
#define NEXUS_USB_CLOCK_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Apply or refuse the actual clock register store boundary. */
void nx_usb_clock_model_write(volatile uint32_t* address, uint32_t value);
/** \brief Observe modeled PLL stabilization without changing production code.
 */
void nx_usb_clock_model_poll(void);
#ifdef __cplusplus
}
#endif
#define NX_USB_CLOCK_WRITE(reg, value)                                         \
    nx_usb_clock_model_write(&(reg), (uint32_t)(value))
#define NX_USB_CLOCK_POLL() nx_usb_clock_model_poll()
#endif
