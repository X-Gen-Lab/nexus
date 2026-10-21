/**
 * \file            usb_device.c
 * \brief           GD32F470 SDK3.3.3 FS device register/IRQ binding
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drv_usb_regs.h"
#include "gd32f470_usb_device.h"
#include "nexus/io/usb_profile.h"
#include <stddef.h>
_Static_assert(USBFS_MAX_EP_COUNT == NX_USB_FS_ENDPOINT_NUMBERS &&
                   USBFS_MAX_FIFO_WORDLEN == NX_USB_FS_FIFO_WORDS &&
                   USBFS_MAX_PACKET_SIZE == NX_USB_FS_MAX_PACKET,
               "Selected public FS profile must match the locked SDK");
_Static_assert(
    offsetof(usb_gr, GAHBCS) == 8U && offsetof(usb_gr, GUSBCS) == 12U &&
        offsetof(usb_gr, GRSTCTL) == 16U && offsetof(usb_gr, GINTF) == 20U &&
        offsetof(usb_gr, GINTEN) == 24U && offsetof(usb_gr, GRSTATP) == 32U &&
        offsetof(usb_gr, GRFLEN) == 36U &&
        offsetof(usb_gr, DIEP0TFLEN_HNPTFLEN) == 40U &&
        offsetof(usb_gr, GCCFG) == 56U && offsetof(usb_gr, DIEPTFLEN) == 0x104U,
    "Actual FS global register layout");
_Static_assert(USB_REG_OFFSET_DEV == 0x800U && USB_REG_OFFSET_EP_IN == 0x900U &&
                   USB_REG_OFFSET_EP_OUT == 0xB00U &&
                   USB_REG_OFFSET_EP == 32U &&
                   USB_DATA_FIFO_OFFSET == 0x1000U &&
                   USB_DATA_FIFO_SIZE == 0x1000U &&
                   offsetof(usb_dr, DCTL) == 4U &&
                   offsetof(usb_dr, DIEPINTEN) == 16U &&
                   offsetof(usb_dr, DAEPINTEN) == 28U &&
                   offsetof(usb_erin, DIEPINTF) == 8U &&
                   offsetof(usb_erin, DIEPLEN) == 16U &&
                   offsetof(usb_erin, DIEPTFSTAT) == 24U &&
                   offsetof(usb_erout, DOEPINTF) == 8U &&
                   offsetof(usb_erout, DOEPLEN) == 16U,
               "FS selected endpoint layout and actual hardware stride");
_Static_assert(USBFS_REG_BASE == 0x50000000U && USBFS_MAX_EP_COUNT == 4U &&
                   USBFS_MAX_FIFO_WORDLEN == 320U &&
                   USBFS_MAX_PACKET_SIZE == 64U && USBFS_IRQn == 67 &&
                   GRSTCTL_DMAIDL == (1U << 31U) && GRSTCTL_CSRST == 1U &&
                   GRSTCTL_RXFF == 16U && GRSTCTL_TXFF == 32U &&
                   DEPCTL_EPEN == (1U << 31U) && DEPCTL_EPD == (1U << 30U) &&
                   DIEPINTF_TF == 1U && DIEPINTF_EPDIS == 2U &&
                   DOEPINTF_STPF == 8U && DIEPINTF_TXFE == 128U &&
                   DIEPINTF_EPTXFUD == 16U && DIEPINTF_IEPNE == 64U &&
                   DEPCTL_NAKS == (1U << 17U) && GCCFG_VBUSIG == (1U << 21U),
               "FS lifecycle masks and capabilities");
_Static_assert(DEPCTL_EPTYPE == (3U << 18U) && DEPCTL_MPL == 0x7FFU &&
                   DIEPCTL_TXFNUM == (15U << 22U) &&
                   DEPCTL_SD0PID == (1U << 28U) && DEPCTL_DPID == (1U << 16U) &&
                   DEPCTL_EPACT == (1U << 15U) &&
                   DEPLEN_PCNT == (0x3FFU << 19U),
               "FS endpoint type, FIFO, DATA0 and packet register facts");
#ifndef NX_USB_SOC_IRQ_GROUP
#define NX_USB_SOC_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif
/** \brief Bind actual FS facts to the selected cold, exclusive clock owner. */
nx_result_t nx_gd32_usb_device_initialize(nx_usb_controller_t* c,
                                          nx_gd32_usb_clock_state_t* clock) {
    if (c == NULL || clock == NULL ||
        c->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        c->irq_policy.external_irq_count != (uint16_t)IPA_IRQn + 1U ||
        (c->registers != 0U && c->registers != USBFS_REG_BASE)) {
        return NX_ERROR_INVALID;
    }
    c->registers = USBFS_REG_BASE;
    c->irq.irq_number = USBFS_IRQn;
    c->irq.priority_group = (uint8_t)NX_USB_SOC_IRQ_GROUP();
    c->clock = (nx_usb_clock_domain_t){.ops = &nx_gd32_usb_clock_ops,
                                       .context = clock,
                                       .context_size = sizeof(*clock)};
    return nx_usb_controller_initialize(c);
}
