/**
 * \file            lp.c
 *
 * \brief           STM32F103 RESET/CTR handling with bounded source drain.
 *
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "lp.h"

enum {
    LINK_TRANSITIONS = USB_ISTR_RESET | USB_ISTR_SUSP | USB_ISTR_WKUP,
    LP_SOURCES = LINK_TRANSITIONS | USB_ISTR_CTR,
    LINE_BITS = USB_FNR_RXDP | USB_FNR_RXDM,
    LP_MASK = USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM,
    INTERRUPT_MASK = USB_CNTR_ESOFM | USB_CNTR_SOFM | USB_CNTR_RESETM |
                     USB_CNTR_SUSPM | USB_CNTR_WKUPM | USB_CNTR_ERRM |
                     USB_CNTR_PMAOVRM | USB_CNTR_CTRM,
    ISTR_W0C = USB_ISTR_ESOF | USB_ISTR_SOF | USB_ISTR_RESET | USB_ISTR_SUSP |
               USB_ISTR_WKUP | USB_ISTR_ERR | USB_ISTR_PMAOVR
};

/** \brief Clear observed W0C flags without acknowledging their neighbors. */
static void clear_flags(USB_TypeDef* registers, uint16_t flags) {
    nx_stm32f1_usb_istr_write(&registers->ISTR, (uint16_t)(ISTR_W0C & ~flags));
}

void nx_stm32f1_usb_lp_mask(USB_TypeDef* registers) {
    registers->CNTR = (uint16_t)(registers->CNTR & ~LP_MASK);
    (void)registers->CNTR;
}

bool nx_stm32f1_usb_lp_pending(const USB_TypeDef* registers) {
    return (registers->ISTR & LP_SOURCES) != 0U;
}

bool nx_stm32f1_usb_lp_ready(const nx_stm32f1_ep0_t* endpoint) {
    if (endpoint->config == NULL) {
        return false;
    }
    const USB_TypeDef* registers = endpoint->config->registers;
    return !endpoint->reset_candidate &&
           (registers->CNTR & USB_CNTR_FSUSP) == 0U &&
           (registers->ISTR & LINK_TRANSITIONS) == 0U;
}

nx_result_t nx_stm32f1_usb_lp_rearm(const nx_stm32f1_ep0_t* endpoint) {
    if (endpoint == NULL || endpoint->config == NULL) {
        return NX_ERROR_STATE;
    }
    USB_TypeDef* registers = endpoint->config->registers;
    const uint16_t control = registers->CNTR;
    if ((control &
         (USB_CNTR_FRES | USB_CNTR_PDWN | (INTERRUPT_MASK & ~LP_MASK))) != 0U) {
        return NX_ERROR_BUSY;
    }
    registers->CNTR = (uint16_t)(control | LP_MASK);
    return (registers->CNTR & LP_MASK) == LP_MASK ? NX_SUCCESS : NX_ERROR_IO;
}

nx_result_t nx_stm32f1_usb_lp_enable(const nx_stm32f1_ep0_t* endpoint) {
    if (endpoint == NULL || endpoint->config == NULL) {
        return NX_ERROR_STATE;
    }
    if (nx_stm32f1_usb_lp_pending(endpoint->config->registers)) {
        return NX_ERROR_BUSY;
    }
    return nx_stm32f1_usb_lp_rearm(endpoint);
}

bool nx_stm32f1_usb_lp_interrupt(USB_TypeDef* registers) {
    if (registers == NULL) {
        return false;
    }
    const uint16_t control = registers->CNTR;
    if ((registers->ISTR & control & LP_MASK) == 0U) {
        return false;
    }
    nx_stm32f1_usb_lp_mask(registers);
    return true;
}

typedef enum {
    LP_IDLE,
    LP_PACKET,
    LP_SETUP,
    LP_RESET,
    LP_SUSPEND,
    LP_RESUME
} lp_observation_t;

/** \brief Keep System clocks live and acknowledge actual bus states only. */
static lp_observation_t link_event(nx_stm32f1_ep0_t* endpoint, uint16_t status,
                                   uint16_t line) {
    USB_TypeDef* registers = endpoint->config->registers;
    const uint16_t control = registers->CNTR;
    const bool suspended =
        endpoint->reset_candidate || (control & USB_CNTR_FSUSP) != 0U;
    if ((status & USB_ISTR_SUSP) != 0U) {
        if (!endpoint->reset_candidate) {
            registers->CNTR = (uint16_t)(control | USB_CNTR_FSUSP);
        }
        clear_flags(registers, USB_ISTR_SUSP);
        return suspended ? LP_PACKET : LP_SUSPEND;
    }
    /* RM0008 Table 172: FSUSP clears on SE0 so the core can collect
     * RESET, while packet admission waits for the actual discriminant. */
    const bool waking = (status & USB_ISTR_WKUP) != 0U;
    const bool resume = suspended && waking && line == USB_FNR_RXDM;
    if (resume || (suspended && waking && line == 0U)) {
        endpoint->reset_candidate = !resume;
        registers->CNTR = (uint16_t)(control & ~USB_CNTR_FSUSP);
    } else if (endpoint->reset_candidate && line != 0U) {
        /* The line left SE0 without a visible RESET or fresh observed K wake.
         */
        registers->CNTR = (uint16_t)(control | USB_CNTR_FSUSP);
        endpoint->reset_candidate = false;
    }
    if (waking) {
        clear_flags(registers, USB_ISTR_WKUP);
    }
    return resume ? LP_RESUME : LP_PACKET;
}

