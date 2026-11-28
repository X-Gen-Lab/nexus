/**
 * \file            pma_board.c
 * \brief           Bind the selected F102/F103 USB controller and four exact
 * pads. \author          Nexus Team \version         1.0.0 \date 2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "pma_board.h"
#include "nexus/arch/arch.h"
#include "nexus_config.h"
#include "private/hardware.h"
#include "system/platform.h"

enum {
    VBUS_PIN = 1U,
    PULLUP_PIN = 2U,
    USB_PINS = (1U << 11U) | (1U << 12U),
    USB_WAKE_EXTI = 1U << 18U,
    MAX_WARMUP_POLLS = 256U
};

#if defined(STM32F102xB)
_Static_assert(NEXUS_CORE_HZ == 48000000U && NEXUS_CLOCK_APB1_HZ == 24000000U,
               "F102 PMA needs its 48 MHz Core and 24 MHz APB1 profile");
#else
_Static_assert(NEXUS_CORE_HZ == 72000000U && NEXUS_CLOCK_APB1_HZ == 36000000U,
               "F103 PMA needs its 72 MHz Core and 36 MHz APB1 profile");
#endif
_Static_assert(NEXUS_CLOCK_PLL48_HZ == 48000000U,
               "USB Device requires the exact nominal 48 MHz lease");

static nx_result_t pullup_write(void* context, uint32_t set, uint32_t reset) {
    GPIO_TypeDef* registers = context;
    registers->BSRR = (set & PULLUP_PIN) | ((reset & PULLUP_PIN) << 16U);
    nx_arch_dsb();
    return NX_SUCCESS;
}

static nx_result_t levels_read(const void* context, uint32_t* output) {
    const GPIO_TypeDef* registers = context;
    *output = registers->IDR & (VBUS_PIN | PULLUP_PIN);
    return NX_SUCCESS;
}

static const nx_gpio_ops_t signal_ops = {
    .write = pullup_write,
    .read = levels_read,
};
static const nx_gpio_port_t signals = {&signal_ops, GPIOA};
const nx_gpio_signal_t nx_stm32f1_usb_pma_vbus = {&signals, VBUS_PIN, true};
const nx_gpio_signal_t nx_stm32f1_usb_pma_pullup = {&signals, PULLUP_PIN, true};

nx_result_t nx_stm32f1_usb_pma_board_prepare(
    nx_stm32f1_usb_pma_device_t* state,
    const nx_stm32f1_usb_pma_device_config_t* config, nx_stm32f1_pads_t* pads,
    uint32_t startup_us) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!nx_stm32f1_system_live() ||
        ((EXTI->IMR | EXTI->EMR) & USB_WAKE_EXTI) != 0U ||
        NVIC_GetEnableIRQ(USBWakeUp_IRQn) != 0U ||
        NVIC_GetPendingIRQ(USBWakeUp_IRQn) != 0U ||
        NVIC_GetActive(USBWakeUp_IRQn) != 0U) {
        return NX_ERROR_STATE;
    }
    nx_time_us_t deadline;
    nx_result_t result =
        nx_deadline_finite_after(nx_time_now_us(), startup_us, &deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    result =
        nx_stm32f1_pads_capture(pads, GPIOA, VBUS_PIN | PULLUP_PIN | USB_PINS);
    if (result != NX_SUCCESS) {
        return result;
    }
    /* Preload PA1 low; PA0 floats into the external voltage translator. */
    result = nx_stm32f1_pads_apply(pads, 0x24U, 0x44000U, 0U);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_stm32f1_usb_pma_device_begin(state, config);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (unsigned poll = 0U; poll < MAX_WARMUP_POLLS; ++poll) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            break;
        }
        result = nx_stm32f1_usb_pma_device_connect(state);
        if (result != NX_ERROR_BUSY) {
            return result;
        }
    }
    return NX_ERROR_TIMEOUT;
}

nx_result_t nx_stm32f1_usb_pma_board_close(nx_stm32f1_usb_pma_device_t* state,
                                           nx_stm32f1_pads_t* pads) {
    const nx_usb_device_port_t device = {&nx_stm32f1_usb_pma_device_ops, state};
    const nx_result_t stopped = nx_usb_device_stop(&device);
    return stopped == NX_SUCCESS ? nx_stm32f1_pads_restore(pads) : stopped;
}
