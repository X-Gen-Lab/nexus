/**
 * \file            usb_device_controller.c
 * \brief           Bounded full-speed PIO packet ownership and control epochs
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "usb_device_controller.h"

enum {
    AHB = 0x008U,
    USB = 0x00CU,
    RESET = 0x010U,
    INTERRUPT = 0x014U,
    MASK = 0x018U,
    RX_STATUS = 0x020U,
    RX_SIZE = 0x024U,
    TX0_SIZE = 0x028U,
    PHY = 0x038U,
    TX_SIZE = 0x100U,
    CONFIG = 0x800U,
    CONTROL = 0x804U,
    IN_MASK = 0x810U,
    OUT_MASK = 0x814U,
    EP_MASK = 0x81CU,
    TX_EMPTY_MASK = 0x834U,
    IN_EP = 0x900U,
    OUT_EP = 0xB00U,
    FIFO = 0x1000U
};
#define ENABLE  (UINT32_C(1) << 31U)
#define DISABLE (UINT32_C(1) << 30U)
#define SNAK    (UINT32_C(1) << 27U)
#define CNAK    (UINT32_C(1) << 26U)
#define STALL   (UINT32_C(1) << 21U)
#define GLOBAL_MASK                                                            \
    ((1U << 4U) | (1U << 11U) | (1U << 12U) | (1U << 13U) | (1U << 18U) |      \
     (1U << 19U) | (1U << 31U))
#ifndef NX_USB_IRQ_ENABLE
/** \brief Direct NVIC configuration, without a hot access table. */
static void irq_enable(uintptr_t base, int irq, uint8_t priority,
                       uint8_t bits) {
    (void)base;
    unsigned number = (unsigned)irq;
    uintptr_t offset = (uintptr_t)(number / 32U) * 4U;
    *(volatile uint8_t*)(UINT32_C(0xE000E400) + number) =
        (uint8_t)(priority << (8U - bits));
    *(volatile uint32_t*)(UINT32_C(0xE000E280) + offset) = 1U << (number % 32U);
    *(volatile uint32_t*)(UINT32_C(0xE000E100) + offset) = 1U << (number % 32U);
}
#define NX_USB_IRQ_ENABLE(base, irq, priority, bits)                           \
    irq_enable((base), (irq), (priority), (bits))
#endif
#ifndef NX_USB_IRQ_DISABLE
/** \brief Disable and clear only the selected device vector. */
static void irq_disable(uintptr_t base, int irq) {
    (void)base;
    unsigned number = (unsigned)irq;
    uintptr_t offset = (uintptr_t)(number / 32U) * 4U;
    *(volatile uint32_t*)(UINT32_C(0xE000E180) + offset) = 1U << (number % 32U);
    *(volatile uint32_t*)(UINT32_C(0xE000E280) + offset) = 1U << (number % 32U);
}
#define NX_USB_IRQ_DISABLE(base, irq) irq_disable((base), (irq))
#endif
#ifndef NX_USB_IRQ_ENABLED
#define NX_USB_IRQ_ENABLED(base, irq)                                          \
    ((void)(base),                                                             \
     ((*(volatile uint32_t*)(UINT32_C(0xE000E100) +                            \
                             (uintptr_t)((unsigned)(irq) / 32U) * 4U) &        \
       (1U << ((unsigned)(irq) % 32U))) != 0U))
#endif
#ifndef NX_USB_IRQ_PRIORITY
#define NX_USB_IRQ_PRIORITY(base, irq, bits)                                   \
    ((void)(base),                                                             \
     (uint8_t)(*(volatile uint8_t*)(UINT32_C(0xE000E400) +                     \
                                    (uintptr_t)(uint16_t)(irq)) >>             \
               (8U - (bits))))
#endif
#ifndef NX_USB_IRQ_GROUP
#define NX_USB_IRQ_GROUP(base)                                                 \
    ((void)(base),                                                             \
     (uint8_t)((*(volatile uint32_t*)UINT32_C(0xE000ED0C) >> 8U) & 7U))
#endif

/** \brief Task methods cannot rely on masked/unprivileged peripheral progress.
 */
static bool task_context(void) {
    return !nx_arch_in_isr() && !nx_arch_irq_is_masked() &&
           nx_arch_is_privileged();
}
/** \brief Integer half-open spans reject arithmetic wrap conservatively. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t av = (uintptr_t)a, bv = (uintptr_t)b;
    if (an > UINTPTR_MAX - av || bn > UINTPTR_MAX - bv) {
        return true;
    }
    return an != 0U && bn != 0U && av < bv + bn && bv < av + an;
}
/** \brief Exact selected direction, bounded by the actual eight directions. */
static nx_usb_endpoint_t* endpoint(nx_usb_controller_t* c, uint8_t number,
                                   bool in) {
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        nx_usb_endpoint_t* ep = &c->endpoints[i];
        if (ep->number == number && ep->in == in) {
            return ep;
        }
    }
    return NULL;
}
/** \brief Endpoint register stride is asserted against both locked SDKs. */
static uint32_t ep_offset(const nx_usb_endpoint_t* ep) {
    return (ep->in ? IN_EP : OUT_EP) + (uint32_t)ep->number * 32U;
}
/** \brief Defined FS W1C events exclude the read-only IN TXFE status bit. */
static uint32_t ep_flags(const nx_usb_endpoint_t* ep) {
    return NX_USB_READ(ep->controller->registers, ep_offset(ep) + 8U) & 0x5BU;
}
/** \brief Cold polls have a finite work budget and never depend on USB frames.
 */
