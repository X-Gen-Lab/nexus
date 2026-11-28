/**
 * \file            pma_device.c
 * \brief           F1 PMA USB Device value events and EP0 task ownership.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "pma_device.h"
#include "dispatch_checks.h"
#include "nexus/arch/arch.h"

#if defined(NX_STM32F1_USB_PMA_EP1)
#include "pma_ep1.h"
#endif

enum {
    LP_IRQ = NX_STM32F1_USB_LP_IRQ,
    LP_IRQ_BIT = 1U << LP_IRQ,
    STEPS_PER_SERVICE = 8U
};

_Static_assert(LP_IRQ == 20, "F1 USB LP must use external IRQ20");

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool overlap(const void* a, size_t size, const void* b, size_t count) {
    return size != 0U && count != 0U &&
           nx_io_dispatch_overlap(a, size, b, count);
}

/** \brief Preserve both directions and their buffers until actual settlement.
 */
static bool overlaps_loan(const nx_stm32f1_ep0_loan_t* loan, const void* bytes,
                          size_t size, bool in) {
    const nx_usb_request_t* request = loan->request;
    if (request == NULL) {
        return false;
    }
    const void* payload = in ? (const void*)request->in : request->out;
    return overlap(bytes, size, request, sizeof(*request)) ||
           overlap(bytes, size, payload, request->length);
}

/** \brief Protect every owned RAM and hardware extent from public copies. */
static bool independent(const nx_stm32f1_usb_pma_device_t* state,
                        const void* data, size_t size, size_t align) {
    if (!nx_io_dispatch_span(data, size, align) ||
        overlap(data, size, state, sizeof(*state))) {
        return false;
    }
    const nx_stm32f1_usb_pma_device_config_t* config = state->config;
    if (config == NULL) {
        return true;
    }
#if defined(NX_STM32F1_USB_PMA_EP1)
    if (overlaps_loan(&state->ep1[0], data, size, false) ||
        overlaps_loan(&state->ep1[1], data, size, true)) {
        return false;
    }
#endif
    return !overlap(data, size, config, sizeof(*config)) &&
           !overlap(data, size, config->events,
                    config->capacity * sizeof(*config->events)) &&
           !overlap(data, size, config->link.endpoint.registers,
                    sizeof(*config->link.endpoint.registers)) &&
           !overlap(data, size, (const void*)config->link.endpoint.pma,
                    NX_STM32F1_USB_PMA_BYTES * sizeof(uint16_t)) &&
           !overlap(data, size, config->link.rcc, sizeof(*config->link.rcc)) &&
           !overlap(data, size, config->link.nvic,
                    sizeof(*config->link.nvic)) &&
           !overlaps_loan(&state->link.endpoint.in, data, size, true) &&
           !overlaps_loan(&state->link.endpoint.out, data, size, false);
}

nx_result_t nx_stm32f1_usb_pma_device_begin(
    nx_stm32f1_usb_pma_device_t* state,
    const nx_stm32f1_usb_pma_device_config_t* config) {
    if (state == NULL || config == NULL || config->capacity == 0U ||
        config->priority >= (1U << __NVIC_PRIO_BITS) ||
        !nx_io_dispatch_span(config->events,
                             config->capacity * sizeof(*config->events),
                             _Alignof(nx_usb_event_t))) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config != NULL) {
        return NX_ERROR_STATE;
    }
    const size_t bytes = config->capacity * sizeof(*config->events);
    if (overlap(config->events, bytes, state, sizeof(*state)) ||
        overlap(config->events, bytes, config, sizeof(*config))) {
        return NX_ERROR_INVALID;
    }
    const nx_result_t result =
        nx_stm32f1_usb_link_begin(&state->link, &config->link);
    if (state->link.config != NULL) {
        state->config = config;
        state->closing = result != NX_SUCCESS;
    }
    return result;
}

