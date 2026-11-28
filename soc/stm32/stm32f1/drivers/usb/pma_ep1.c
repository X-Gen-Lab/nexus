/**
 * \file            pma_ep1.c
 * \brief           Single-buffer EP1 loans for STM32F103 Bulk and Interrupt.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "pma_ep1.h"
#include "nexus/arch/arch.h"

enum {
    DATA_EP = 1U,
    BOTH_CTR = USB_EP_CTR_RX | USB_EP_CTR_TX,
    BOTH_STAT = USB_EPRX_STAT | USB_EPTX_STAT
};

typedef struct {
    uint16_t status;
    uint16_t ctr;
    uint16_t dtog;
    uint16_t nak;
    uint16_t valid;
    uint16_t stall;
} direction_t;

static const direction_t directions[] = {
    {USB_EPRX_STAT, USB_EP_CTR_RX, USB_EP_DTOG_RX, USB_EP_RX_NAK,
     USB_EP_RX_VALID, USB_EP_RX_STALL},
    {USB_EPTX_STAT, USB_EP_CTR_TX, USB_EP_DTOG_TX, USB_EP_TX_NAK,
     USB_EP_TX_VALID, USB_EP_TX_STALL}};

static volatile uint16_t* ep1(const nx_stm32f1_usb_pma_device_t* state) {
    return &state->config->link.endpoint.registers->EP1R;
}

static void set_ep1(volatile uint16_t* reg, uint16_t observed, uint16_t ack,
                    uint16_t changes, uint16_t value) {
    nx_stm32f1_usb_epr_write(
        reg, nx_stm32f1_usb_epr_word(observed, ack, changes, value));
}

nx_result_t nx_stm32f1_usb_ep1_prepare(nx_stm32f1_usb_pma_device_t* state) {
    const uint16_t type = state->config->ep1_type;
    if (type != USB_EP_BULK && type != USB_EP_INTERRUPT) {
        return NX_ERROR_INVALID;
    }
    volatile uint16_t* reg = ep1(state);
    const uint16_t observed = *reg;
    if ((observed & (BOTH_CTR | BOTH_STAT)) != 0U)
        return NX_ERROR_BUSY;
    const nx_stm32f1_ep0_config_t* ep0 = &state->config->link.endpoint;
    const nx_stm32f1_usb_pma_endpoint_t* layout = &state->config->ep1_layout;
    nx_result_t result =
        nx_stm32f1_usb_pma_prepare_ep1(ep0->pma, &ep0->layout, layout);
    if (result != NX_SUCCESS)
        return result;
    /* Clear KIND for single buffering. Both directions stay disabled. */
    const uint16_t fields = USB_EP_TYPE_MASK | USB_EP_KIND | USB_EPADDR_FIELD;
    set_ep1(reg, observed, 0U, fields, type | DATA_EP);
    return (*reg & fields) == (type | DATA_EP) ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief A quiescent direction is safe to toggle without racing hardware. */
static bool inactive_source(uint16_t observed, const direction_t* dir) {
    return (observed & dir->ctr) == 0U &&
           (observed & dir->status) != dir->valid;
}

static nx_result_t activate(void* context, uint64_t epoch, uint8_t address,
                            bool active) {
    nx_stm32f1_usb_pma_device_t* state = context;
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (address != DATA_EP && address != (0x80U | DATA_EP))
        return NX_ERROR_INVALID;
    const nx_stm32f1_ep0_t* control = &state->link.endpoint;
    if (!state->attached || state->closing || !control->observed_full_speed ||
        control->phase != NX_STM32F1_EP0_IDLE ||
        epoch != control->setup.epoch ||
        (state->config->link.endpoint.registers->CNTR & USB_CNTR_FSUSP) != 0U ||
        (state->config->link.endpoint.registers->ISTR & USB_ISTR_RESET) != 0U)
        return NX_ERROR_STATE;
    const bool in = (address & 0x80U) != 0U;
    const direction_t* dir = &directions[in];
    volatile uint16_t* reg = ep1(state);
    const uint16_t observed = *reg;
    if (state->ep1[in].request != NULL || !inactive_source(observed, dir))
        return NX_ERROR_BUSY;
    if (((observed & dir->status) != 0U) == active)
        return NX_SUCCESS;
    const uint16_t expected = active ? dir->nak : 0U;
    set_ep1(reg, observed, 0U, (uint16_t)(dir->status | dir->dtog), expected);
    if ((*reg & (dir->status | dir->dtog)) == expected)
        return NX_SUCCESS;
    state->closing = true;
    return NX_ERROR_IO;
}