static bool wait_bits(nx_usb_controller_t* c, uint32_t offset, uint32_t mask,
                      uint32_t value) {
    for (uint32_t i = 0U; i < c->poll_limit; ++i) {
        NX_USB_POLL(c->registers);
        if ((NX_USB_READ(c->registers, offset) & mask) == value) {
            return true;
        }
    }
    return false;
}
/** \brief Cold FIFO flush excludes all packet admission. */
static bool cold_flush(nx_usb_controller_t* c, uint32_t command) {
    NX_USB_WRITE(c->registers, RESET, command);
    if (!wait_bits(c, RESET, command & 0x31U, 0U)) {
        return false;
    }
    /* At <=200MHz, sixteen ordered AHB reads exceed three 48MHz PHY cycles. */
    for (unsigned i = 0U; i < 16U; ++i) {
        (void)NX_USB_READ(c->registers, RESET);
    }
    nx_arch_dsb();
    return true;
}
/** \brief Hot flush is one command/readback, never the cold poll budget. */
static bool hot_flush(nx_usb_controller_t* c, uint32_t command) {
    if ((NX_USB_READ(c->registers, RESET) & 0x31U) != 0U) {
        return false;
    }
    NX_USB_WRITE(c->registers, RESET, command);
    nx_arch_dsb();
    if ((NX_USB_READ(c->registers, RESET) & 0x31U) != 0U) {
        return false;
    }
    for (unsigned i = 0U; i < 16U; ++i) {
        (void)NX_USB_READ(c->registers, RESET);
    }
    return true;
}
/** \brief Validate every immutable plan before retaining clock or touching
 * MMIO. */
static bool plan_valid(nx_usb_controller_t* c) {
    if (c == NULL || c->registers == 0U || c->endpoints == NULL ||
        c->endpoint_count < 2U || c->endpoint_count > 8U || c->events == NULL ||
        c->capacity == 0U || c->capacity > SIZE_MAX / sizeof(*c->events) ||
        c->poll_limit == 0U || c->drain_timeout_us == 0U ||
        c->snapshot.started || c->clock_retained || c->clock.ops == NULL ||
        c->clock.ops->retain == NULL || c->clock.ops->release == NULL ||
        c->clock.ops->validate == NULL || c->clock.context == NULL ||
        c->clock.context_size == 0U || c->snapshot.epoch == UINT64_MAX ||
        overlap(c, sizeof(*c), c->clock.ops, sizeof(*c->clock.ops)) ||
        overlap(c->endpoints, c->endpoint_count * sizeof(*c->endpoints),
                c->clock.ops, sizeof(*c->clock.ops)) ||
        overlap(c->events, c->capacity * sizeof(*c->events), c->clock.ops,
                sizeof(*c->clock.ops)) ||
        overlap(c->clock.context, c->clock.context_size, c->clock.ops,
                sizeof(*c->clock.ops)) ||
        overlap(c, sizeof(*c), c->clock.context, c->clock.context_size) ||
        overlap(c->endpoints, c->endpoint_count * sizeof(*c->endpoints),
                c->clock.context, c->clock.context_size) ||
        overlap(c->events, c->capacity * sizeof(*c->events), c->clock.context,
                c->clock.context_size) ||
        c->rx_fifo_words < 64U || c->rx_fifo_words > 320U ||
        nx_irq_wake_validate(NULL, &c->irq_policy, &c->irq) != NX_SUCCESS ||
        overlap(c, sizeof(*c), c->endpoints,
                c->endpoint_count * sizeof(*c->endpoints)) ||
        overlap(c, sizeof(*c), c->events, c->capacity * sizeof(*c->events)) ||
        overlap(c->endpoints, c->endpoint_count * sizeof(*c->endpoints),
                c->events, c->capacity * sizeof(*c->events))) {
        return false;
    }
    bool in0 = false, out0 = false;
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        nx_usb_endpoint_t* ep = &c->endpoints[i];
        if (ep->number > 3U || ep->request != NULL ||
            (ep->number == 0U
                 ? (ep->bulk || ep->max_packet != 64U)
                 : (!ep->bulk ||
                    (ep->max_packet != 8U && ep->max_packet != 16U &&
                     ep->max_packet != 32U && ep->max_packet != 64U)))) {
            return false;
        }
        if (ep->number == 0U) {
            if (ep->in) {
                in0 = true;
            } else {
                out0 = true;
            }
        }
        if (ep->in &&
            (ep->fifo_words < 16U || ep->fifo_start < c->rx_fifo_words ||
             (uint32_t)ep->fifo_start + ep->fifo_words > 320U)) {
            return false;
        }
        if (!ep->in && (ep->fifo_start != 0U || ep->fifo_words != 0U)) {
            return false;
        }
        for (size_t j = 0U; j < i; ++j) {
            const nx_usb_endpoint_t* other = &c->endpoints[j];
            if (ep->number == other->number && ep->in == other->in) {
                return false;
            }
            if (ep->in && other->in &&
                ep->fifo_start < other->fifo_start + other->fifo_words &&
                other->fifo_start < ep->fifo_start + ep->fifo_words) {
                return false;
            }
        }
    }
    return in0 && out0;
}
/** \brief Keep EP0 SETUP reception armed, without borrowing application
 * buffers. */
