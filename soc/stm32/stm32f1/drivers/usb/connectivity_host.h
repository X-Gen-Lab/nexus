/**
 * \file            connectivity_host.h
 * \brief           Fixed-A software-reference power and STM32F1 Host lease.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_CONNECTIVITY_HOST_H
#define NEXUS_STM32F1_CONNECTIVITY_HOST_H
#include "drivers/gpio/instance.h"
#include "drivers/usb/connectivity_host_transport.h"

typedef struct {
    nx_stm32f1_pads_t phy;
    nx_stm32f1_pads_t power;
    bool power_owned;
} nx_stm32f1_usb_host_owner_t;

nx_result_t nx_stm32f1_usb_host_prepare(
    nx_stm32_usb_host_t* state, const nx_stm32_usb_host_config_t* config,
    nx_stm32f1_usb_host_owner_t* owner, uint32_t startup_us);
nx_result_t nx_stm32f1_usb_host_close(nx_stm32_usb_host_t* state,
                                      nx_stm32f1_usb_host_owner_t* owner);

#endif
