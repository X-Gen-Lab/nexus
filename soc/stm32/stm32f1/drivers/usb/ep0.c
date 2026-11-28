/**
 * \file            ep0.c
 *
 * \brief           Private STM32F1 single-buffer EP0 packet ownership.
 *
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "ep0.h"

enum {
    CTR_BITS = USB_EP_CTR_RX | USB_EP_CTR_TX,
    STAT_BITS = USB_EPRX_STAT | USB_EPTX_STAT,
    RW_BITS = USB_EP_TYPE_MASK | USB_EP_KIND | USB_EPADDR_FIELD,
    TOGGLE_BITS = STAT_BITS | USB_EP_DTOG_RX | USB_EP_DTOG_TX,
    NAK_BOTH = USB_EP_RX_NAK | USB_EP_TX_NAK,
    SET_ADDRESS = 5U,
    SETUP_BYTES = 8U
};

uint16_t nx_stm32f1_usb_epr_word(uint16_t snapshot, uint16_t acknowledge,
                                 uint16_t changes, uint16_t desired) {
    const unsigned direct =
        ((snapshot & ~(unsigned)changes) | (desired & changes)) & RW_BITS;
    const unsigned commands = (snapshot ^ desired) & changes & TOGGLE_BITS;
    const unsigned keep = CTR_BITS & ~(unsigned)(snapshot & acknowledge);
    return (uint16_t)(direct | commands | keep);
}

/** \brief Apply one reviewed snapshot without copying raw register bits. */
static void write_endpoint(const nx_stm32f1_ep0_t* state, uint16_t snapshot,
                           uint16_t acknowledge, uint16_t changes,
                           uint16_t desired) {
    nx_stm32f1_usb_epr_write(
        &state->config->registers->EP0R,
        nx_stm32f1_usb_epr_word(snapshot, acknowledge, changes, desired));
}

