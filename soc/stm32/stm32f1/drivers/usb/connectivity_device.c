/**
 * \file            connectivity_device.c
 * \brief           STM32F105/F107 OTG FS B-device and VBUS sensing.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/usb/connectivity_device.h"
#include "drivers/usb/connectivity_clock.h"
#include "private/hardware.h"
#include "system/platform.h"
#include <stddef.h>

#if !defined(STM32F105xC) && !defined(STM32F107xC)
#error "Connectivity DWC Device requires the reviewed F105/F107 selector"
#endif
#ifndef NEXUS_CLOCK_PLL48_HZ
#include "nexus_config.h"
#endif
_Static_assert(
    NEXUS_CORE_HZ == 72000000U && NEXUS_CLOCK_PLL48_HZ == 48000000U,
    "Connectivity Device requires HCLK72 and the exact PLL48 domain");
_Static_assert(USB_OTG_FS_PERIPH_BASE == 0x50000000U && OTG_FS_IRQn == 67 &&
                   USB_OTG_DEVICE_BASE == 0x800U &&
                   offsetof(USB_OTG_DeviceTypeDef, DCTL) == 4U &&
                   offsetof(USB_OTG_GlobalTypeDef, GOTGCTL) == 0U &&
                   offsetof(USB_OTG_GlobalTypeDef, GINTSTS) == 0x14U &&
                   offsetof(USB_OTG_GlobalTypeDef, GCCFG) == 0x38U &&
                   USB_OTG_GOTGCTL_BSVLD == (1U << 19U) &&
                   USB_OTG_GCCFG_VBUSBSEN == (1U << 19U) &&
                   USB_OTG_DCTL_SDIS == (1U << 1U) &&
                   RCC_AHBENR_OTGFSEN == (1U << 12U) &&
                   RCC_AHBRSTR_OTGFSRST == (1U << 12U),
               "Exact Connectivity device, VBUS and AHB ownership");

#ifndef NX_STM32F1_DWC_BASE
#define NX_STM32F1_DWC_BASE USB_OTG_FS_PERIPH_BASE
#endif
#ifndef NX_STM32F1_DWC_IRQ_BUSY
#define NX_STM32F1_DWC_IRQ_BUSY()                                              \
    (NVIC_GetEnableIRQ(OTG_FS_IRQn) != 0U ||                                   \
     NVIC_GetPendingIRQ(OTG_FS_IRQn) != 0U ||                                  \
     NVIC_GetActive(OTG_FS_IRQn) != 0U)
#define NX_STM32F1_DWC_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif

#define DWC_PADS ((1U << 9U) | (1U << 11U) | (1U << 12U))
#define DWC_DCTL (USB_OTG_DEVICE_BASE + 4U)
#define DWC_PHY  0x038U

static nx_result_t validate(const void* context) {
    const nx_stm32f1_dwc_device_owner_t* owner = context;
    if (owner == NULL || !owner->gate_owned)
        return NX_ERROR_STATE;
    return nx_stm32f1_connectivity_usb_clock_live() &&
                   (RCC->AHBENR & RCC_AHBENR_OTGFSEN) != 0U &&
                   (RCC->AHBRSTR & RCC_AHBRSTR_OTGFSRST) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

static nx_result_t retain(void* context) {
    nx_stm32f1_dwc_device_owner_t* owner = context;
    const nx_result_t checked = validate(owner);
    if (checked != NX_SUCCESS)
        return checked;
    if (owner->retained)
        return NX_ERROR_BUSY;
    owner->retained = true;
    return NX_SUCCESS;
}

static nx_result_t release(void* context) {
    nx_stm32f1_dwc_device_owner_t* owner = context;
    if (owner == NULL || !owner->retained)
        return NX_ERROR_STATE;
    owner->retained = false;
    return NX_SUCCESS;
}

static const nx_usb_clock_ops_t clock_ops = {retain, release, validate, NULL};

static bool disconnect(uintptr_t base) {
    NX_USB_WRITE(base, DWC_DCTL,
                 NX_USB_READ(base, DWC_DCTL) | USB_OTG_DCTL_SDIS);
    nx_arch_dsb();
    return (NX_USB_READ(base, DWC_DCTL) & USB_OTG_DCTL_SDIS) != 0U;
}

/** \brief Reuse F1 pad receipts without disturbing unrelated GPIOA pins. */
static nx_result_t take_pads(nx_stm32f1_dwc_device_owner_t* owner) {
    nx_result_t result = nx_stm32f1_pads_capture(&owner->pads, GPIOA, DWC_PADS);
    if (result != NX_SUCCESS)
        return result;
    const uint32_t fields = (15U << 4U) | (15U << 12U) | (15U << 16U);
    const uint32_t floating = (4U << 4U) | (4U << 12U) | (4U << 16U);
    /* RM0008 Table 30: the PHY switches DM/DP, while VBUS is floating input. */
    owner->pads.captured = false;
    return (owner->pads.high & fields) == floating ? NX_SUCCESS : NX_ERROR_BUSY;
}