/** \brief Service one source and describe only evidence handled here. */
static nx_result_t lp_step(nx_stm32f1_ep0_t* endpoint, nx_time_us_t now,
                           lp_observation_t* observation) {
    if (endpoint == NULL || endpoint->config == NULL) {
        return NX_ERROR_STATE;
    }
    USB_TypeDef* registers = endpoint->config->registers;
    if ((registers->CNTR & INTERRUPT_MASK) != 0U) {
        return NX_ERROR_BUSY;
    }
    const uint16_t status = registers->ISTR;
    if ((status & USB_ISTR_RESET) != 0U) {
        const nx_result_t result = nx_stm32f1_usb_ep0_bus_reset(endpoint);
        if (result != NX_SUCCESS) {
            return result;
        }
        registers->CNTR = (uint16_t)(registers->CNTR & ~USB_CNTR_FSUSP);
        clear_flags(registers,
                    (uint16_t)(status & (USB_ISTR_RESET | USB_ISTR_SUSP |
                                         USB_ISTR_WKUP)));
        *observation = LP_RESET;
        return NX_SUCCESS;
    }
    if ((status & USB_ISTR_CTR) == 0U &&
        ((status & LINK_TRANSITIONS) != 0U || endpoint->reset_candidate)) {
        const uint16_t line = registers->FNR & LINE_BITS;
        if ((status & LINK_TRANSITIONS) != 0U || line != 0U) {
            *observation = link_event(endpoint, status, line);
            return NX_SUCCESS;
        }
    }
    if ((status & USB_ISTR_CTR) != 0U && (status & USB_ISTR_EP_ID) != 0U) {
        return NX_ERROR_IO;
    }
    const uint64_t epoch = endpoint->setup.epoch;
    const nx_result_t result = nx_stm32f1_usb_ep0_service(endpoint, now);
    *observation = endpoint->setup.epoch != epoch
                       ? LP_SETUP
                       : ((status & USB_ISTR_CTR) != 0U ? LP_PACKET : LP_IDLE);
    return result;
}

nx_result_t nx_stm32f1_usb_lp_service(nx_stm32f1_ep0_t* endpoint,
                                      nx_time_us_t now, uint8_t budget) {
    if (budget == 0U) {
        return NX_ERROR_INVALID;
    }
    for (uint8_t attempted = 0U; attempted < budget; ++attempted) {
        lp_observation_t observation;
        const nx_result_t result = lp_step(endpoint, now, &observation);
        if (result != NX_SUCCESS || observation == LP_IDLE) {
            return result;
        }
    }
    return (endpoint->config->registers->ISTR & LP_SOURCES) == 0U
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}

nx_result_t nx_stm32f1_usb_lp_event(nx_stm32f1_ep0_t* endpoint,
                                    nx_time_us_t now, nx_usb_event_t* output) {
    if (output == NULL) {
        return NX_ERROR_INVALID;
    }
    lp_observation_t observation;
    const nx_result_t result = lp_step(endpoint, now, &observation);
    if (result != NX_SUCCESS) {
        return result;
    }
    switch (observation) {
        case LP_SETUP:
            *output = (nx_usb_event_t){.setup = endpoint->setup,
                                       .timestamp_us = now,
                                       .kind = NX_USB_EVENT_SETUP};
            break;
        case LP_RESET:
            *output = (nx_usb_event_t){.timestamp_us = now,
                                       .kind = NX_USB_EVENT_RESET};
            break;
        case LP_SUSPEND:
            *output = (nx_usb_event_t){.timestamp_us = now,
                                       .kind = NX_USB_EVENT_SUSPEND};
            break;
        case LP_RESUME:
            *output = (nx_usb_event_t){.timestamp_us = now,
                                       .kind = NX_USB_EVENT_RESUME};
            break;
        default:
            return NX_ERROR_EMPTY;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_usb_lp_close(nx_stm32f1_ep0_t* endpoint) {
    if (endpoint == NULL || endpoint->config == NULL) {
        return NX_ERROR_STATE;
    }
    USB_TypeDef* registers = endpoint->config->registers;
    if ((registers->CNTR & (INTERRUPT_MASK | USB_CNTR_FRES)) != USB_CNTR_FRES) {
        return NX_ERROR_BUSY;
    }
    clear_flags(registers, (uint16_t)(registers->ISTR & LP_SOURCES));
    if ((registers->ISTR & (LP_SOURCES & ~USB_ISTR_CTR)) != 0U) {
        return NX_ERROR_BUSY;
    }
    return nx_stm32f1_usb_ep0_close(endpoint);
}