static void arm_setup(nx_usb_controller_t* c) {
    nx_usb_endpoint_t* ep = endpoint(c, 0U, false);
    if (ep == NULL || ep->request != NULL || c->snapshot.closing ||
        c->setup_pending || c->reset_pending) {
        return;
    }
    uint32_t offset = ep_offset(ep);
    NX_USB_WRITE(c->registers, offset + 16U, (3U << 29U) | (1U << 19U) | 24U);
    NX_USB_WRITE(c->registers, offset,
                 (NX_USB_READ(c->registers, offset) & ~STALL) | ENABLE | CNAK);
}
/** \brief Cold FS-only PIO start, no DMA or role inference. */
nx_result_t nx_usb_controller_initialize(nx_usb_controller_t* c) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!plan_valid(c)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = c->clock.ops->validate(c->clock.context);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = c->clock.ops->retain(c->clock.context);
    if (result != NX_SUCCESS) {
        return result;
    }
    c->clock_retained = true;
    c->snapshot.started = true;
    c->snapshot.closing = true;
    /* Failed cold start is still an owner: every validated direction must be
     * bound before the first MMIO operation can fail and require stop. */
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        c->endpoints[i].controller = c;
    }
    uintptr_t base = c->registers;
    NX_USB_WRITE(base, AHB, 0U);
    NX_USB_WRITE(base, MASK, 0U);
    NX_USB_IRQ_DISABLE(base, c->irq.irq_number);
    NX_USB_WRITE(base, USB, (1U << 30U) | (1U << 6U) | (6U << 10U));
    if (!wait_bits(c, RESET, ENABLE, ENABLE) || !cold_flush(c, 1U) ||
        !wait_bits(c, INTERRUPT, 1U, 0U) ||
        !cold_flush(c, (0x10U << 6U) | 32U) || !cold_flush(c, 16U)) {
        return NX_ERROR_IO;
    }
    NX_USB_WRITE(base, CONTROL, 2U);
    NX_USB_WRITE(base, PHY,
                 (1U << 16U) | (c->vbus_sensing ? (1U << 19U) : (1U << 21U)));
    NX_USB_WRITE(base, CONFIG, 3U);
    NX_USB_WRITE(base, RX_SIZE, c->rx_fifo_words);
    uint32_t ep_mask = 0U;
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        nx_usb_endpoint_t* ep = &c->endpoints[i];
        ep->controller = c;
        ep->halted = false;
        ep->received = 0U;
        ep->rx_seen = false;
        ep->disable_requested = false;
        uint32_t offset = ep_offset(ep);
        uint32_t control = SNAK | (1U << 15U) |
                           (ep->number == 0U ? 0U
                                             : (uint32_t)ep->max_packet |
                                                   (2U << 18U) | (1U << 28U));
        if (ep->in) {
            control |= (uint32_t)ep->number << 22U;
            uint32_t fifo = ((uint32_t)ep->fifo_words << 16U) | ep->fifo_start;
            uint32_t fifo_offset = ep->number == 0U
                                       ? TX0_SIZE
                                       : TX_SIZE + (uint32_t)ep->number * 4U;
            NX_USB_WRITE(base, fifo_offset, fifo);
            if (NX_USB_READ(base, fifo_offset) != fifo) {
                return NX_ERROR_IO;
            }
        }
        NX_USB_WRITE(base, offset, control);
        NX_USB_WRITE(base, offset + 8U, 0x5BU);
        if (ep_flags(ep) != 0U) {
            return NX_ERROR_IO;
        }
        ep_mask |= 1U << (ep->number + (ep->in ? 0U : 16U));
    }
    NX_USB_WRITE(base, IN_MASK, 0x0BU);
    NX_USB_WRITE(base, OUT_MASK, 0x5BU);
    NX_USB_WRITE(base, EP_MASK, ep_mask);
    NX_USB_WRITE(base, TX_EMPTY_MASK, 0U);
    NX_USB_WRITE(base, INTERRUPT,
                 GLOBAL_MASK & ~((1U << 4U) | (1U << 18U) | (1U << 19U)));
    /* Metadata is complete before any global source can publish facts. */
    c->head = c->count = 0U;
    c->lost = 0U;
    c->publishers = 0U;
    c->snapshot =
        (nx_usb_device_state_t){.started = true, .epoch = c->snapshot.epoch};
    c->address_pending = false;
    c->reset_pending = c->setup_pending = false;
    arm_setup(c);
    NX_USB_WRITE(base, MASK, GLOBAL_MASK);
    NX_USB_IRQ_ENABLE(base, c->irq.irq_number, c->irq.priority,
                      c->irq_policy.priority_bits);
    NX_USB_WRITE(base, AHB, 1U);
    nx_arch_dsb();
    if (NX_USB_READ(base, MASK) != GLOBAL_MASK ||
        NX_USB_READ(base, AHB) != 1U ||
        !NX_USB_IRQ_ENABLED(base, c->irq.irq_number) ||
        NX_USB_IRQ_PRIORITY(base, c->irq.irq_number,
                            c->irq_policy.priority_bits) != c->irq.priority ||
        NX_USB_READ(base, RX_SIZE) != c->rx_fifo_words) {
        c->snapshot.closing = true;
        return NX_ERROR_IO;
    }
    NX_USB_WRITE(base, CONTROL, NX_USB_READ(base, CONTROL) & ~2U);
    if ((NX_USB_READ(base, CONTROL) & 2U) != 0U) {
        c->snapshot.closing = true;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
/** \brief Loss is ordered after existing events, never overwrites them. */
static void event(nx_usb_controller_t* c, nx_usb_event_kind_t kind) {
    nx_time_us_t now = nx_time_now_us();
    if (c->lost != 0U || c->count == c->capacity) {
        if (c->lost != UINT64_MAX) {
            ++c->lost;
        }
        if (c->snapshot.lost_events != UINT64_MAX) {
            ++c->snapshot.lost_events;
        }
        c->loss_time = now;
        return;
    }
    size_t tail = (c->head + c->count) % c->capacity;
    c->events[tail] = (nx_usb_event_t){
        .setup = c->snapshot.setup, .timestamp_us = now, .kind = kind};
    ++c->count;
}
/** \brief Abort records responsibility before touching the hardware source. */
static void begin_drain(nx_usb_endpoint_t* ep, nx_result_t reason) {
    if (ep->request == NULL) {
        return;
    }
    nx_request_state_t state = nx_request_state(&ep->request->base);
    if (state == NX_REQUEST_ACTIVE) {
        ep->terminal = reason;
        ep->drain_deadline = nx_deadline_after(
            nx_time_now_us(), ep->controller->drain_timeout_us);
        (void)nx_request_transition(&ep->request->base, NX_REQUEST_DRAINING);
    } else if (reason == NX_ERROR_STATE) {
        ep->terminal = reason;
    }
}
/** \brief New control/reset epochs never silently revoke earlier endpoint
 * loans. */
static void invalidate(nx_usb_controller_t* c, bool reset) {
    c->address_pending = false;
    c->snapshot.control_pending = false;
    if (c->snapshot.epoch == UINT64_MAX) {
        c->snapshot.closing = true;
    } else {
        ++c->snapshot.epoch;
    }
    c->control_transferred = 0U;
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        if (reset || c->endpoints[i].number == 0U) {
            begin_drain(&c->endpoints[i], NX_ERROR_STATE);
        }
    }
    if (reset) {
        c->reset_pending = true;
    } else {
        c->setup_pending = true;
    }
}
/** \brief One SETUP is exactly eight little-endian bytes, not a class callback.
 */