nx_result_t
nx_stm32f1_usb_pma_device_connect(nx_stm32f1_usb_pma_device_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config == NULL || state->closing || state->attached) {
        return NX_ERROR_STATE;
    }
    if (!nx_deadline_expired(state->link.ready_at, nx_time_now_us())) {
        return NX_ERROR_BUSY;
    }
    NVIC_Type* nvic = state->config->link.nvic;
    if (!state->nvic_owned) {
        if (((nvic->ISER[0] | nvic->ISPR[0] | nvic->IABR[0]) & LP_IRQ_BIT) !=
            0U) {
            return NX_ERROR_BUSY;
        }
        state->previous_priority = nvic->IPR[LP_IRQ];
        const uint8_t priority =
            (uint8_t)(state->config->priority << (8U - __NVIC_PRIO_BITS));
        nvic->IPR[LP_IRQ] = priority;
        state->nvic_owned = true;
        if (nvic->IPR[LP_IRQ] != priority) {
            state->closing = true;
            return NX_ERROR_IO;
        }
        nx_stm32f1_usb_lp_nvic_enable(nvic);
        nx_arch_dsb();
        if ((nvic->ISER[0] & LP_IRQ_BIT) == 0U) {
            state->closing = true;
            return NX_ERROR_IO;
        }
    }
#if defined(NX_STM32F1_USB_PMA_EP1)
    nx_result_t result = nx_stm32f1_usb_link_connect(&state->link);
    if (result == NX_SUCCESS)
        result = nx_stm32f1_usb_ep1_prepare(state);
#else
    const nx_result_t result = nx_stm32f1_usb_link_connect(&state->link);
#endif
    state->attached = result == NX_SUCCESS;
    state->closing =
        !state->attached &&
        (state->config->link.endpoint.registers->CNTR & USB_CNTR_FRES) == 0U;
    return result;
}

void nx_stm32f1_usb_pma_device_irq(nx_stm32f1_usb_pma_device_t* state) {
    if (state != NULL && state->nvic_owned) {
        (void)nx_stm32f1_usb_lp_interrupt(
            state->config->link.endpoint.registers);
    }
}

/** \brief Loss follows all retained events and precedes any newer records. */
static void push(nx_stm32f1_usb_pma_device_t* state, nx_usb_event_t event) {
    const uint8_t capacity = state->config->capacity;
    if (state->count == capacity || state->lost != 0U) {
        state->lost += state->lost != UINT64_MAX;
        state->lost_total += state->lost_total != UINT64_MAX;
        state->loss_time = event.timestamp_us;
        return;
    }
    unsigned tail = (unsigned)state->head + state->count;
    if (tail >= capacity) {
        tail -= capacity;
    }
    state->config->events[tail] = event;
    ++state->count;
}

/** \brief Disconnect first, then join LP and prove CPU/PMA source withdrawal.
 */
static nx_result_t stop(void* context) {
    nx_stm32f1_usb_pma_device_t* state = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config == NULL) {
        return NX_SUCCESS;
    }
    state->closing = true;
    nx_result_t result = nx_stm32f1_usb_link_disconnect(&state->link);
    if (result != NX_SUCCESS) {
        return result;
    }
    NVIC_Type* nvic = state->config->link.nvic;
    if (state->nvic_owned) {
        nx_stm32f1_usb_lp_nvic_disable(nvic);
        nx_arch_dsb();
        nx_arch_isb();
        if (((nvic->ISER[0] | nvic->IABR[0]) & LP_IRQ_BIT) != 0U) {
            return NX_ERROR_BUSY;
        }
        nx_stm32f1_usb_lp_nvic_ack(nvic);
        nx_arch_dsb();
        if ((nvic->ISPR[0] & LP_IRQ_BIT) != 0U) {
            return NX_ERROR_BUSY;
        }
        nvic->IPR[LP_IRQ] = state->previous_priority;
        if (nvic->IPR[LP_IRQ] != state->previous_priority) {
            return NX_ERROR_IO;
        }
        state->nvic_owned = false;
    }
#if defined(NX_STM32F1_USB_PMA_EP1)
    result = nx_stm32f1_usb_ep1_drain_reset(state);
    if (result != NX_SUCCESS)
        return result;
#endif
    result = nx_stm32f1_usb_link_release(&state->link);
    if (result == NX_SUCCESS) {
#if defined(NX_STM32F1_USB_PMA_EP1)
        nx_stm32f1_usb_ep1_revoke(state);
#endif
        state->config = NULL;
        state->head = 0U;
        state->count = 0U;
        state->lost = 0U;
        state->lost_total = 0U;
        state->attached = false;
        state->closing = false;
    }
    return result;
}

/** \brief LP ISR masks causes; only this task consumes PMA and writes events.
 */
