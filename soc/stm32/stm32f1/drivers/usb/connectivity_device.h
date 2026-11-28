/**
 * \file            connectivity_device.h
 * \brief           STM32F105/F107 EP0 Device cold ownership.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F1_CONNECTIVITY_DWC_DEVICE_H
#define NEXUS_STM32F1_CONNECTIVITY_DWC_DEVICE_H
#include "drivers/gpio/instance.h"
#include "usb_device_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One RCC gate, three reviewed pads and one portable lease. */
typedef struct {
    nx_stm32f1_pads_t pads;
    bool gate_owned;
    bool phy_owned;
    bool retained;
} nx_stm32f1_dwc_device_owner_t;

typedef struct {
    nx_usb_endpoint_t* endpoints;
    const nx_usb_endpoint_config_t* plan;
    nx_usb_event_t* events;
    size_t endpoint_count;
    size_t capacity;
    nx_irq_policy_t irq_policy;
    uint32_t poll_limit;
    uint32_t drain_timeout_us;
    uint32_t clock_timeout_us;
    uint16_t rx_fifo_words;
    uint8_t priority;
} nx_stm32f1_dwc_device_config_t;

nx_result_t
nx_stm32f1_dwc_device_configure(nx_usb_controller_t* controller,
                                const nx_stm32f1_dwc_device_config_t* config,
                                nx_stm32f1_dwc_device_owner_t* owner);
nx_result_t nx_stm32f1_dwc_device_prepare(nx_usb_controller_t* controller,
                                          nx_stm32f1_dwc_device_owner_t* owner);
nx_result_t nx_stm32f1_dwc_device_close(nx_usb_controller_t* controller,
                                        nx_stm32f1_dwc_device_owner_t* owner);

#ifdef __cplusplus
}
#endif
#endif