static void setup_packet(nx_usb_controller_t* c, const uint8_t* b) {
    invalidate(c, false);
    c->snapshot.setup = (nx_usb_setup_t){
        .request_type = b[0],
        .request = b[1],
        .value = (uint16_t)((uint16_t)b[2] | ((uint16_t)b[3] << 8U)),
        .index = (uint16_t)((uint16_t)b[4] | ((uint16_t)b[5] << 8U)),
        .length = (uint16_t)((uint16_t)b[6] | ((uint16_t)b[7] << 8U)),
        .epoch = c->snapshot.epoch};
    c->control_stage =
        c->snapshot.setup.length == 0U ? NX_USB_STATUS : NX_USB_DATA;
    c->snapshot.control_pending = true;
    event(c, NX_USB_EVENT_SETUP);
}
/** \brief RX status/data work is limited to eight headers and 128 payload
 * words. */
static void receive(nx_usb_controller_t* c) {
    uintptr_t base = c->registers;
    size_t words_left = 128U;
    for (unsigned header = 0U; header < 8U; ++header) {
        if ((NX_USB_READ(base, INTERRUPT) & 16U) == 0U) {
            break;
        }
        uint32_t status = NX_USB_READ(base, RX_STATUS);
        uint8_t number = (uint8_t)(status & 15U);
        size_t bytes = (status >> 4U) & 0x7FFU;
        unsigned type = (status >> 17U) & 15U;
        if (bytes > 64U || (bytes + 3U) / 4U > words_left) {
            c->snapshot.closing = true;
            for (size_t i = 0U; i < c->endpoint_count; ++i) {
                begin_drain(&c->endpoints[i], NX_ERROR_IO);
            }
            return;
        }
        nx_usb_endpoint_t* ep = endpoint(c, number, false);
        bool copy = !c->snapshot.closing && !c->reset_pending && type == 2U &&
                    ep != NULL && ep->request != NULL &&
                    nx_request_state(&ep->request->base) == NX_REQUEST_ACTIVE;
        uint8_t setup[8] = {0};
        size_t offset = 0U;
        while (offset < bytes) {
            uint32_t word = NX_USB_READ(base, FIFO);
            --words_left;
            for (unsigned lane = 0U; lane < 4U && offset < bytes;
                 ++lane, ++offset) {
                uint8_t value = (uint8_t)(word >> (lane * 8U));
                if (type == 6U && number == 0U && bytes == 8U) {
                    setup[offset] = value;
                }
                if (copy && ep->received < ep->request->length) {
                    ep->request->out[ep->received] = value;
                }
                if (copy) {
                    ++ep->received;
                }
            }
        }
        if (type == 6U) {
            /* A reset snapshot gives no ordering proof for queued old SETUP.
             * Drain it without admission; late SETUP also cannot reopen stop.
             */
            if (!c->snapshot.closing && !c->reset_pending) {
                if (number == 0U && bytes == 8U) {
                    setup_packet(c, setup);
                } else {
                    c->snapshot.closing = true;
                }
            }
        } else if (copy) {
            ep->rx_seen = true;
            if (ep->received > ep->request->length) {
                begin_drain(ep, NX_ERROR_OVERFLOW);
            }
        }
    }
}
/** \brief Drain has no hot busy-wait; each invocation only observes/requests.
 */
static bool drain(nx_usb_endpoint_t* ep) {
    nx_usb_controller_t* c = ep->controller;
    uintptr_t base = c->registers;
    uint32_t offset = ep_offset(ep);
    if (!ep->in) {
        NX_USB_WRITE(base, CONTROL, NX_USB_READ(base, CONTROL) | (1U << 9U));
        if ((NX_USB_READ(base, CONTROL) & 8U) == 0U) {
            return false;
        }
    }
    uint32_t control = NX_USB_READ(base, offset);
    if ((control & ENABLE) != 0U) {
        /* Control OUT0 has no EPD command.  Individual cancellation must retain
         * the PIO loan until completion; full stop has a separate source proof.
         */
        if (!ep->in && ep->number == 0U) {
            return false;
        }
        if (ep->in) {
            NX_USB_WRITE(base, offset, control | SNAK);
            nx_arch_dsb();
            control = NX_USB_READ(base, offset);
            if ((control & (1U << 17U)) == 0U) {
                return false;
            }
        }
        NX_USB_WRITE(base, offset, control | DISABLE | SNAK);
        ep->disable_requested = true;
        nx_arch_dsb();
    }
    uint32_t flags = ep_flags(ep);
    if ((NX_USB_READ(base, offset) & ENABLE) != 0U ||
        (ep->disable_requested && (flags & 2U) == 0U)) {
        return false;
    }
    if (ep->in) {
        uint32_t reset = NX_USB_READ(base, RESET);
        if ((reset & 32U) != 0U) {
            return false;
        }
        NX_USB_WRITE(base, RESET, ((uint32_t)ep->number << 6U) | 32U);
        nx_arch_dsb();
        if ((NX_USB_READ(base, RESET) & 32U) != 0U) {
            return false;
        }
        for (unsigned i = 0U; i < 16U; ++i) {
            (void)NX_USB_READ(base, RESET);
        }
    } else if ((NX_USB_READ(base, INTERRUPT) & 16U) != 0U) {
        return false;
    }
    NX_USB_WRITE(base, offset + 8U, flags);
    nx_arch_dsb();
    bool clear = ep_flags(ep) == 0U;
    if (clear) {
        ep->disable_requested = false;
    }
    return clear;
}
/** \brief Advance EP0 only after real packet completion in the current epoch.
 */
