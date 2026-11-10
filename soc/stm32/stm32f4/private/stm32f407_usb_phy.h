/**
 * \file            stm32f407_usb_phy.h
 * \brief           Exact F407 OTG PHY identities independent of role
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_USB_PHY_H
#define NEXUS_STM32F407_USB_PHY_H
typedef enum {
    NX_STM32_USB_FS_INTERNAL,
    NX_STM32_USB_HS_INTERNAL_FS,
    NX_STM32_USB_HS_ULPI
} nx_stm32_usb_phy_t;
#endif