static nx_result_t service(void* context) {
    nx_stm32f1_usb_pma_device_t* state = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->attached || state->closing) {
        return NX_ERROR_STATE;
    }
    bool vbus;
    nx_result_t result = nx_gpio_signal_read(state->config->link.vbus, &vbus);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!vbus) {
        return stop(state);
    }

    nx_stm32f1_ep0_t* ep0 = &state->link.endpoint;
    USB_TypeDef* registers = state->config->link.endpoint.registers;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_stm32f1_usb_lp_mask(registers);
    nx_arch_irq_restore(mask);
#if defined(NX_STM32F1_USB_PMA_EP1)
    nx_stm32f1_usb_ep1_expire(state, nx_time_now_us());
#endif
    for (unsigned step = 0U; step < STEPS_PER_SERVICE; ++step) {
#if defined(NX_STM32F1_USB_PMA_EP1)
        const uint16_t observed = registers->ISTR;
        if ((observed & USB_ISTR_RESET) != 0U) {
            result = nx_stm32f1_usb_ep1_drain_reset(state);
            if (result != NX_SUCCESS)
                break;
        } else if ((observed & (USB_ISTR_CTR | USB_ISTR_EP_ID)) ==
                   (USB_ISTR_CTR | 1U)) {
            result = nx_stm32f1_usb_ep1_packet(state);
            if (result != NX_SUCCESS)
                break;
            continue;
        }
#endif
        const bool observed_speed = ep0->observed_full_speed;
        nx_usb_event_t event;
        result = nx_stm32f1_usb_lp_event(ep0, nx_time_now_us(), &event);
        if (result == NX_SUCCESS) {
#if defined(NX_STM32F1_USB_PMA_EP1)
            if (event.kind == NX_USB_EVENT_RESET) {
                nx_stm32f1_usb_ep1_revoke(state);
                result = nx_stm32f1_usb_ep1_prepare(state);
                if (result != NX_SUCCESS) {
                    state->closing = true;
                    break;
                }
            }
#endif
            if (!observed_speed && event.kind == NX_USB_EVENT_SETUP) {
                push(state, (nx_usb_event_t){.timestamp_us = event.timestamp_us,
                                             .kind = NX_USB_EVENT_ENUMERATED});
            }
            push(state, event);
        } else if (result != NX_ERROR_EMPTY) {
            break;
        }
        if (!nx_stm32f1_usb_lp_pending(registers)) {
            break;
        }
    }
    /*
     * A bus event after the last poll is represented by an asserted source
     * when rearmed. Reenable only this provider's masks, never other events.
     */
    mask = nx_arch_irq_save();
    const nx_result_t rearmed = nx_stm32f1_usb_lp_rearm(ep0);
    nx_arch_irq_restore(mask);
    if (rearmed != NX_SUCCESS) {
        return rearmed;
    }
    return result == NX_ERROR_EMPTY || result == NX_ERROR_BUSY ? NX_SUCCESS
                                                               : result;
}

static nx_result_t read_events(void* context, nx_usb_event_t* output,
                               size_t capacity, size_t* count) {
    nx_stm32f1_usb_pma_device_t* state = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (capacity == 0U || capacity > SIZE_MAX / sizeof(*output) ||
        !independent(state, output, capacity * sizeof(*output),
                     _Alignof(nx_usb_event_t)) ||
        !independent(state, count, sizeof(*count), _Alignof(size_t)) ||
        overlap(output, capacity * sizeof(*output), count, sizeof(*count))) {
        return NX_ERROR_INVALID;
    }
    if (!state->attached) {
        return NX_ERROR_STATE;
    }
    size_t copied = 0U;
    while (copied < capacity && state->count != 0U) {
        output[copied++] = state->config->events[state->head];
        if (++state->head == state->config->capacity) {
            state->head = 0U;
        }
        --state->count;
    }
    if (copied < capacity && state->lost != 0U) {
        output[copied++] = (nx_usb_event_t){
            .timestamp_us = state->loss_time,
            .lost = state->lost,
            .kind = NX_USB_EVENT_LOSS,
        };
        state->lost = 0U;
    }
    *count = copied;
    return copied == 0U ? NX_ERROR_EMPTY : NX_SUCCESS;
}

