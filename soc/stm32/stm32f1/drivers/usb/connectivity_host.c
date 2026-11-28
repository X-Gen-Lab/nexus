/**
 * \file            connectivity_host.c
 * \brief           Exact F105/F107 fixed-A PHY and guarded external VBUS.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/usb/connectivity_host.h"
#include "drivers/usb/connectivity_clock.h"
#include "nexus/core/fault.h"
#include "nexus_config.h"
#include "private/hardware.h"

enum {
    VBUS_EN = 1U << 0U,
    VBUS_FAULT_N = 1U << 1U,
    VBUS_OK = 1U << 10U,
    PHY_PADS = (1U << 9U) | (1U << 11U) | (1U << 12U),
    POWER_PADS = VBUS_EN | VBUS_FAULT_N | VBUS_OK,
    MAX_POWER_POLLS = 200000U
};

_Static_assert(NEXUS_CORE_HZ == 72000000U && NEXUS_CLOCK_PLL48_HZ == 48000000U,
               "Connectivity Host needs exact 72 MHz HCLK and USB 48 MHz");
_Static_assert(OTG_FS_IRQn == 67 && USB_OTG_HOST_BASE == 0x400U &&
                   USB_OTG_HOST_PORT_BASE == 0x440U &&
                   USB_OTG_HOST_CHANNEL_BASE == 0x500U &&
                   sizeof(USB_OTG_HostChannelTypeDef) == 32U &&
                   RCC_AHBENR_OTGFSEN == (1U << 12U),
               "Connectivity Host register and IRQ facts");

bool nx_stm32f1_usb_host_clock_ready(void) {
    return nx_stm32f1_connectivity_usb_clock_live() &&
           (RCC->AHBRSTR & RCC_AHBRSTR_OTGFSRST) == 0U;
}

bool nx_stm32f1_usb_host_power_ready(void) {
    return (GPIOB->IDR & POWER_PADS) == POWER_PADS;
}

/** \brief Assert or remove only the carrier's externally guarded VBUS switch.
 */
static bool power_enable(bool enabled) {
    GPIOB->BSRR = enabled ? VBUS_EN : VBUS_EN << 16U;
    nx_arch_dsb();
    return ((GPIOB->IDR & VBUS_EN) != 0U) == enabled;
}

nx_result_t nx_stm32f1_usb_host_prepare(
    nx_stm32_usb_host_t* state, const nx_stm32_usb_host_config_t* config,
    nx_stm32f1_usb_host_owner_t* owner, uint32_t startup_us) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (owner == NULL || startup_us == 0U ||
        nx_stm32_usb_host_validate(state, config) != NX_SUCCESS)
        return NX_ERROR_INVALID;
    if (owner->phy.registers != NULL || owner->power.registers != NULL ||
        (RCC->AHBENR & RCC_AHBENR_OTGFSEN) != 0U ||
        NVIC_GetEnableIRQ(OTG_FS_IRQn) != 0U ||
        NVIC_GetPendingIRQ(OTG_FS_IRQn) != 0U ||
        NVIC_GetActive(OTG_FS_IRQn) != 0U)
        return NX_ERROR_BUSY;
    if (!nx_stm32f1_usb_host_clock_ready())
        return NX_ERROR_STATE;

    nx_time_us_t deadline;
    nx_result_t result =
        nx_deadline_finite_after(nx_time_now_us(), startup_us, &deadline);
    if (result != NX_SUCCESS)
        return result;

    result = nx_stm32f1_pads_capture(&owner->phy, GPIOA, PHY_PADS);
    if (result != NX_SUCCESS)
        return result;
    const uint32_t fields = (15U << 4U) | (15U << 12U) | (15U << 16U);
    const uint32_t floating = (4U << 4U) | (4U << 12U) | (4U << 16U);
    /* The embedded OTG PHY switches DM/DP, as in RM0008 Table 30. */
    owner->phy.captured = false;
    if ((owner->phy.high & fields) != floating)
        return NX_ERROR_BUSY;

    result = nx_stm32f1_pads_capture(&owner->power, GPIOB, POWER_PADS);
    if (result != NX_SUCCESS)
        return result;
    if ((owner->power.low & 0xFFU) != 0x44U ||
        (owner->power.high & 0xF00U) != 0x400U ||
        (owner->power.latch & VBUS_EN) != 0U) {
        return NX_ERROR_BUSY;
    }
    result = nx_stm32f1_pads_apply(&owner->power, 0x42U, 0x400U, 0U);
    if (result != NX_SUCCESS)
        return result;
    if ((GPIOB->IDR & (VBUS_OK | VBUS_FAULT_N)) != VBUS_FAULT_N)
        return NX_ERROR_STATE;
    owner->power_owned = true;
    if (!power_enable(true))
        return NX_ERROR_IO;
    for (unsigned poll = 0U; poll < MAX_POWER_POLLS; ++poll) {
        if ((GPIOB->IDR & VBUS_FAULT_N) == 0U)
            return NX_ERROR_IO;
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            break;
        if (nx_stm32f1_usb_host_power_ready())
            return nx_stm32_usb_host_initialize(state, config, deadline);
    }
    return NX_ERROR_TIMEOUT;
}

nx_result_t nx_stm32f1_usb_host_close(nx_stm32_usb_host_t* state,
                                      nx_stm32f1_usb_host_owner_t* owner) {
    if (owner == NULL || state == NULL)
        return NX_ERROR_INVALID;
    const nx_usb_host_port_t transport = {&nx_stm32_usb_host_ops, state};
    nx_result_t result = nx_usb_host_stop(&transport);
    if (result != NX_SUCCESS)
        return result;
    if (owner->power_owned) {
        if (!power_enable(false) || (GPIOB->IDR & VBUS_OK) != 0U)
            return NX_ERROR_BUSY;
        owner->power_owned = false;
    }
    result = nx_stm32f1_pads_restore(&owner->power);
    return result == NX_SUCCESS ? nx_stm32f1_pads_restore(&owner->phy) : result;
}

/** \brief Official mode switch delays use the established Core clock. */
void HAL_Delay(uint32_t milliseconds);

void HAL_Delay(uint32_t milliseconds) {
    nx_time_us_t prior = nx_time_now_us();
    nx_time_us_t until;
    if (nx_deadline_finite_after(prior, (uint64_t)milliseconds * 1000U,
                                 &until) == NX_SUCCESS) {
        uint32_t left = NEXUS_CORE_HZ / 1000U + 1U;
        while (nx_stm32f1_usb_host_clock_ready()) {
            const nx_time_us_t now = nx_time_now_us();
            if (now < prior || left == 0U)
                break;
            if (nx_deadline_expired(until, now))
                return;
            if (now != prior)
                left = NEXUS_CORE_HZ / 1000U + 1U;
            --left;
            prior = now;
        }
    }
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_STATE, __FILE__, __LINE__);
    for (;;)
        nx_arch_dsb();
}
