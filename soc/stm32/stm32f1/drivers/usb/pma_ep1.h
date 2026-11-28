/**
 * \file            pma_ep1.h
 * \brief           Selected single-buffer EP1 endpoint pair and withdrawal.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_USB_PMA_EP1_H
#define NEXUS_STM32F1_USB_PMA_EP1_H
#include "pma_device.h"

nx_result_t nx_stm32f1_usb_ep1_prepare(nx_stm32f1_usb_pma_device_t* state);
nx_result_t nx_stm32f1_usb_ep1_submit(nx_stm32f1_usb_pma_device_t* state,
                                      nx_usb_request_t* request, bool in,
                                      nx_time_us_t now);
nx_result_t nx_stm32f1_usb_ep1_cancel(nx_stm32f1_usb_pma_device_t* state,
                                      nx_usb_request_t* request, bool in);
nx_result_t nx_stm32f1_usb_ep1_stall(nx_stm32f1_usb_pma_device_t* state,
                                     bool in, bool stalled);
void nx_stm32f1_usb_ep1_expire(nx_stm32f1_usb_pma_device_t* state,
                               nx_time_us_t now);
nx_result_t nx_stm32f1_usb_ep1_packet(nx_stm32f1_usb_pma_device_t* state);
nx_result_t nx_stm32f1_usb_ep1_drain_reset(nx_stm32f1_usb_pma_device_t* state);
void nx_stm32f1_usb_ep1_revoke(nx_stm32f1_usb_pma_device_t* state);

#endif