static void control_complete(nx_usb_endpoint_t* ep, nx_result_t result,
                             size_t count) {
    nx_usb_controller_t* c = ep->controller;
    nx_usb_request_t* r = ep->request;
    if (ep->number != 0U || result != NX_SUCCESS ||
        r->control_epoch != c->snapshot.epoch || !c->snapshot.control_pending) {
        return;
    }
    if (r->stage == NX_USB_DATA) {
        c->control_transferred += count;
        if (count < ep->max_packet ||
            c->control_transferred >= c->snapshot.setup.length) {
            c->control_stage = NX_USB_STATUS;
        }
    } else {
        if (ep->in && c->address_pending) {
            uint32_t value = (NX_USB_READ(c->registers, CONFIG) & ~0x7F0U) |
                             ((uint32_t)c->pending_address << 4U);
            NX_USB_WRITE(c->registers, CONFIG, value);
            if (NX_USB_READ(c->registers, CONFIG) != value) {
                c->snapshot.closing = true;
            } else {
                c->snapshot.address = c->pending_address;
            }
        }
        c->address_pending = false;
        c->snapshot.control_pending = false;
    }
}
/** \brief References detach before SETTLED, which is the final request access.
 */
static void finish(nx_usb_endpoint_t* ep, nx_result_t result, size_t count) {
    nx_usb_request_t* request = ep->request;
    control_complete(ep, result, count);
    ep->request = NULL;
    ep->disable_requested = false;
    nx_request_settle(&request->base, result, count);
}
/** \brief Completion, error, cancel and deadline all require actual detach
 * proof. */
static void progress(nx_usb_controller_t* c) {
    uintptr_t base = c->registers;
    nx_time_us_t now = nx_time_now_us();
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        nx_usb_endpoint_t* ep = &c->endpoints[i];
        if (ep->request == NULL) {
            uint32_t idle = ep_offset(ep) + 8U;
            uint32_t observed = ep_flags(ep);
            if (observed != 0U) {
                NX_USB_WRITE(base, idle, observed);
                if (ep_flags(ep) != 0U) {
                    c->snapshot.closing = true;
                }
            }
            continue;
        }
        uint32_t offset = ep_offset(ep);
        uint32_t flags = ep_flags(ep);
        nx_request_state_t state = nx_request_state(&ep->request->base);
        uint32_t hints = flags & (ep->in ? 0x50U : 0x48U);
        if (hints != 0U) {
            NX_USB_WRITE(base, offset + 8U, hints);
        }
        if (state == NX_REQUEST_ACTIVE && (flags & (ep->in ? 8U : 16U)) != 0U) {
            begin_drain(ep, ep->in ? NX_ERROR_IO : NX_ERROR_OVERFLOW);
            state = NX_REQUEST_DRAINING;
        }
        bool complete =
            (flags & 1U) != 0U && (NX_USB_READ(base, offset) & ENABLE) == 0U &&
            (ep->in ? NX_USB_READ(base, offset + 24U) == ep->fifo_words
                    : ep->rx_seen);
        if (state == NX_REQUEST_ACTIVE && complete) {
            NX_USB_WRITE(base, offset + 8U, flags);
            nx_arch_dsb();
            if (ep_flags(ep) == 0U) {
                finish(ep, NX_SUCCESS,
                       ep->in ? ep->request->length : ep->received);
                continue;
            }
            begin_drain(ep, NX_ERROR_IO);
            state = NX_REQUEST_DRAINING;
        }
        if (state == NX_REQUEST_ACTIVE && now >= ep->request->base.deadline) {
            begin_drain(ep, NX_ERROR_TIMEOUT);
            state = NX_REQUEST_DRAINING;
        }
        if (state != NX_REQUEST_ACTIVE) {
            if (drain(ep)) {
                nx_result_t result = ep->terminal == NX_ERROR_STATE
                                         ? NX_ERROR_CANCELLED
                                         : ep->terminal;
                if (complete && ep->terminal == NX_ERROR_CANCELLED) {
                    result = NX_SUCCESS;
                }
                size_t count =
                    result == NX_SUCCESS
                        ? (ep->in ? ep->request->length : ep->received)
                        : (ep->in ? 0U
                                  : (ep->received > ep->request->length
                                         ? ep->request->length
                                         : ep->received));
                finish(ep, result, count);
            } else if (now >= ep->drain_deadline) {
                (void)nx_request_transition(&ep->request->base,
                                            NX_REQUEST_QUARANTINED);
            }
        }
    }
    if (c->setup_pending && endpoint(c, 0U, true)->request == NULL &&
        endpoint(c, 0U, false)->request == NULL) {
        c->setup_pending = false;
        for (unsigned i = 0U; i < 2U; ++i) {
            nx_usb_endpoint_t* ep = endpoint(c, 0U, i == 0U);
            uint32_t offset = ep_offset(ep);
            NX_USB_WRITE(base, offset, NX_USB_READ(base, offset) & ~STALL);
            ep->halted = false;
        }
    }
    bool pending = false;
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        pending = pending || c->endpoints[i].request != NULL;
    }
    if (c->reset_pending && !pending) {
        if (hot_flush(c, (0x10U << 6U) | 32U) && hot_flush(c, 16U)) {
            c->reset_pending = false;
            uint32_t config = NX_USB_READ(base, CONFIG) & ~0x7F0U;
            NX_USB_WRITE(base, CONFIG, config);
            c->snapshot.address = 0U;
        }
    }
    bool draining_out = false;
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        nx_usb_endpoint_t* ep = &c->endpoints[i];
        draining_out = draining_out || (!ep->in && ep->request != NULL &&
                                        nx_request_state(&ep->request->base) !=
                                            NX_REQUEST_ACTIVE);
    }
    if (!draining_out && !c->snapshot.closing) {
        NX_USB_WRITE(base, CONTROL, NX_USB_READ(base, CONTROL) | (1U << 10U));
    }
    arm_setup(c);
}
/** \brief Capture publishers before notification and join them before reclaim.
 */
