/**
 * \file            pma_board.h
 * \brief           F102/F103 references' exact USB transceiver and pad
 * ownership. \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_USB_PMA_BOARD_H
#define NEXUS_STM32F1_USB_PMA_BOARD_H

#include "drivers/gpio/instance.h"
#include "pma_device.h"
#define NX_STM32F1_USB_PMA_HALFWORDS ((volatile uint16_t*)USB_PMAADDR)

extern const nx_gpio_signal_t nx_stm32f1_usb_pma_vbus;
extern const nx_gpio_signal_t nx_stm32f1_usb_pma_pullup;

nx_result_t nx_stm32f1_usb_pma_board_prepare(
    nx_stm32f1_usb_pma_device_t* state,
    const nx_stm32f1_usb_pma_device_config_t* config, nx_stm32f1_pads_t* pads,
    uint32_t startup_us);
nx_result_t nx_stm32f1_usb_pma_board_close(nx_stm32f1_usb_pma_device_t* state,
                                           nx_stm32f1_pads_t* pads);

#endif