/** \brief Decode one unaligned little-endian SETUP field. */
static uint16_t le16(const uint8_t* data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

/** \brief A default control endpoint uses equal 8, 16, 32 or 64 B MPS. */
static bool control_layout(const nx_stm32f1_usb_pma_endpoint_t* layout) {
    const uint16_t capacity = layout->tx_capacity;
    return capacity == layout->rx_capacity && capacity >= 8U &&
           capacity <= 64U && (capacity & (capacity - 1U)) == 0U;
}

nx_result_t nx_stm32f1_usb_ep0_start(nx_stm32f1_ep0_t* state,
                                     const nx_stm32f1_ep0_config_t* config) {
    if (state == NULL || config == NULL || config->registers == NULL ||
        config->pma == NULL || !control_layout(&config->layout)) {
        return NX_ERROR_INVALID;
    }
    if (state->config != NULL || state->in.request != NULL ||
        state->out.request != NULL) {
        return NX_ERROR_STATE;
    }
    USB_TypeDef* registers = config->registers;
    const uint16_t snapshot = registers->EP0R;
    if ((snapshot & (CTR_BITS | STAT_BITS)) != 0U ||
        (registers->DADDR & USB_DADDR_EF) != 0U || registers->BTABLE != 0U ||
        (registers->CNTR & USB_CNTR_FRES) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result =
        nx_stm32f1_usb_pma_prepare(config->pma, &config->layout, 1U);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->config = config;
    state->phase = NX_STM32F1_EP0_IDLE;
    write_endpoint(state, snapshot, 0U, RW_BITS | TOGGLE_BITS,
                   USB_EP_CONTROL | NAK_BOTH);
    registers->DADDR = USB_DADDR_EF;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_usb_ep0_address(nx_stm32f1_ep0_t* state, uint64_t epoch,
                                       uint8_t address) {
    if (state == NULL || address > 127U || epoch == 0U) {
        return NX_ERROR_INVALID;
    }
    if (state->config == NULL || state->in.request != NULL ||
        state->out.request != NULL ||
        state->phase != NX_STM32F1_EP0_STATUS_IN ||
        state->setup.epoch != epoch || state->setup.request_type != 0U ||
        state->setup.request != SET_ADDRESS || state->setup.index != 0U ||
        state->setup.length != 0U || state->setup.value != address) {
        return NX_ERROR_STATE;
    }
    state->staged_address = address;
    state->address_pending = true;
    return NX_SUCCESS;
}

/** \brief Read the direction the current control stage owns. */
static bool in_phase(nx_stm32f1_ep0_phase_t phase) {
    return phase == NX_STM32F1_EP0_DATA_IN || phase == NX_STM32F1_EP0_STATUS_IN;
}

/** \brief Recognize the independent STATUS OUT side of a control read. */
static bool early_status_out(const nx_stm32f1_ep0_t* state, bool in,
                             const nx_usb_request_t* request) {
    return !in && state->phase == NX_STM32F1_EP0_DATA_IN &&
           request->stage == NX_USB_STATUS;
}

/** \brief Match finite packet arguments before creating a borrow. */
static bool accepts(const nx_stm32f1_ep0_t* state,
                    const nx_usb_request_t* request, bool in) {
    if (request->control_epoch != state->setup.epoch ||
        (in ? request->out != NULL : request->in != NULL)) {
        return false;
    }
    if (early_status_out(state, in, request)) {
        return request->length == 0U;
    }
    const bool status = state->phase == NX_STM32F1_EP0_STATUS_IN ||
                        state->phase == NX_STM32F1_EP0_STATUS_OUT;
    if (state->phase == NX_STM32F1_EP0_IDLE || in != in_phase(state->phase) ||
        request->stage != (status ? NX_USB_STATUS : NX_USB_DATA)) {
        return false;
    }
    const size_t capacity = in ? state->config->layout.tx_capacity
                               : state->config->layout.rx_capacity;
    return request->length <= capacity && request->length <= state->remaining &&
           (request->length == 0U ||
            (in ? request->in != NULL : request->out != NULL));
}

/** \brief Remove the last software reference before terminal publication. */
static void settle(nx_stm32f1_ep0_loan_t* loan, nx_result_t result,
                   size_t count) {
    nx_usb_request_t* request = loan->request;
    loan->request = NULL;
    loan->terminating = NX_SUCCESS;
    if (request != NULL) {
        nx_request_settle(&request->base, result, count);
    }
}

/** \brief Preserve the documented reason across an independent reset proof. */
static void withdrawn(nx_stm32f1_ep0_loan_t* loan) {
    settle(loan,
           loan->terminating == NX_SUCCESS ? NX_ERROR_CANCELLED
                                           : loan->terminating,
           0U);
}

/** \brief Keep a requested old loan reserved while its source remains live. */
static void withdraw(nx_stm32f1_ep0_loan_t* loan) {
    if (loan->request != NULL && loan->terminating == NX_SUCCESS) {
        (void)nx_request_transition(&loan->request->base, NX_REQUEST_DRAINING);
        loan->terminating = NX_ERROR_CANCELLED;
    }
}

nx_result_t nx_stm32f1_usb_ep0_submit(nx_stm32f1_ep0_t* state,
                                      nx_usb_request_t* request, bool in,
                                      nx_time_us_t now) {
    if (state == NULL || request == NULL ||
        request->base.deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (state->config == NULL || !accepts(state, request, in)) {
        return NX_ERROR_STATE;
    }
    nx_stm32f1_ep0_loan_t* loan = in ? &state->in : &state->out;
    if (loan->request != NULL) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(request->base.deadline, now)) {
        return NX_ERROR_TIMEOUT;
    }
    const nx_stm32f1_ep0_config_t* config = state->config;
    const uint16_t mask = in ? USB_EPTX_STAT : USB_EPRX_STAT;
    const uint16_t nak = in ? USB_EP_TX_NAK : USB_EP_RX_NAK;
    const uint16_t snapshot = config->registers->EP0R;
    if ((snapshot & (CTR_BITS | mask)) != nak) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        return result;
    }
    loan->request = request;
    if (in) {
        result = nx_stm32f1_usb_pma_transmit(config->pma, 0U, &config->layout,
                                             request->in, request->length);
        if (result != NX_SUCCESS) {
            settle(loan, result, 0U);
            return NX_SUCCESS;
        }
    }
    const uint16_t latest = config->registers->EP0R;
    if ((latest & (CTR_BITS | mask)) != nak) {
        /* This request was admitted but its hardware source was never armed. */
        settle(loan, NX_ERROR_CANCELLED, 0U);
        return NX_SUCCESS;
    }
    const uint16_t enable = in ? USB_EP_TX_VALID : USB_EP_RX_VALID;
    const bool status_out = !in && request->stage == NX_USB_STATUS;
    const uint16_t changes = in ? mask : (uint16_t)(mask | USB_EP_KIND);
    write_endpoint(state, latest, 0U, changes,
                   (uint16_t)(enable | (status_out ? USB_EP_KIND : 0U)));
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_usb_ep0_cancel(nx_stm32f1_ep0_t* state,
                                      nx_usb_request_t* request) {
    if (state == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_stm32f1_ep0_loan_t* loan =
        state->in.request == request ? &state->in : &state->out;
    if (loan->request != request || loan->terminating != NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    withdraw(loan);
    return NX_SUCCESS;
}

/** \brief Replace the control epoch only after sampling a protected SETUP. */
static nx_result_t setup_packet(nx_stm32f1_ep0_t* state, uint16_t snapshot) {
    if ((snapshot & STAT_BITS) != NAK_BOTH) {
        return NX_ERROR_BUSY;
    }
    uint8_t bytes[SETUP_BYTES];
    size_t count;
    const nx_stm32f1_ep0_config_t* config = state->config;
    const nx_result_t result = nx_stm32f1_usb_pma_receive(
        config->pma, 0U, &config->layout, bytes, sizeof(bytes), &count);
    write_endpoint(state, snapshot, CTR_BITS, USB_EP_KIND, 0U);
    state->phase = NX_STM32F1_EP0_IDLE;
    state->address_pending = false;
    state->remaining = 0U;
    withdrawn(&state->in);
    withdrawn(&state->out);
    if (result != NX_SUCCESS || count != SETUP_BYTES) {
        return NX_ERROR_IO;
    }
    if (state->setup.epoch == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    state->setup.request_type = bytes[0];
    state->setup.request = bytes[1];
    state->setup.value = le16(&bytes[2]);
    state->setup.index = le16(&bytes[4]);
    state->setup.length = le16(&bytes[6]);
    state->setup.epoch++;
    state->observed_full_speed = true;
    state->remaining = state->setup.length;
    state->phase = state->remaining == 0U
                       ? NX_STM32F1_EP0_STATUS_IN
                       : ((bytes[0] & 0x80U) != 0U ? NX_STM32F1_EP0_DATA_IN
                                                   : NX_STM32F1_EP0_DATA_OUT);
    return NX_SUCCESS;
}

/** \brief Retire one owner after proof of its wire completion and ACK clear. */
static nx_result_t completed(nx_stm32f1_ep0_t* state, uint16_t snapshot,
                             bool in) {
    const uint16_t ctr = in ? USB_EP_CTR_TX : USB_EP_CTR_RX;
    const uint16_t mask = in ? USB_EPTX_STAT : USB_EPRX_STAT;
    const uint16_t nak = in ? USB_EP_TX_NAK : USB_EP_RX_NAK;
    if ((snapshot & mask) != nak) {
        return NX_ERROR_BUSY;
    }
    nx_stm32f1_ep0_loan_t* loan = in ? &state->in : &state->out;
    nx_stm32f1_ep0_loan_t* other = in ? &state->out : &state->in;
    const bool status = loan->request->stage == NX_USB_STATUS;
    nx_result_t result = loan->terminating;
    size_t count = 0U;
    if (result == NX_SUCCESS) {
        if (in) {
            count = loan->request->length;
        } else {
            result = nx_stm32f1_usb_pma_receive(
                state->config->pma, 0U, &state->config->layout,
                loan->request->out, loan->request->length, &count);
        }
    }
    write_endpoint(state, snapshot, ctr, !in && status ? USB_EP_KIND : 0U, 0U);
    if (result != NX_SUCCESS || status) {
        if (result == NX_SUCCESS && in && state->address_pending) {
            const uint16_t current = state->config->registers->EP0R;
            if ((current & (USB_EP_CTR_RX | USB_EP_SETUP)) ==
                (USB_EP_CTR_RX | USB_EP_SETUP)) {
                return setup_packet(state, current);
            }
            state->config->registers->DADDR =
                (uint16_t)(USB_DADDR_EF | state->staged_address);
        }
        state->phase = NX_STM32F1_EP0_IDLE;
        state->remaining = 0U;
        state->address_pending = false;
        withdraw(other);
    } else {
        state->remaining = (uint16_t)(state->remaining - count);
        if (state->remaining == 0U ||
            (in && count < state->config->layout.tx_capacity)) {
            state->remaining = 0U;
            state->phase =
                in ? NX_STM32F1_EP0_STATUS_OUT : NX_STM32F1_EP0_STATUS_IN;
        }
    }
    settle(loan, result, count);
    return NX_SUCCESS;
}

/** \brief Latch an expired independent deadline without revoking the loan. */
static void expire(nx_stm32f1_ep0_loan_t* loan, nx_time_us_t now) {
    nx_usb_request_t* request = loan->request;
    if (request == NULL || !nx_deadline_expired(request->base.deadline, now) ||
        nx_request_state(&request->base) == NX_REQUEST_QUARANTINED) {
        return;
    }
    if (loan->terminating == NX_SUCCESS) {
        loan->terminating = NX_ERROR_TIMEOUT;
    }
    (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
}

nx_result_t nx_stm32f1_usb_ep0_service(nx_stm32f1_ep0_t* state,
                                       nx_time_us_t now) {
    if (state == NULL || state->config == NULL) {
        return NX_ERROR_STATE;
    }
    const uint16_t snapshot = state->config->registers->EP0R;
    if ((snapshot & (USB_EP_CTR_RX | USB_EP_SETUP)) ==
        (USB_EP_CTR_RX | USB_EP_SETUP)) {
        return setup_packet(state, snapshot);
    }
    expire(&state->in, now);
    expire(&state->out, now);
    /* A host may close its read by completing STATUS OUT before DATA IN is
     * withdrawn. Process RX first and keep the TX source until separately
     * acknowledged or reset. */
    if ((snapshot & USB_EP_CTR_RX) != 0U && state->out.request != NULL) {
        return completed(state, snapshot, false);
    }
    if ((snapshot & USB_EP_CTR_TX) != 0U && state->in.request != NULL) {
        return completed(state, snapshot, true);
    }
    if ((snapshot & CTR_BITS) != 0U) {
        write_endpoint(state, snapshot, CTR_BITS, 0U, 0U);
        return NX_ERROR_IO;
    }
    return state->in.request != NULL || state->out.request != NULL
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}

/** \brief Invalidate the current setup while retaining a monotonic epoch. */
static void reset_control(nx_stm32f1_ep0_t* state) {
    state->phase = NX_STM32F1_EP0_IDLE;
    state->address_pending = false;
    state->remaining = 0U;
    state->config = NULL;
    state->observed_full_speed = false;
    withdrawn(&state->in);
    withdrawn(&state->out);
}

/** \brief RESET disables STAT but preserves the old CPU-owned CTR latches. */
static nx_result_t drain_reset(const nx_stm32f1_ep0_t* state) {
    USB_TypeDef* registers = state->config->registers;
    const uint16_t snapshot = registers->EP0R;
    if ((snapshot & STAT_BITS) != 0U) {
        return NX_ERROR_BUSY;
    }
    const uint16_t pending = snapshot & CTR_BITS;
    if (pending != 0U) {
        /*
         * RM0008 §23.5.2 retains CTR across RESET or FRES. Source
         * withdrawal is already established; no old payload is published.
         * The halfword command acknowledges exactly these observed flags.
         */
        write_endpoint(state, snapshot, pending, 0U, 0U);
    }
    return (registers->EP0R & (CTR_BITS | STAT_BITS)) == 0U &&
                   (registers->ISTR & USB_ISTR_CTR) == 0U
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}

nx_result_t nx_stm32f1_usb_ep0_bus_reset(nx_stm32f1_ep0_t* state) {
    if (state == NULL || state->config == NULL) {
        return NX_ERROR_STATE;
    }
    const nx_stm32f1_ep0_config_t* config = state->config;
    USB_TypeDef* registers = config->registers;
    if ((registers->ISTR & USB_ISTR_RESET) == 0U) {
        return NX_ERROR_STATE;
    }
    if ((registers->CNTR & USB_CNTR_FRES) != 0U ||
        drain_reset(state) != NX_SUCCESS) {
        return NX_ERROR_BUSY;
    }
    registers->DADDR = 0U;
    reset_control(state);
    return nx_stm32f1_usb_ep0_start(state, config);
}

nx_result_t nx_stm32f1_usb_ep0_close(nx_stm32f1_ep0_t* state) {
    if (state == NULL || state->config == NULL) {
        return NX_ERROR_STATE;
    }
    const USB_TypeDef* registers = state->config->registers;
    if ((registers->CNTR & USB_CNTR_FRES) == 0U ||
        (registers->DADDR & USB_DADDR_EF) != 0U ||
        drain_reset(state) != NX_SUCCESS) {
        return NX_ERROR_BUSY;
    }
    reset_control(state);
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_usb_ep0_snapshot(const nx_stm32f1_ep0_t* state,
                                        nx_usb_device_state_t* output) {
    if (state == NULL || output == NULL) {
        return NX_ERROR_INVALID;
    }
    if (state->config == NULL) {
        return NX_ERROR_STATE;
    }
    const USB_TypeDef* registers = state->config->registers;
    const uint16_t address = registers->DADDR;
    const uint16_t control = registers->CNTR;
    output->setup = state->setup;
    output->epoch = state->setup.epoch;
    output->lost_events = 0U;
    output->started = (address & USB_DADDR_EF) != 0U;
    output->closing = (control & USB_CNTR_FRES) != 0U;
    output->suspended = (control & USB_CNTR_FSUSP) != 0U;
    output->control_pending = state->phase != NX_STM32F1_EP0_IDLE;
    output->address = (uint8_t)(address & USB_DADDR_ADD);
    output->speed = state->observed_full_speed ? NX_USB_DEVICE_FULL_SPEED
                                               : NX_USB_DEVICE_SPEED_UNKNOWN;
    output->remote_wakeup_enabled = false;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_usb_ep0_stall(nx_stm32f1_ep0_t* state, bool in,
                                     bool stalled) {
    if (state == NULL || state->config == NULL) {
        return NX_ERROR_STATE;
    }
    const nx_stm32f1_ep0_loan_t* loan = in ? &state->in : &state->out;
    const uint16_t mask = in ? USB_EPTX_STAT : USB_EPRX_STAT;
    const uint16_t nak = in ? USB_EP_TX_NAK : USB_EP_RX_NAK;
    const uint16_t stall = in ? USB_EP_TX_STALL : USB_EP_RX_STALL;
    const uint16_t snapshot = state->config->registers->EP0R;
    const uint16_t status = snapshot & mask;
    if (loan->request != NULL || (snapshot & CTR_BITS) != 0U ||
        (status != nak && status != stall)) {
        return NX_ERROR_BUSY;
    }
    write_endpoint(state, snapshot, 0U, mask, stalled ? stall : nak);
    if (stalled) {
        state->phase = NX_STM32F1_EP0_IDLE;
        state->remaining = 0U;
        state->address_pending = false;
        withdraw(in ? &state->out : &state->in);
    }
    const uint16_t observed = state->config->registers->EP0R & mask;
    return observed == (stalled ? stall : nak) ? NX_SUCCESS : NX_ERROR_IO;
}