void nx_usb_controller_irq(nx_usb_controller_t* c) {
    if (c == NULL) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!c->snapshot.started || c->publishers == UINT32_MAX) {
        nx_arch_irq_restore(saved);
        return;
    }
    ++c->publishers;
    uint32_t flags = NX_USB_READ(c->registers, INTERRUPT);
    if ((flags & (1U << 12U)) != 0U) {
        invalidate(c, true);
        event(c, NX_USB_EVENT_RESET);
        NX_USB_WRITE(c->registers, INTERRUPT, 1U << 12U);
    }
    if ((flags & (1U << 11U)) != 0U) {
        c->snapshot.suspended = true;
        event(c, NX_USB_EVENT_SUSPEND);
        NX_USB_WRITE(c->registers, INTERRUPT, 1U << 11U);
    }
    if ((flags & ENABLE) != 0U) {
        c->snapshot.suspended = false;
        event(c, NX_USB_EVENT_RESUME);
        NX_USB_WRITE(c->registers, INTERRUPT, ENABLE);
    }
    if ((flags & (1U << 13U)) != 0U) {
        if ((NX_USB_READ(c->registers, 0x808U) & 6U) != 6U) {
            c->snapshot.closing = true;
        }
        NX_USB_WRITE(c->registers, INTERRUPT, 1U << 13U);
    }
    receive(c);
    progress(c);
    const nx_irq_wake_t* wake = c->wake;
    nx_arch_irq_restore(saved);
    if (wake != NULL) {
        (void)wake->notify(wake->context);
    }
    saved = nx_arch_irq_save();
    --c->publishers;
    nx_arch_irq_restore(saved);
}
/** \brief Protect private state, immutable targets and every active borrow. */
static bool storage_valid(const nx_usb_controller_t* c, const void* bytes,
                          size_t length, bool protect_wake) {
    if (overlap(bytes, length, c, sizeof(*c)) ||
        overlap(bytes, length, c->endpoints,
                c->endpoint_count * sizeof(*c->endpoints)) ||
        overlap(bytes, length, c->events, c->capacity * sizeof(*c->events)) ||
        overlap(bytes, length, c->clock.context, c->clock.context_size) ||
        overlap(bytes, length, c->clock.ops, sizeof(*c->clock.ops)) ||
        (protect_wake && c->wake != NULL &&
         overlap(bytes, length, c->wake, sizeof(*c->wake)))) {
        return false;
    }
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        const nx_usb_endpoint_t* ep = &c->endpoints[i];
        if (ep->request != NULL) {
            const nx_usb_request_t* request = ep->request;
            const void* payload =
                ep->in ? (const void*)request->in : (const void*)request->out;
            if (overlap(bytes, length, request, sizeof(*request)) ||
                overlap(bytes, length, payload, request->length)) {
                return false;
            }
        }
    }
    return true;
}
/** \brief Reject all request/buffer aliases to private or other borrowed
 * storage. */
static bool request_valid(nx_usb_endpoint_t* ep, nx_usb_request_t* r) {
    nx_usb_controller_t* c = ep->controller;
    if (r == NULL || r->length > ep->max_packet ||
        (r->stage != NX_USB_DATA && r->stage != NX_USB_STATUS) ||
        (r->length != 0U && (ep->in ? r->in == NULL || r->out != NULL
                                    : r->out == NULL || r->in != NULL)) ||
        (r->stage == NX_USB_STATUS && r->length != 0U)) {
        return false;
    }
    const void* bytes = ep->in ? (const void*)r->in : (const void*)r->out;
    if (overlap(r, sizeof(*r), bytes, r->length) ||
        !storage_valid(c, r, sizeof(*r), true) ||
        !storage_valid(c, bytes, r->length, true)) {
        return false;
    }
    if (ep->number != 0U) {
        return r->control_epoch == 0U && r->stage == NX_USB_DATA;
    }
    if (!c->snapshot.control_pending || c->setup_pending || c->reset_pending ||
        r->control_epoch != c->snapshot.epoch || r->stage != c->control_stage) {
        return false;
    }
    bool data_in = (c->snapshot.setup.request_type & 0x80U) != 0U;
    bool wanted_in = r->stage == NX_USB_DATA
                         ? data_in
                         : (c->snapshot.setup.length == 0U || !data_in);
    return wanted_in == ep->in &&
           (r->stage != NX_USB_DATA ||
            r->length <=
                (size_t)c->snapshot.setup.length - c->control_transferred);
}
/** \brief One packet admission validates all effects before ACTIVE publication.
 */