nx_result_t
nx_stm32f1_dwc_device_prepare(nx_usb_controller_t* controller,
                              nx_stm32f1_dwc_device_owner_t* owner) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (controller == NULL || owner == NULL ||
        controller->endpoint_count != 2U || controller->dma ||
        controller->remote != NULL ||
        controller->irq_policy.priority_bits != __NVIC_PRIO_BITS ||
        controller->irq_policy.external_irq_count != (uint16_t)OTG_FS_IRQn + 1U)
        return NX_ERROR_INVALID;
    if (controller->registers != 0U &&
        controller->registers != NX_STM32F1_DWC_BASE)
        return NX_ERROR_INVALID;
    if (owner->gate_owned || owner->phy_owned || owner->retained ||
        owner->pads.registers != NULL || controller->snapshot.started ||
        NX_STM32F1_DWC_IRQ_BUSY() || (RCC->AHBENR & RCC_AHBENR_OTGFSEN) != 0U ||
        (RCC->AHBRSTR & RCC_AHBRSTR_OTGFSRST) != 0U)
        return NX_ERROR_BUSY;
    if (!nx_stm32f1_connectivity_usb_clock_live())
        return NX_ERROR_STATE;
    controller->registers = NX_STM32F1_DWC_BASE;
    controller->core = NX_USB_CORE_FS;
    controller->turnaround = nx_usb_turnaround(NX_USB_CORE_FS, NEXUS_CORE_HZ);
    controller->vbus_sensing = true;
    controller->irq.irq_number = OTG_FS_IRQn;
    controller->irq.priority_group = (uint8_t)NX_STM32F1_DWC_IRQ_GROUP();
    controller->clock =
        (nx_usb_clock_domain_t){&clock_ops, owner, sizeof(*owner)};
    nx_result_t result = nx_usb_controller_plan_validate(controller);
    if (result != NX_SUCCESS)
        return result;
    owner->gate_owned = true;
    if (!nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_OTGFSEN,
                                 RCC_AHBENR_OTGFSEN))
        return NX_ERROR_IO;
    if ((NX_USB_READ(controller->registers, 0x014U) & USB_OTG_GINTSTS_CMOD) !=
        0U)
        return NX_ERROR_BUSY;
    if (!disconnect(controller->registers))
        return NX_ERROR_IO;
    result = take_pads(owner);
    if (result != NX_SUCCESS)
        return result;
    owner->phy_owned = true;
    return nx_usb_controller_initialize(controller);
}

nx_result_t
nx_stm32f1_dwc_device_configure(nx_usb_controller_t* controller,
                                const nx_stm32f1_dwc_device_config_t* config,
                                nx_stm32f1_dwc_device_owner_t* owner) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (controller == NULL || config == NULL || owner == NULL ||
        config->endpoint_count != 2U || config->endpoints == NULL ||
        config->plan == NULL || config->events == NULL ||
        config->clock_timeout_us == 0U)
        return NX_ERROR_INVALID;
    if (owner->gate_owned || owner->phy_owned || owner->retained ||
        owner->pads.registers != NULL || controller->snapshot.started)
        return NX_ERROR_BUSY;
    nx_time_us_t deadline;
    const nx_result_t budget = nx_deadline_finite_after(
        nx_time_now_us(), config->clock_timeout_us, &deadline);
    if (budget != NX_SUCCESS)
        return budget;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    const uint64_t epoch = controller->snapshot.epoch;
    *controller = (nx_usb_controller_t){
        .endpoints = config->endpoints,
        .endpoint_count = config->endpoint_count,
        .events = config->events,
        .capacity = config->capacity,
        .irq_policy = config->irq_policy,
        .irq = {.priority = config->priority},
        .snapshot = {.epoch = epoch},
        .poll_limit = config->poll_limit,
        .drain_timeout_us = config->drain_timeout_us,
        .rx_fifo_words = config->rx_fifo_words,
    };
    for (size_t i = 0U; i < config->endpoint_count; ++i) {
        const nx_usb_endpoint_config_t* plan = &config->plan[i];
        config->endpoints[i] = (nx_usb_endpoint_t){
            .number = plan->number,
            .in = plan->in,
            .transfer = plan->transfer,
            .max_packet = plan->max_packet,
            .fifo_start = plan->fifo_start,
            .fifo_words = plan->fifo_words,
        };
    }
    return nx_stm32f1_dwc_device_prepare(controller, owner);
}

nx_result_t nx_stm32f1_dwc_device_close(nx_usb_controller_t* controller,
                                        nx_stm32f1_dwc_device_owner_t* owner) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (controller == NULL || owner == NULL)
        return NX_ERROR_INVALID;
    if (owner->gate_owned && (controller->registers != NX_STM32F1_DWC_BASE ||
                              controller->clock.context != owner))
        return NX_ERROR_STATE;
    if (owner->retained) {
        if ((RCC->AHBENR & RCC_AHBENR_OTGFSEN) == 0U)
            return NX_ERROR_BUSY;
        const nx_result_t stopped = nx_usb_fs_device_ops.stop(controller);
        if (stopped != NX_SUCCESS)
            return stopped;
    }
    if (owner->phy_owned) {
        if ((RCC->AHBENR & RCC_AHBENR_OTGFSEN) == 0U)
            return NX_ERROR_BUSY;
        const uint32_t phy = USB_OTG_GCCFG_PWRDWN | USB_OTG_GCCFG_VBUSBSEN;
        NX_USB_WRITE(controller->registers, DWC_PHY,
                     NX_USB_READ(controller->registers, DWC_PHY) & ~phy);
        nx_arch_dsb();
        if ((NX_USB_READ(controller->registers, DWC_PHY) & phy) != 0U)
            return NX_ERROR_BUSY;
        owner->phy_owned = false;
    }
    if (owner->gate_owned) {
        if (owner->retained ||
            !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_OTGFSEN, 0U))
            return NX_ERROR_BUSY;
        owner->gate_owned = false;
    }
    return nx_stm32f1_pads_restore(&owner->pads);
}