const nx_usb_activation_ops_t nx_stm32f1_usb_pma_ep1_activation_ops = {
    .set = activate,
};

/** \brief Remove the pointer before the caller can observe SETTLED. */
static void settle(nx_stm32f1_ep0_loan_t* loan, nx_result_t result, size_t n) {
    nx_usb_request_t* request = loan->request;
    loan->request = NULL;
    loan->terminating = NX_SUCCESS;
    if (request != NULL)
        nx_request_settle(&request->base, result, n);
}

nx_result_t nx_stm32f1_usb_ep1_submit(nx_stm32f1_usb_pma_device_t* state,
                                      nx_usb_request_t* request, bool in,
                                      nx_time_us_t now) {
    const nx_stm32f1_usb_pma_device_config_t* config = state->config;
    const nx_stm32f1_usb_pma_endpoint_t* layout = &config->ep1_layout;
    if (request->stage != NX_USB_DATA || request->control_epoch != 0U ||
        request->base.deadline == NX_DEADLINE_NEVER ||
        request->length > (in ? layout->tx_capacity : layout->rx_capacity) ||
        (in ? request->out != NULL : request->in != NULL))
        return NX_ERROR_INVALID;
    if ((config->link.endpoint.registers->ISTR & USB_ISTR_RESET) != 0U)
        return NX_ERROR_STATE;
    const direction_t* dir = &directions[in];
    nx_stm32f1_ep0_loan_t* loan = &state->ep1[in];
    volatile uint16_t* reg = ep1(state);
    const uint16_t observed = *reg;
    if ((observed & dir->status) == 0U)
        return NX_ERROR_STATE;
    if (loan->request != NULL ||
        (observed & (dir->ctr | dir->status)) != dir->nak)
        return NX_ERROR_BUSY;
    if (nx_deadline_expired(request->base.deadline, now))
        return NX_ERROR_TIMEOUT;
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    loan->request = request;
    if (in) {
        result =
            nx_stm32f1_usb_pma_transmit(config->link.endpoint.pma, DATA_EP,
                                        layout, request->in, request->length);
        if (result != NX_SUCCESS) {
            settle(loan, result, 0U);
            return NX_SUCCESS;
        }
    }
    const uint16_t latest = *reg;
    if ((config->link.endpoint.registers->ISTR & USB_ISTR_RESET) != 0U ||
        (latest & (dir->ctr | dir->status)) != dir->nak) {
        settle(loan, NX_ERROR_CANCELLED, 0U);
        return NX_SUCCESS;
    }
    set_ep1(reg, latest, 0U, dir->status, dir->valid);
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_usb_ep1_cancel(nx_stm32f1_usb_pma_device_t* state,
                                      nx_usb_request_t* request, bool in) {
    nx_stm32f1_ep0_loan_t* loan = &state->ep1[in];
    if (loan->request != request || loan->terminating != NX_SUCCESS)
        return NX_ERROR_STATE;
    /* VALID can transition in hardware. Only CTR or reset revokes this loan. */
    loan->terminating = NX_ERROR_CANCELLED;
    return nx_request_transition(&request->base, NX_REQUEST_DRAINING);
}

nx_result_t nx_stm32f1_usb_ep1_stall(nx_stm32f1_usb_pma_device_t* state,
                                     bool in, bool stalled) {
    const direction_t* dir = &directions[in];
    volatile uint16_t* reg = ep1(state);
    const uint16_t observed = *reg;
    if ((observed & dir->status) == 0U)
        return NX_ERROR_STATE;
    if (state->ep1[in].request != NULL || !inactive_source(observed, dir))
        return NX_ERROR_BUSY;
    const uint16_t mask =
        stalled ? dir->status : (uint16_t)(dir->status | dir->dtog);
    const uint16_t expected = stalled ? dir->stall : dir->nak;
    set_ep1(reg, observed, 0U, mask, expected);
    if ((*reg & mask) == expected)
        return NX_SUCCESS;
    state->closing = true;
    return NX_ERROR_IO;
}

void nx_stm32f1_usb_ep1_expire(nx_stm32f1_usb_pma_device_t* state,
                               nx_time_us_t now) {
    for (size_t i = 0U; i < 2U; ++i) {
        nx_stm32f1_ep0_loan_t* loan = &state->ep1[i];
        nx_usb_request_t* request = loan->request;
        if (request != NULL &&
            nx_deadline_expired(request->base.deadline, now) &&
            nx_request_state(&request->base) != NX_REQUEST_QUARANTINED) {
            if (loan->terminating == NX_SUCCESS)
                loan->terminating = NX_ERROR_TIMEOUT;
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        }
    }
}

nx_result_t nx_stm32f1_usb_ep1_packet(nx_stm32f1_usb_pma_device_t* state) {
    volatile uint16_t* reg = ep1(state);
    const uint16_t observed = *reg;
    const bool in = (observed & USB_EP_CTR_RX) == 0U;
    const direction_t* dir = &directions[in];
    if ((observed & (dir->status | dir->ctr)) != (dir->nak | dir->ctr))
        return NX_ERROR_BUSY;
    nx_stm32f1_ep0_loan_t* loan = &state->ep1[in];
    nx_usb_request_t* request = loan->request;
    if (request == NULL)
        return NX_ERROR_IO;
    nx_result_t result = loan->terminating;
    size_t count = 0U;
    if (result == NX_SUCCESS) {
        if (in) {
            count = request->length;
        } else {
            const nx_stm32f1_usb_pma_device_config_t* config = state->config;
            result = nx_stm32f1_usb_pma_receive(
                config->link.endpoint.pma, DATA_EP, &config->ep1_layout,
                request->out, request->length, &count);
        }
    }
    set_ep1(reg, observed, dir->ctr, 0U, 0U);
    if ((*reg & dir->ctr) != 0U)
        return NX_ERROR_BUSY;
    settle(loan, result, count);
    return NX_SUCCESS;
}

/** \brief Confirm EP1 stopped and retire only its preserved CTR flags. */
nx_result_t nx_stm32f1_usb_ep1_drain_reset(nx_stm32f1_usb_pma_device_t* state) {
    USB_TypeDef* regs = state->config->link.endpoint.registers;
    const bool bus_reset = (regs->ISTR & USB_ISTR_RESET) != 0U;
    if ((regs->CNTR & USB_CNTR_FRES) == 0U && !bus_reset)
        return NX_ERROR_STATE;
    volatile uint16_t* reg = &regs->EP1R;
    const uint16_t observed = *reg;
    if ((observed & BOTH_STAT) != 0U && !bus_reset)
        return NX_ERROR_BUSY;
    /*
     * RM0008 USB_ISTR.RESET blocks transfers until acknowledged. A late
     * pre-reset CPU write can leave STAT set after the hardware reset.
     */
    if ((observed & (BOTH_STAT | BOTH_CTR)) != 0U)
        set_ep1(reg, observed, BOTH_CTR, BOTH_STAT, 0U);
    return (*reg & (BOTH_STAT | BOTH_CTR)) == 0U ? NX_SUCCESS : NX_ERROR_BUSY;
}

void nx_stm32f1_usb_ep1_revoke(nx_stm32f1_usb_pma_device_t* state) {
    for (size_t i = 0U; i < 2U; ++i) {
        nx_stm32f1_ep0_loan_t* loan = &state->ep1[i];
        settle(loan,
               loan->terminating == NX_SUCCESS ? NX_ERROR_CANCELLED
                                               : loan->terminating,
               0U);
    }
}
