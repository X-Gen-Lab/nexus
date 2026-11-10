/**
 * \file            stm32f407_usb_pins.h
 * \brief           Shared exact PHY pad facts for USB cold owners
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_USB_PINS_H
#define NEXUS_STM32F407_USB_PINS_H
#include "stm32f407_packet_cold.h"
#include "stm32f407_usb_phy.h"

/** \brief Check exact package pads independently of external PCB wiring. */
static inline bool
nx_stm32_usb_pins_valid(nx_stm32_usb_phy_t phy,
                        const nx_stm32_platform_pin_config_t* pins,
                        uint8_t count) {
    if (pins == NULL ||
        (phy != NX_STM32_USB_FS_INTERNAL &&
         phy != NX_STM32_USB_HS_INTERNAL_FS && phy != NX_STM32_USB_HS_ULPI) ||
        count != (phy == NX_STM32_USB_HS_ULPI ? 12U : 2U)) {
        return false;
    }
    static const uint8_t ulpi_banks[12] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2};
    static const uint8_t ulpi_pads[12] = {3,  5,  0,  1, 5, 10,
                                          11, 12, 13, 0, 2, 3};
    for (uint8_t i = 0U; i < count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &pins[i];
        const bool ulpi = phy == NX_STM32_USB_HS_ULPI;
        const bool fs = phy == NX_STM32_USB_FS_INTERNAL;
        uint8_t bank = ulpi ? ulpi_banks[i] : (uint8_t)(fs ? 0U : 1U);
        uint8_t pad = ulpi ? ulpi_pads[i] : (uint8_t)((fs ? 11U : 14U) + i);
        if (pin->snapshot == NULL || pin->port_index != bank ||
            pin->pin != pad ||
            (uintptr_t)pin->registers !=
                (uintptr_t)GPIOA + (uintptr_t)bank * 0x400U) {
            return false;
        }
        for (uint8_t j = 0U; j < i; ++j) {
            if (pin->snapshot == pins[j].snapshot) {
                return false;
            }
        }
    }
    return true;
}
#endif
