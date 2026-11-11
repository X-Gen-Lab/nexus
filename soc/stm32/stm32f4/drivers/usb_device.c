/**
 * \file            usb_device.c
 * \brief           STM32F407 locked SDK FS register/IRQ binding
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/usb_profile.h"
#include "stm32f407_clock.h"
#include "stm32f407_usb_device.h"
#include "stm32f4xx.h"
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
_Static_assert(USB_OTG_GINTSTS_ISOODRP == (1U << 14U) &&
                   USB_OTG_GINTSTS_IISOIXFR == (1U << 20U) &&
                   USB_OTG_GINTSTS_PXFR_INCOMPISOOUT == (1U << 21U) &&
                   USB_OTG_DIEPCTL_SODDFRM == (1U << 29U) &&
                   USB_OTG_DOEPCTL_SODDFRM == (1U << 29U) &&
                   USB_OTG_DIEPTSIZ_MULCNT == (3U << 29U) &&
                   USB_OTG_DSTS_FNSOF == (0x3FFFU << 8U),
               "ISO loss, frame parity and single-transaction register facts");
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
#ifndef NX_USB_SOC_HCLK_HZ
#define NX_USB_SOC_HCLK_HZ NEXUS_CORE_HZ
#endif
#ifndef NX_USB_CLOCK_POLL
#define NX_USB_CLOCK_POLL() ((void)0)
#endif

/** \brief Allow a conservative 50 ms for the RM0090 force-mode transition.
 * \note This runs only after cold clock/pad acquisition, before any DCD IRQ
 *       or packet owner is published. CMOD acknowledgement alone is not a
 *       substitute for the mode-switch settling interval. A stopped or
 *       backward clock cannot make the wait succeed.
 */
static nx_result_t device_mode(nx_usb_controller_t* c) {
    nx_result_t result = nx_usb_controller_plan_validate(c);
    if (result != NX_SUCCESS)
        return result;
    result = c->clock.ops->validate(c->clock.context);
    if (result != NX_SUCCESS)
        return result;
    USB_OTG_GlobalTypeDef* core = (USB_OTG_GlobalTypeDef*)c->registers;
    if ((core->GINTSTS & USB_OTG_GINTSTS_CMOD) == 0U)
        return NX_SUCCESS;
    core->GUSBCFG = (core->GUSBCFG & ~(uint32_t)USB_OTG_GUSBCFG_FHMOD) |
                    (uint32_t)USB_OTG_GUSBCFG_FDMOD;
    nx_arch_dsb();
    nx_time_us_t first = nx_time_now_us();
    nx_time_us_t deadline;
    result = nx_deadline_finite_after(first, 100000U, &deadline);
    if (result != NX_SUCCESS)
        return result;
    uint32_t remaining = NX_USB_SOC_HCLK_HZ / 100U;
    do {
        NX_USB_CLOCK_POLL();
        nx_time_us_t now = nx_time_now_us();
        result = c->clock.ops->validate(c->clock.context);
        if (result != NX_SUCCESS)
            return result;
        if (now < first || nx_deadline_expired(deadline, now))
            return NX_ERROR_TIMEOUT;
        if (now - first >= 50000U &&
            (core->GINTSTS & USB_OTG_GINTSTS_CMOD) == 0U)
            return NX_SUCCESS;
    } while (remaining-- != 0U);
    return NX_ERROR_TIMEOUT;
}

/** \brief Bind actual FS facts to the selected cold, exclusive clock owner. */
nx_result_t nx_stm32_usb_device_initialize(nx_usb_controller_t* c,
                                           nx_stm32_usb_clock_state_t* clock) {
    if (c == NULL || clock == NULL ||
        c->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        c->irq_policy.external_irq_count != (uint16_t)FPU_IRQn + 1U ||
        (c->registers != 0U && c->registers != USB_OTG_FS_PERIPH_BASE)) {
        return NX_ERROR_INVALID;
    }
    uint8_t turnaround = nx_usb_turnaround(NX_USB_CORE_FS, NX_USB_SOC_HCLK_HZ);
    if (turnaround == 0U) {
        return NX_ERROR_INVALID;
    }
    c->registers = USB_OTG_FS_PERIPH_BASE;
    c->core = NX_USB_CORE_FS;
    c->turnaround = turnaround;
    c->irq.irq_number = OTG_FS_IRQn;
    c->irq.priority_group = (uint8_t)NX_USB_SOC_IRQ_GROUP();
    c->clock = (nx_usb_clock_domain_t){.ops = &nx_stm32_usb_clock_ops,
                                       .context = clock,
                                       .context_size = sizeof(*clock)};
    nx_result_t result = device_mode(c);
    return result == NX_SUCCESS ? nx_usb_controller_initialize(c) : result;
}

_Static_assert(USB_OTG_HS_PERIPH_BASE == 0x40040000U &&
                   USB_OTG_HS_MAX_IN_ENDPOINTS == 6U &&
                   USB_OTG_HS_MAX_OUT_ENDPOINTS == 6U &&
                   USB_OTG_HS_TOTAL_FIFO_SIZE == 4096U && OTG_HS_IRQn == 77,
               "Exact F407 HS core geometry differs from the locked SDK");

/** \brief Bind the HS core to its actual explicitly handed-over PHY domain. */
nx_result_t nx_stm32_usb_device_hs_initialize(
    nx_usb_controller_t* c, const nx_usb_clock_domain_t* clock, bool ulpi) {
    if (c == NULL || clock == NULL ||
        c->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        c->irq_policy.external_irq_count != (uint16_t)FPU_IRQn + 1U ||
        (c->registers != 0U && c->registers != USB_OTG_HS_PERIPH_BASE)) {
        return NX_ERROR_INVALID;
    }
    uint8_t core = ulpi ? NX_USB_CORE_HS_ULPI : NX_USB_CORE_HS_INTERNAL_FS;
    uint8_t turnaround = nx_usb_turnaround(core, NX_USB_SOC_HCLK_HZ);
    if (turnaround == 0U) {
        return NX_ERROR_INVALID;
    }
    uint32_t required =
        RCC_AHB1ENR_OTGHSEN | (ulpi ? RCC_AHB1ENR_OTGHSULPIEN : 0U);
    if ((RCC->AHB1ENR & required) != required) {
        return NX_ERROR_STATE;
    }
    c->registers = USB_OTG_HS_PERIPH_BASE;
    c->core = core;
    c->turnaround = turnaround;
    c->irq.irq_number = OTG_HS_IRQn;
    c->irq.priority_group = (uint8_t)NX_USB_SOC_IRQ_GROUP();
    c->clock = *clock;
    nx_result_t result = device_mode(c);
    return result == NX_SUCCESS ? nx_usb_controller_initialize(c) : result;
}
