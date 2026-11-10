/**
 * \file            st_usb_host_delay_model.h
 * \brief           External CPU park boundary for the real Core delay
 * \author          Nexus Team
 */
#ifndef NEXUS_ST_USB_HOST_DELAY_MODEL_H
#define NEXUS_ST_USB_HOST_DELAY_MODEL_H
#ifdef __cplusplus
extern "C" {
#endif
void nx_st_usb_delay_model_park(void);
#ifdef __cplusplus
}
#endif
#define NX_ST_USB_DELAY_PARK() nx_st_usb_delay_model_park()
#endif
