/**
 * \file            usb_device.c
 * \brief           STM32F407 locked SDK FS register/IRQ binding
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/usb_profile.h"
#include "stm32f407_usb_device.h"
#include "stm32f407xx.h"
#include <stddef.h>
_Static_assert(USB_OTG_FS_MAX_IN_ENDPOINTS == NX_USB_FS_ENDPOINT_NUMBERS &&
                   USB_OTG_FS_MAX_OUT_ENDPOINTS == NX_USB_FS_ENDPOINT_NUMBERS &&
                   USB_OTG_FS_TOTAL_FIFO_SIZE / 4U == NX_USB_FS_FIFO_WORDS,
               "Selected public FS profile must match the locked SDK");
_Static_assert(offsetof(USB_OTG_GlobalTypeDef, GAHBCFG) == 8U &&
                   offsetof(USB_OTG_GlobalTypeDef, GUSBCFG) == 12U &&
                   offsetof(USB_OTG_GlobalTypeDef, GRSTCTL) == 16U &&
                   offsetof(USB_OTG_GlobalTypeDef, GINTSTS) == 20U &&
                   offsetof(USB_OTG_GlobalTypeDef, GINTMSK) == 24U &&
                   offsetof(USB_OTG_GlobalTypeDef, GRXSTSP) == 32U &&
                   offsetof(USB_OTG_GlobalTypeDef, GRXFSIZ) == 36U &&
                   offsetof(USB_OTG_GlobalTypeDef, DIEPTXF0_HNPTXFSIZ) == 40U &&
                   offsetof(USB_OTG_GlobalTypeDef, GCCFG) == 56U &&
                   offsetof(USB_OTG_GlobalTypeDef, DIEPTXF) == 0x104U,
               "FS global register layout");
_Static_assert(USB_OTG_DEVICE_BASE == 0x800U &&
                   USB_OTG_IN_ENDPOINT_BASE == 0x900U &&
                   USB_OTG_OUT_ENDPOINT_BASE == 0xB00U &&
                   USB_OTG_FIFO_BASE == 0x1000U &&
                   offsetof(USB_OTG_DeviceTypeDef, DCTL) == 4U &&
                   offsetof(USB_OTG_DeviceTypeDef, DIEPMSK) == 16U &&
                   offsetof(USB_OTG_DeviceTypeDef, DAINTMSK) == 28U &&
                   offsetof(USB_OTG_INEndpointTypeDef, DIEPINT) == 8U &&
                   offsetof(USB_OTG_INEndpointTypeDef, DIEPTSIZ) == 16U &&
                   offsetof(USB_OTG_INEndpointTypeDef, DTXFSTS) == 24U &&
                   sizeof(USB_OTG_INEndpointTypeDef) == 32U &&
                   offsetof(USB_OTG_OUTEndpointTypeDef, DOEPINT) == 8U &&
                   offsetof(USB_OTG_OUTEndpointTypeDef, DOEPTSIZ) == 16U &&
                   sizeof(USB_OTG_OUTEndpointTypeDef) == 32U,
               "FS endpoint layout and stride");
_Static_assert(
    USB_OTG_FS_PERIPH_BASE == 0x50000000U &&
        USB_OTG_FS_MAX_IN_ENDPOINTS == 4U &&
        USB_OTG_FS_MAX_OUT_ENDPOINTS == 4U &&
        USB_OTG_FS_TOTAL_FIFO_SIZE == 1280U && OTG_FS_IRQn == 67 &&
        USB_OTG_GRSTCTL_AHBIDL == (1U << 31U) && USB_OTG_GRSTCTL_CSRST == 1U &&
        USB_OTG_GRSTCTL_RXFFLSH == 16U && USB_OTG_GRSTCTL_TXFFLSH == 32U &&
        USB_OTG_DIEPCTL_EPENA == (1U << 31U) &&
        USB_OTG_DIEPCTL_EPDIS == (1U << 30U) && USB_OTG_DIEPINT_XFRC == 1U &&
        USB_OTG_DIEPINT_EPDISD == 2U && USB_OTG_DIEPINT_TXFE == 128U &&
        USB_OTG_DIEPINT_ITTXFE == 16U && USB_OTG_DIEPINT_INEPNE == 64U &&
        USB_OTG_DIEPCTL_NAKSTS == (1U << 17U) && USB_OTG_DOEPINT_STUP == 8U &&
        USB_OTG_GCCFG_NOVBUSSENS == (1U << 21U),
    "FS capabilities, lifecycle bits and exact vector");
_Static_assert(USB_OTG_DIEPCTL_EPTYP_Pos == 18U &&
                   USB_OTG_DIEPCTL_EPTYP == (3U << 18U) &&
                   USB_OTG_DOEPCTL_EPTYP_Pos == 18U &&
                   USB_OTG_DOEPCTL_EPTYP == (3U << 18U) &&
                   USB_OTG_DIEPCTL_MPSIZ == 0x7FFU &&
                   USB_OTG_DIEPCTL_TXFNUM == (15U << 22U) &&
                   USB_OTG_DIEPCTL_SD0PID_SEVNFRM == (1U << 28U) &&
                   USB_OTG_DOEPCTL_SD0PID_SEVNFRM == (1U << 28U) &&
                   USB_OTG_DIEPCTL_EONUM_DPID == (1U << 16U) &&
                   USB_OTG_DIEPCTL_USBAEP == (1U << 15U) &&
                   USB_OTG_DOEPCTL_USBAEP == (1U << 15U) &&
                   USB_OTG_DIEPTSIZ_PKTCNT == (0x3FFU << 19U),
               "FS endpoint type, FIFO, DATA0 and packet register facts");
_Static_assert(USB_OTG_DAINTMSK_IEPM_Pos == 0U &&
                   USB_OTG_DAINTMSK_OEPM_Pos == 16U &&
                   (USB_OTG_DAINTMSK_IEPM & 0xFU) == 0xFU &&
                   (USB_OTG_DAINTMSK_OEPM & 0xF0000U) == 0xF0000U &&
                   USB_OTG_DIEPCTL_SNAK == (1U << 27U) &&
                   USB_OTG_DOEPCTL_SNAK == (1U << 27U) &&
                   USB_OTG_DIEPCTL_CNAK == (1U << 26U) &&
                   USB_OTG_DOEPCTL_CNAK == (1U << 26U) &&
                   USB_OTG_DIEPCTL_STALL == (1U << 21U) &&
                   USB_OTG_DOEPCTL_STALL == (1U << 21U) &&
                   USB_OTG_DOEPCTL_MPSIZ == 0x7FFU &&
                   USB_OTG_DOEPCTL_NAKSTS == (1U << 17U) &&
                   USB_OTG_DOEPCTL_EPENA == (1U << 31U) &&
                   USB_OTG_DOEPCTL_EPDIS == (1U << 30U),
               "FS activation, selected interrupt masks and OUT control facts");
#ifndef NX_USB_SOC_IRQ_GROUP
#define NX_USB_SOC_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif
/** \brief Bind actual FS facts to the selected cold, exclusive clock owner. */
nx_result_t nx_stm32_usb_device_initialize(nx_usb_controller_t* c,
                                           nx_stm32_usb_clock_state_t* clock) {
    if (c == NULL || clock == NULL ||
        c->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        c->irq_policy.external_irq_count != (uint16_t)FPU_IRQn + 1U ||
        (c->registers != 0U && c->registers != USB_OTG_FS_PERIPH_BASE)) {
        return NX_ERROR_INVALID;
    }
    c->registers = USB_OTG_FS_PERIPH_BASE;
    c->irq.irq_number = OTG_FS_IRQn;
    c->irq.priority_group = (uint8_t)NX_USB_SOC_IRQ_GROUP();
    c->clock = (nx_usb_clock_domain_t){.ops = &nx_stm32_usb_clock_ops,
                                       .context = clock,
                                       .context_size = sizeof(*clock)};
    return nx_usb_controller_initialize(c);
}