static nx_result_t submit(void* context, nx_usb_request_t* r) {
    nx_usb_endpoint_t* ep = context;
    if (ep == NULL || ep->controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_usb_controller_t* c = ep->controller;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    uintptr_t base = c->registers;
    uint32_t offset = ep_offset(ep);
    if (!c->snapshot.started || c->snapshot.closing || ep->halted ||
        c->reset_pending) {
        result = NX_ERROR_STATE;
    } else if (!request_valid(ep, r)) {
        result = NX_ERROR_INVALID;
    } else if (ep->request != NULL ||
               (NX_USB_READ(base, offset + 8U) & 0x1BU) != 0U ||
               ((ep->in || ep->number != 0U) &&
                (NX_USB_READ(base, offset) & ENABLE) != 0U) ||
               (!ep->in && (NX_USB_READ(base, CONTROL) & 8U) != 0U)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    } else if (nx_time_now_us() >= r->base.deadline) {
        result = NX_ERROR_TIMEOUT;
    } else if (ep->in &&
               NX_USB_READ(base, offset + 24U) < (r->length + 3U) / 4U) {
        result = NX_ERROR_BUSY;
    } else {
        result = nx_request_admit(&r->base, NX_REQUEST_ACTIVE);
    }
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return result;
    }
    ep->request = r;
    ep->received = 0U;
    ep->rx_seen = false;
    ep->disable_requested = false;
    ep->terminal = NX_SUCCESS;
    uint32_t size =
        (1U << 19U) | (uint32_t)(ep->in ? r->length : ep->max_packet);
    if (!ep->in && ep->number == 0U) {
        size |= 3U << 29U;
    }
    NX_USB_WRITE(base, offset + 16U, size);
    uint32_t control = NX_USB_READ(base, offset) &
                       ~(DISABLE | SNAK | (1U << 29U) | (1U << 28U));
    /* EP0 DATA PID is managed by SETUP/core. SD0/SD1 are reserved for EP0. */
    NX_USB_WRITE(base, offset, control | ENABLE | CNAK);
    if (ep->in) {
        for (size_t index = 0U; index < r->length; index += 4U) {
            uint32_t word = 0U;
            for (unsigned lane = 0U; lane < 4U && index + lane < r->length;
                 ++lane) {
                word |= (uint32_t)r->in[index + lane] << (lane * 8U);
            }
            NX_USB_WRITE(base, FIFO + (uint32_t)ep->number * 0x1000U, word);
        }
    }
    nx_arch_dsb();
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief Cancellation requests progress; it never clears a live loan early. */
static nx_result_t cancel(void* context, nx_usb_request_t* r) {
    nx_usb_endpoint_t* ep = context;
    if (ep == NULL || r == NULL || ep->controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (ep->request != r) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    begin_drain(ep, NX_ERROR_CANCELLED);
    nx_arch_irq_restore(saved);
    nx_usb_controller_irq(ep->controller);
    return NX_SUCCESS;
}
/** \brief Stall is a proved idle endpoint operation, never hidden cancellation.
 */
static nx_result_t stall(void* context, bool value) {
    nx_usb_endpoint_t* ep = context;
    if (ep == NULL || ep->controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_usb_controller_t* c = ep->controller;
    if (!c->snapshot.started || c->snapshot.closing) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (ep->request != NULL) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    uint32_t offset = ep_offset(ep);
    uint32_t control = NX_USB_READ(c->registers, offset);
    if ((ep->in || ep->number != 0U) && (control & ENABLE) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    control = value ? control | STALL : control & ~STALL;
    if (!value && ep->bulk) {
        control |= 1U << 28U;
    }
    NX_USB_WRITE(c->registers, offset, control);
    uint32_t readback = NX_USB_READ(c->registers, offset);
    bool accepted = ((readback & STALL) != 0U) == value &&
                    (value || !ep->bulk || (readback & (1U << 16U)) == 0U);
    if (accepted) {
        ep->halted = value;
    }
    nx_arch_irq_restore(saved);
    return accepted ? NX_SUCCESS : NX_ERROR_IO;
}
/** \brief Independent monotonic timeout service, including after
 * unplug/suspend. */
static nx_result_t service(void* context) {
    nx_usb_controller_t* c = context;
    if (c == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!c->snapshot.started) {
        return NX_ERROR_STATE;
    }
    nx_usb_controller_irq(c);
    return NX_SUCCESS;
}
/** \brief Copy bounded value events; successful stop excludes ring access. */
static nx_result_t read_events(void* context, nx_usb_event_t* output,
                               size_t capacity, size_t* count) {
    nx_usb_controller_t* c = context;
    if (c == NULL || output == NULL || count == NULL || capacity == 0U) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (capacity > SIZE_MAX / sizeof(*output) ||
        !storage_valid(c, output, capacity * sizeof(*output), true) ||
        !storage_valid(c, count, sizeof(*count), true) ||
        overlap(output, capacity * sizeof(*output), count, sizeof(*count))) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_INVALID;
    }
    if (!c->snapshot.started) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    *count = 0U;
    while (*count < capacity) {
        if (c->count != 0U) {
            output[*count] = c->events[c->head];
            c->head = (c->head + 1U) % c->capacity;
            --c->count;
        } else if (c->lost != 0U) {
            output[*count] = (nx_usb_event_t){.kind = NX_USB_EVENT_LOSS,
                                              .timestamp_us = c->loss_time,
                                              .lost = c->lost};
            c->lost = 0U;
        } else {
            break;
        }
        ++*count;
    }
    nx_arch_irq_restore(saved);
    return *count == 0U ? NX_ERROR_EMPTY : NX_SUCCESS;
}
/** \brief Latest SETUP/error state remains authoritative when event storage
 * fills. */
static nx_result_t state(void* context, nx_usb_device_state_t* output) {
    nx_usb_controller_t* c = context;
    if (c == NULL || output == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!storage_valid(c, output, sizeof(*output), true)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_INVALID;
    }
    *output = c->snapshot;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief SET_ADDRESS is staged, never applied before the status-IN ACK. */
static nx_result_t address(void* context, uint64_t epoch, uint8_t value) {
    nx_usb_controller_t* c = context;
    if (c == NULL || value > 127U) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    const nx_usb_setup_t* setup = &c->snapshot.setup;
    bool valid = c->snapshot.started && !c->snapshot.closing &&
                 c->snapshot.control_pending && epoch == c->snapshot.epoch &&
                 setup->request_type == 0U && setup->request == 5U &&
                 setup->value == value && setup->index == 0U &&
                 setup->length == 0U;
    if (valid) {
        c->pending_address = value;
        c->address_pending = true;
    }
    nx_arch_irq_restore(saved);
    return valid ? NX_SUCCESS : NX_ERROR_STATE;
}
/** \brief No sink replacement until every publisher has completed final access.
 */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_usb_controller_t* c = context;
    if (c == NULL || (wake != NULL && wake->notify == NULL)) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    nx_irq_source_t actual = c->irq;
    actual.priority = NX_USB_IRQ_PRIORITY(c->registers, actual.irq_number,
                                          c->irq_policy.priority_bits);
    actual.priority_group = NX_USB_IRQ_GROUP(c->registers);
    if (!c->snapshot.started || c->snapshot.closing) {
        result = NX_ERROR_STATE;
    } else if (c->publishers != 0U) {
        result = NX_ERROR_BUSY;
    } else if (wake != NULL && !storage_valid(c, wake, sizeof(*wake), false)) {
        result = NX_ERROR_INVALID;
    } else if (wake != NULL &&
               (nx_irq_wake_validate(wake, &c->irq_policy, &actual) !=
                    NX_SUCCESS ||
                (wake->calls_kernel && actual.priority < ceiling))) {
        result = NX_ERROR_PERMISSION;
    } else {
        c->wake = wake;
    }
    nx_arch_irq_restore(saved);
    return result;
}
/** \brief Stop retains all loans until endpoint/FIFO/NVIC/clock proof is
 * complete. */
static nx_result_t stop(void* context) {
    nx_usb_controller_t* c = context;
    if (c == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!c->snapshot.started) {
        nx_arch_irq_restore(saved);
        return NX_SUCCESS;
    }
    c->snapshot.closing = true;
    if (c->publishers != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        begin_drain(&c->endpoints[i], NX_ERROR_CANCELLED);
    }
    NX_USB_WRITE(c->registers, CONTROL,
                 NX_USB_READ(c->registers, CONTROL) | 2U | (1U << 9U));
    nx_arch_dsb();
    if ((NX_USB_READ(c->registers, CONTROL) & 10U) != 10U ||
        (NX_USB_READ(c->registers, AHB) & (1U << 5U)) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_restore(saved);
    nx_usb_controller_irq(c);
    saved = nx_arch_irq_save();
    for (size_t i = 0U; i < c->endpoint_count; ++i) {
        nx_usb_endpoint_t* ep = &c->endpoints[i];
        if (!ep->in && ep->number == 0U) {
            /* SETUP may bypass NAK.  PIO OUT0 is only withdrawn globally after
             * disconnect, FIFO drain, source disable and publisher join below.
             */
            if ((NX_USB_READ(c->registers, INTERRUPT) & 16U) != 0U) {
                nx_arch_irq_restore(saved);
                return NX_ERROR_BUSY;
            }
        } else if (ep->request != NULL || !drain(ep)) {
            nx_arch_irq_restore(saved);
            return NX_ERROR_BUSY;
        }
    }
    NX_USB_WRITE(c->registers, MASK, 0U);
    NX_USB_WRITE(c->registers, AHB, 0U);
    NX_USB_WRITE(c->registers, EP_MASK, 0U);
    NX_USB_IRQ_DISABLE(c->registers, c->irq.irq_number);
    nx_arch_dsb();
    bool detached = NX_USB_READ(c->registers, MASK) == 0U &&
                    NX_USB_READ(c->registers, AHB) == 0U &&
                    NX_USB_READ(c->registers, EP_MASK) == 0U &&
                    !NX_USB_IRQ_ENABLED(c->registers, c->irq.irq_number) &&
                    c->publishers == 0U &&
                    (NX_USB_READ(c->registers, CONTROL) & 10U) == 10U &&
                    (NX_USB_READ(c->registers, INTERRUPT) & 16U) == 0U;
    nx_arch_irq_restore(saved);
    if (!detached || !cold_flush(c, (0x10U << 6U) | 32U) ||
        !cold_flush(c, 16U)) {
        return NX_ERROR_BUSY;
    }
    saved = nx_arch_irq_save();
    if ((NX_USB_READ(c->registers, CONTROL) & 10U) != 10U ||
        NX_USB_READ(c->registers, AHB) != 0U ||
        NX_USB_READ(c->registers, MASK) != 0U ||
        NX_USB_READ(c->registers, EP_MASK) != 0U ||
        (NX_USB_READ(c->registers, INTERRUPT) & 16U) != 0U ||
        NX_USB_IRQ_ENABLED(c->registers, c->irq.irq_number) ||
        c->publishers != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    nx_usb_endpoint_t* out0 = endpoint(c, 0U, false);
    if (out0->request != NULL) {
        nx_result_t terminal = out0->terminal == NX_ERROR_STATE
                                   ? NX_ERROR_CANCELLED
                                   : out0->terminal;
        size_t received = out0->received > out0->request->length
                              ? out0->request->length
                              : out0->received;
        finish(out0, terminal, received);
    }
    nx_arch_irq_restore(saved);
    nx_result_t result = c->clock_retained
                             ? c->clock.ops->release(c->clock.context)
                             : NX_SUCCESS;
    if (result != NX_SUCCESS) {
        return result;
    }
    saved = nx_arch_irq_save();
    c->clock_retained = false;
    c->snapshot.started = false;
    c->wake = NULL;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
const nx_usb_endpoint_ops_t nx_usb_fs_endpoint_ops = {
    .submit = submit, .cancel = cancel, .stall = stall};
const nx_usb_device_ops_t nx_usb_fs_device_ops = {.service = service,
                                                  .read_events = read_events,
                                                  .state = state,
                                                  .address = address,
                                                  .attach_wake = attach_wake,
                                                  .stop = stop};