static nx_result_t snapshot(void* context, nx_usb_device_state_t* output) {
    const nx_stm32f1_usb_pma_device_t* state = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!independent(state, output, sizeof(*output),
                     _Alignof(nx_usb_device_state_t))) {
        return NX_ERROR_INVALID;
    }
    nx_usb_device_state_t value = {0};
    if (state->link.endpoint.config != NULL) {
        const nx_result_t result =
            nx_stm32f1_usb_ep0_snapshot(&state->link.endpoint, &value);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    value.started = state->attached;
    value.closing = state->closing;
    value.lost_events = state->lost_total;
    *output = value;
    return NX_SUCCESS;
}

/** \brief Only an established active link may expose new packet effects. */
static bool packet_admission(const nx_stm32f1_usb_pma_device_t* state) {
    return state->attached && !state->closing &&
           nx_stm32f1_usb_lp_ready(&state->link.endpoint);
}

static nx_result_t address(void* context, uint64_t epoch, uint8_t value) {
    nx_stm32f1_usb_pma_device_t* state = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    return packet_admission(state)
               ? nx_stm32f1_usb_ep0_address(&state->link.endpoint, epoch, value)
               : NX_ERROR_STATE;
}

/** \brief Check aliases before the concrete directional operation reads data.
 */
static nx_result_t submit(void* context, nx_usb_request_t* request) {
    const nx_stm32f1_usb_pma_endpoint_face_t* face = context;
    nx_stm32f1_usb_pma_device_t* state = face->device;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!packet_admission(state)) {
        return NX_ERROR_STATE;
    }
    if (!independent(state, request, sizeof(*request),
                     _Alignof(nx_usb_request_t)) ||
        overlap(request, sizeof(*request), face, sizeof(*face))) {
        return NX_ERROR_INVALID;
    }
    const void* bytes = face->in ? (const void*)request->in : request->out;
    if (request->length != 0U &&
        (!independent(state, bytes, request->length, _Alignof(uint8_t)) ||
         overlap(bytes, request->length, request, sizeof(*request)) ||
         overlap(bytes, request->length, face, sizeof(*face)))) {
        return NX_ERROR_INVALID;
    }
#if defined(NX_STM32F1_USB_PMA_EP1)
    if (face->number == 1U)
        return nx_stm32f1_usb_ep1_submit(state, request, face->in,
                                         nx_time_now_us());
#endif
    return nx_stm32f1_usb_ep0_submit(&state->link.endpoint, request, face->in,
                                     nx_time_now_us());
}

static nx_result_t cancel(void* context, nx_usb_request_t* request) {
    const nx_stm32f1_usb_pma_endpoint_face_t* face = context;
    nx_stm32f1_usb_pma_device_t* state = face->device;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (request == NULL) {
        return NX_ERROR_INVALID;
    }
#if defined(NX_STM32F1_USB_PMA_EP1)
    if (face->number == 1U)
        return nx_stm32f1_usb_ep1_cancel(state, request, face->in);
#endif
    const nx_stm32f1_ep0_t* endpoint = &state->link.endpoint;
    const nx_stm32f1_ep0_loan_t* loan =
        face->in ? &endpoint->in : &endpoint->out;
    return request == loan->request
               ? nx_stm32f1_usb_ep0_cancel(&state->link.endpoint, request)
               : NX_ERROR_STATE;
}

static nx_result_t stall(void* context, bool stalled) {
    const nx_stm32f1_usb_pma_endpoint_face_t* face = context;
    nx_stm32f1_usb_pma_device_t* state = face->device;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!packet_admission(state)) {
        return NX_ERROR_STATE;
    }
#if defined(NX_STM32F1_USB_PMA_EP1)
    if (face->number == 1U) {
        return nx_stm32f1_usb_ep1_stall(state, face->in, stalled);
    }
#endif
    return nx_stm32f1_usb_ep0_stall(&state->link.endpoint, face->in, stalled);
}

const nx_usb_device_ops_t nx_stm32f1_usb_pma_device_ops = {
    .service = service,
    .read_events = read_events,
    .state = snapshot,
    .address = address,
    .stop = stop,
};

const nx_usb_endpoint_ops_t nx_stm32f1_usb_pma_endpoint_ops = {
    .submit = submit,
    .cancel = cancel,
    .stall = stall,
};
