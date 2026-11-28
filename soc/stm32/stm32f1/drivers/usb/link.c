/**
 * \file            link.c
 *
 * \brief           Reviewed STM32F102/F103 clocks and external pull-up
 * sequencing.
 *
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "link.h"

enum {
    LP_IRQ = NX_STM32F1_USB_LP_IRQ,
    LP_IRQ_MASK = 1U << LP_IRQ,
    /* DS5319 Table 44 and DS5933 Table 42 give 1 us PHY startup. A second tick
     * accounts for integer-microsecond sampling just before a counter
     * increment. */
    STARTUP_US = 2U
};

_Static_assert(LP_IRQ < 32U, "The USB LP vector occupies NVIC word zero");

/** \brief Recognize the live System-owned USB clock and PMA exclusion. */
static bool clock_valid(const RCC_TypeDef* rcc) {
    const uint32_t live =
        RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
    const uint32_t selection = RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |
                               RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |
                               RCC_CFGR_PLLMULL | RCC_CFGR_USBPRE;
    const uint32_t source =
        RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PLLSRC;
#if defined(STM32F102xB)
    const uint32_t required = source | RCC_CFGR_PLLMULL6 | RCC_CFGR_USBPRE;
    const uint32_t clock_inputs = live;
#else
    const uint32_t required = source | RCC_CFGR_PLLMULL9;
    const uint32_t clock_inputs = live | RCC_CR_HSEBYP;
#endif
#if defined(RCC_APB1ENR_CAN1EN)
    if ((rcc->APB1ENR & RCC_APB1ENR_CAN1EN) != 0U) {
        return false;
    }
#endif
    return (rcc->CR & (live | RCC_CR_HSEBYP)) == clock_inputs &&
           (rcc->CFGR & selection) == required;
}

/** \brief Verify the Board's digital pull-up control signal. */
static nx_result_t drive(const nx_gpio_signal_t* pullup, bool connected) {
    nx_result_t result = nx_gpio_signal_write(pullup, connected);
    if (result != NX_SUCCESS) {
        return result;
    }
    bool observed;
    result = nx_gpio_signal_read(pullup, &observed);
    if (result != NX_SUCCESS) {
        return result;
    }
    return observed == connected ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief VBUS must be live immediately before letting the host see D+. */
static nx_result_t present(const nx_gpio_signal_t* vbus) {
    bool active;
    const nx_result_t result = nx_gpio_signal_read(vbus, &active);
    if (result != NX_SUCCESS) {
        return result;
    }
    return active ? NX_SUCCESS : NX_ERROR_STATE;
}

nx_result_t
nx_stm32f1_usb_link_begin(nx_stm32f1_usb_link_t* link,
                          const nx_stm32f1_usb_link_config_t* config) {
    if (link == NULL || config == NULL || config->rcc == NULL ||
        config->nvic == NULL || config->endpoint.registers == NULL ||
        config->endpoint.pma == NULL || config->pullup == NULL ||
        config->vbus == NULL || config->pullup == config->vbus) {
        return NX_ERROR_INVALID;
    }
    if (link->config != NULL || !clock_valid(config->rcc) ||
        (config->rcc->APB1ENR & RCC_APB1ENR_USBEN) != 0U ||
        (config->rcc->APB1RSTR & RCC_APB1RSTR_USBRST) != 0U ||
        ((config->nvic->ISER[0] | config->nvic->ISPR[0] |
          config->nvic->IABR[0]) &
         LP_IRQ_MASK) != 0U) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = drive(config->pullup, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    config->rcc->APB1ENR |= RCC_APB1ENR_USBEN;
    if ((config->rcc->APB1ENR & RCC_APB1ENR_USBEN) == 0U) {
        return NX_ERROR_IO;
    }
    link->config = config;
    link->ready_at = NX_DEADLINE_NEVER;
    USB_TypeDef* registers = config->endpoint.registers;
    registers->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
    registers->CNTR = USB_CNTR_FRES;
    if (registers->CNTR != USB_CNTR_FRES) {
        return NX_ERROR_IO;
    }
    return nx_deadline_finite_after(nx_time_now_us(), STARTUP_US,
                                    &link->ready_at);
}

nx_result_t nx_stm32f1_usb_link_connect(nx_stm32f1_usb_link_t* link) {
    if (link == NULL || link->config == NULL || link->endpoint.config != NULL ||
        link->ready_at == NX_DEADLINE_NEVER) {
        return NX_ERROR_STATE;
    }
    const nx_stm32f1_usb_link_config_t* config = link->config;
    if (!clock_valid(config->rcc) ||
        (config->rcc->APB1ENR & RCC_APB1ENR_USBEN) == 0U) {
        return NX_ERROR_IO;
    }
    if (!nx_deadline_expired(link->ready_at, nx_time_now_us()) ||
        (config->nvic->ISER[0] & LP_IRQ_MASK) == 0U) {
        return NX_ERROR_BUSY;
    }
    USB_TypeDef* registers = config->endpoint.registers;
    if (registers->CNTR != USB_CNTR_FRES) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = present(config->vbus);
    if (result != NX_SUCCESS) {
        return result;
    }
    registers->CNTR = 0U;
    nx_stm32f1_usb_istr_write(&registers->ISTR, 0U);
    registers->BTABLE = 0U;
    result = nx_stm32f1_usb_ep0_start(&link->endpoint, &config->endpoint);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_stm32f1_usb_lp_enable(&link->endpoint);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = present(config->vbus);
    return result == NX_SUCCESS ? drive(config->pullup, true) : result;
}

nx_result_t nx_stm32f1_usb_link_disconnect(nx_stm32f1_usb_link_t* link) {
    if (link == NULL || link->config == NULL) {
        return NX_ERROR_STATE;
    }
    const nx_stm32f1_usb_link_config_t* config = link->config;
    nx_result_t result = drive(config->pullup, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    if ((config->rcc->APB1ENR & RCC_APB1ENR_USBEN) == 0U) {
        return NX_ERROR_IO;
    }
    USB_TypeDef* registers = config->endpoint.registers;
    const uint16_t reset =
        (uint16_t)((registers->CNTR & USB_CNTR_PDWN) | USB_CNTR_FRES);
    registers->CNTR = reset;
    registers->DADDR = 0U;
    return registers->CNTR == reset ? NX_SUCCESS : NX_ERROR_BUSY;
}

nx_result_t nx_stm32f1_usb_link_release(nx_stm32f1_usb_link_t* link) {
    if (link == NULL || link->config == NULL) {
        return NX_ERROR_STATE;
    }
    const nx_stm32f1_usb_link_config_t* config = link->config;
    USB_TypeDef* registers = config->endpoint.registers;
    if ((registers->CNTR & ~USB_CNTR_PDWN) != USB_CNTR_FRES ||
        ((config->nvic->ISER[0] | config->nvic->ISPR[0] |
          config->nvic->IABR[0]) &
         LP_IRQ_MASK) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (link->endpoint.config != NULL) {
        const nx_result_t result = nx_stm32f1_usb_lp_close(&link->endpoint);
        if (result != NX_SUCCESS) {
            return result;
        }
    } else if ((registers->ISTR & USB_ISTR_CTR) != 0U) {
        return NX_ERROR_BUSY;
    }
    registers->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
    if (registers->CNTR != (USB_CNTR_FRES | USB_CNTR_PDWN)) {
        return NX_ERROR_IO;
    }
    config->rcc->APB1ENR &= ~(uint32_t)RCC_APB1ENR_USBEN;
    if ((config->rcc->APB1ENR & RCC_APB1ENR_USBEN) != 0U) {
        return NX_ERROR_IO;
    }
    link->config = NULL;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_usb_link_stop(nx_stm32f1_usb_link_t* link) {
    const nx_result_t result = nx_stm32f1_usb_link_disconnect(link);
    return result == NX_SUCCESS ? nx_stm32f1_usb_link_release(link) : result;
}
