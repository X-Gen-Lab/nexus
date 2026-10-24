/**
 * \file            can_controller.c
 * \brief           Static classic CAN mailboxes, filter ownership and IRQ RX
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "can_controller.h"

/* STM32F407/GD32F470 SDK offsets/masks are asserted by each SoC binding. */
enum {
    CAN_CTL = 0x00U,
    CAN_STATUS = 0x04U,
    CAN_TX_STATUS = 0x08U,
    CAN_FIFO0 = 0x0CU,
    CAN_INTERRUPTS = 0x14U,
    CAN_ERROR = 0x18U,
    CAN_TIMING = 0x1CU,
    CAN_MAILBOX0 = 0x180U,
    CAN_RECEIVE0 = 0x1B0U,
    CAN_FILTER_CONTROL = 0x200U,
    CAN_FILTER_MODE = 0x204U,
    CAN_FILTER_SCALE = 0x20CU,
    CAN_FILTER_FIFO = 0x214U,
    CAN_FILTER_ENABLE = 0x21CU,
    CAN_FILTER0 = 0x240U
};
#define CAN_IRQ_MASK 0x00008F7FU
#ifndef NX_CAN_IRQ_PRIORITY
#define NX_CAN_IRQ_PRIORITY(controller, vector)                                \
    ((uint8_t)(*(volatile uint8_t*)(UINT32_C(0xE000E400) +                     \
                                    (uint16_t)(controller)                     \
                                        ->sources[(vector)]                    \
                                        .irq_number) >>                        \
               (8U - (controller)->irq_policy.priority_bits)))
#endif
#ifndef NX_CAN_IRQ_GROUP
#define NX_CAN_IRQ_GROUP(controller)                                           \
    ((uint8_t)((*(volatile uint32_t*)UINT32_C(0xE000ED0C) >> 8U) & 7U))
#endif
#ifndef NX_CAN_IRQ_CONFIGURE
#define NX_CAN_IRQ_CONFIGURE(controller, vector)                               \
    (*(volatile uint8_t*)(UINT32_C(0xE000E400) + (uint16_t)(controller)        \
                                                     ->sources[(vector)]       \
                                                     .irq_number) =            \
         (uint8_t)((controller)->sources[(vector)].priority                    \
                   << (8U - (controller)->irq_policy.priority_bits)))
#endif
#ifndef NX_CAN_IRQ_SET
/** \brief Write the exact selected NVIC bit and clear its pending fact. */
static void irq_set(nx_can_controller_t* controller, unsigned vector,
                    bool enabled) {
    unsigned irq = (unsigned)controller->sources[vector].irq_number;
    uintptr_t offset = (uintptr_t)(irq / 32U) * 4U;
    *(volatile uint32_t*)(UINT32_C(0xE000E280) + offset) = 1U << (irq % 32U);
    *(volatile uint32_t*)((enabled ? UINT32_C(0xE000E100)
                                   : UINT32_C(0xE000E180)) +
                          offset) = 1U << (irq % 32U);
}
#define NX_CAN_IRQ_SET(controller, vector, enabled)                            \
    irq_set((controller), (vector), (enabled))
#endif
#ifndef NX_CAN_IRQ_ENABLED
#define NX_CAN_IRQ_ENABLED(controller, vector)                                 \
    ((*(volatile uint32_t*)(UINT32_C(0xE000E100) +                             \
                            (uintptr_t)((uint16_t)(controller)                 \
                                            ->sources[(vector)]                \
                                            .irq_number /                      \
                                        32U) *                                 \
                                4U) &                                          \
      (1U << ((uint16_t)(controller)->sources[(vector)].irq_number % 32U))) != \
     0U)
#endif

/** \brief Task controls must not rely on masked or unprivileged progress. */
static bool task_context(void) {
    return !nx_arch_in_isr() && !nx_arch_irq_is_masked() &&
           nx_arch_is_privileged();
}
/** \brief Treat overflow as overlap rather than wrapping a storage interval. */
static bool overlap(const void* left, size_t left_size, const void* right,
                    size_t right_size) {
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    if (left_size > UINTPTR_MAX - a || right_size > UINTPTR_MAX - b) {
        return true;
    }
    return a < b + right_size && b < a + left_size;
}
/** \brief Bounded cold INAK polling never waits for OS/tick progress. */
static bool wait_init(nx_can_controller_t* controller, bool asserted) {
    for (uint32_t i = 0; i < controller->poll_limit; ++i) {
        NX_CAN_POLL(controller->registers);
        if (((NX_CAN_READ(controller->registers, CAN_STATUS) & 1U) != 0U) ==
            asserted) {
            return true;
        }
    }
    return false;
}
/** \brief Encode physical bit timings only after exact checked validation. */
static bool timing_valid(const nx_can_controller_t* controller) {
    const nx_can_timing_t* timing = &controller->timing;
    if (timing->bitrate == 0U || timing->bitrate > 1000000U ||
        timing->prescaler == 0U || timing->prescaler > 1024U ||
        timing->segment1 == 0U || timing->segment1 > 16U ||
        timing->segment2 == 0U || timing->segment2 > 8U || timing->sjw == 0U ||
        timing->sjw > 4U || timing->sjw > timing->segment2 ||
        (controller->mode != NX_CAN_NORMAL &&
         controller->mode != NX_CAN_LOOPBACK &&
         controller->mode != NX_CAN_SILENT_LOOPBACK)) {
        return false;
    }
    uint64_t frequency = (uint64_t)timing->bitrate * timing->prescaler *
                         (1U + timing->segment1 + timing->segment2);
    return frequency == controller->clock_hz;
}
/** \brief Reject malformed/overlapping filter banks before hardware access. */
static bool filters_valid(const nx_can_filter_domain_t* domain) {
    if (domain == NULL || domain->primary == 0U || domain->split > 28U ||
        domain->clock_enable == NULL || domain->primary_clock_mask == 0U ||
        domain->secondary_clock_mask == 0U ||
        (domain->primary_clock_mask & domain->secondary_clock_mask) != 0U ||
        domain->filter_count > 28U ||
        (domain->filter_count != 0U && domain->filters == NULL)) {
        return false;
    }
    uint32_t claimed = 0U;
    for (size_t i = 0; i < domain->filter_count; ++i) {
        const nx_can_filter_t* filter = &domain->filters[i];
        uint32_t limit =
            filter->format == NX_CAN_STANDARD ? 0x7FFU : 0x1FFFFFFFU;
        if (filter->bank >= 28U || filter->controller > 1U ||
            filter->fifo > 1U ||
            (filter->format != NX_CAN_STANDARD &&
             filter->format != NX_CAN_EXTENDED) ||
            filter->id > limit || filter->mask > limit ||
            (filter->controller == 0U ? filter->bank >= domain->split
                                      : filter->bank < domain->split) ||
            (claimed & (1U << filter->bank)) != 0U) {
            return false;
        }
        claimed |= 1U << filter->bank;
    }
    return true;
}
/** \brief Configure the shared filter register bank exactly once, cold only. */
nx_result_t nx_can_filter_domain_initialize(nx_can_filter_domain_t* domain) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!filters_valid(domain)) {
        return NX_ERROR_INVALID;
    }
    if (domain->active_mask != 0U || domain->retained) {
        return NX_ERROR_BUSY;
    }
    if (domain->configured) {
        return NX_SUCCESS;
    }
    *domain->clock_enable |= domain->primary_clock_mask;
    nx_arch_dsb();
    if ((*domain->clock_enable & domain->primary_clock_mask) == 0U) {
        domain->retained = true;
        return NX_ERROR_IO;
    }
    uintptr_t base = domain->primary;
    uint32_t control = (uint32_t)domain->split << 8U;
    NX_CAN_WRITE(base, CAN_FILTER_CONTROL, control | 1U);
    NX_CAN_WRITE(base, CAN_FILTER_ENABLE, 0U);
    NX_CAN_WRITE(base, CAN_FILTER_MODE, 0U);
    uint32_t scale = 0U;
    uint32_t fifo = 0U;
    uint32_t enable = 0U;
    for (size_t i = 0; i < domain->filter_count; ++i) {
        const nx_can_filter_t* filter = &domain->filters[i];
        unsigned shift = filter->format == NX_CAN_STANDARD ? 21U : 3U;
        uint32_t identifier = filter->id << shift;
        uint32_t mask = (filter->mask << shift) | 4U;
        if (filter->format == NX_CAN_EXTENDED) {
            identifier |= 4U;
        }
        if (filter->remote) {
            identifier |= 2U;
        }
        if (filter->match_remote) {
            mask |= 2U;
        }
        unsigned address = CAN_FILTER0 + (unsigned)filter->bank * 8U;
        NX_CAN_WRITE(base, address, identifier);
        NX_CAN_WRITE(base, address + 4U, mask);
        if (NX_CAN_READ(base, address) != identifier ||
            NX_CAN_READ(base, address + 4U) != mask) {
            domain->retained = true;
            return NX_ERROR_IO;
        }
        scale |= 1U << filter->bank;
        fifo |= (uint32_t)filter->fifo << filter->bank;
        enable |= 1U << filter->bank;
    }
    NX_CAN_WRITE(base, CAN_FILTER_SCALE, scale);
    NX_CAN_WRITE(base, CAN_FILTER_FIFO, fifo);
    NX_CAN_WRITE(base, CAN_FILTER_ENABLE, enable);
    NX_CAN_WRITE(base, CAN_FILTER_CONTROL, control);
    nx_arch_dsb();
    if (NX_CAN_READ(base, CAN_FILTER_CONTROL) != control ||
        NX_CAN_READ(base, CAN_FILTER_ENABLE) != enable ||
        NX_CAN_READ(base, CAN_FILTER_SCALE) != scale ||
        NX_CAN_READ(base, CAN_FILTER_FIFO) != fifo ||
        NX_CAN_READ(base, CAN_FILTER_MODE) != 0U) {
        domain->retained = true;
        return NX_ERROR_IO;
    }
    domain->configured = true;
    return NX_SUCCESS;
}
/** \brief Shared clocks cannot be released while either controller is active.
 */
nx_result_t nx_can_filter_domain_stop(nx_can_filter_domain_t* domain) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!filters_valid(domain)) {
        return NX_ERROR_INVALID;
    }
    if (domain->active_mask != 0U) {
        return NX_ERROR_BUSY;
    }
    uint32_t owned = domain->primary_clock_mask | domain->secondary_clock_mask;
    *domain->clock_enable &= ~owned;
    nx_arch_dsb();
    if ((*domain->clock_enable & owned) != 0U) {
        domain->retained = true;
        return NX_ERROR_BUSY;
    }
    domain->configured = false;
    domain->retained = false;
    return NX_SUCCESS;
}
/** \brief Unowned hardware must also drain; empty software slots are
 * insufficient.
 */
static bool unowned_idle(nx_can_controller_t* controller, bool abort) {
    uintptr_t base = controller->registers;
    bool idle = true;
    for (unsigned i = 0; i < 3U; ++i) {
        if (controller->slots[i].request != NULL) {
            continue;
        }
        unsigned address = CAN_MAILBOX0 + i * 16U;
        uint32_t empty = 1U << (26U + i);
        uint32_t complete = 1U << (i * 8U);
        uint32_t status = NX_CAN_READ(base, CAN_TX_STATUS);
        bool busy =
            (status & empty) == 0U || (NX_CAN_READ(base, address) & 1U) != 0U;
        if (busy && abort) {
            NX_CAN_WRITE(base, CAN_TX_STATUS, 1U << (i * 8U + 7U));
            nx_arch_dsb();
            status = NX_CAN_READ(base, CAN_TX_STATUS);
            busy = (status & (empty | complete)) != (empty | complete) ||
                   (NX_CAN_READ(base, address) & 1U) != 0U;
        }
        if (busy) {
            idle = false;
        } else if ((status & complete) != 0U) {
            NX_CAN_WRITE(base, CAN_TX_STATUS, complete);
            nx_arch_dsb();
            idle = (NX_CAN_READ(base, CAN_TX_STATUS) & complete) == 0U && idle;
        }
    }
    return idle;
}
/** \brief INAK excludes arrivals, so exact FIFO count decrease proves drain. */
static bool drain_receive(nx_can_controller_t* controller) {
    uintptr_t base = controller->registers;
    for (unsigned fifo = 0; fifo < 2U; ++fifo) {
        unsigned offset = CAN_FIFO0 + fifo * 4U;
        unsigned count = NX_CAN_READ(base, offset) & 3U;
        for (unsigned i = 0; i < count; ++i) {
            NX_CAN_WRITE(base, offset, 32U);
            nx_arch_dsb();
            uint32_t status = NX_CAN_READ(base, offset);
            if ((status & 32U) != 0U || (status & 3U) != count - i - 1U) {
                return false;
            }
        }
        NX_CAN_WRITE(base, offset, 0x18U);
        if ((NX_CAN_READ(base, offset) & 0x3BU) != 0U) {
            return false;
        }
    }
    return true;
}
/** \brief Cold initialization retains failed hardware responsibility
 * explicitly.
 */
nx_result_t nx_can_controller_initialize(nx_can_controller_t* controller) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (controller == NULL || controller->registers == 0U ||
        controller->snapshot.started || controller->index > 1U ||
        controller->events == NULL || controller->capacity == 0U ||
        controller->capacity > SIZE_MAX / sizeof(nx_can_event_t) ||
        controller->drain_timeout_us == 0U || controller->poll_limit == 0U ||
        !timing_valid(controller) || !filters_valid(controller->domain)) {
        return NX_ERROR_INVALID;
    }
    size_t bytes = controller->capacity * sizeof(nx_can_event_t);
    if (overlap(controller->events, bytes, controller, sizeof(*controller)) ||
        overlap(controller->events, bytes, controller->domain,
                sizeof(*controller->domain)) ||
        overlap(controller->events, bytes, controller->domain->filters,
                controller->domain->filter_count * sizeof(nx_can_filter_t))) {
        return NX_ERROR_INVALID;
    }
    for (unsigned i = 0; i < 4U; ++i) {
        if (nx_irq_wake_validate(NULL, &controller->irq_policy,
                                 &controller->sources[i]) != NX_SUCCESS) {
            return NX_ERROR_INVALID;
        }
    }
    nx_can_filter_domain_t* domain = controller->domain;
    if ((domain->active_mask & (1U << controller->index)) != 0U ||
        domain->retained) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = NX_SUCCESS;
    if (!domain->configured) {
        result = nx_can_filter_domain_initialize(domain);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    uint32_t clocks =
        domain->primary_clock_mask |
        (controller->index == 1U ? domain->secondary_clock_mask : 0U);
    *domain->clock_enable |= clocks;
    nx_arch_dsb();
    if ((*domain->clock_enable & clocks) != clocks) {
        domain->retained = true;
        return NX_ERROR_IO;
    }
    uintptr_t base = controller->registers;
    NX_CAN_WRITE(base, CAN_INTERRUPTS, 0U);
    NX_CAN_WRITE(base, CAN_CTL,
                 1U | 8U | 16U | (controller->fifo_order ? 4U : 0U));
    if (!wait_init(controller, true)) {
        /* An unacknowledged mode write must still be undone and read back. */
        NX_CAN_WRITE(base, CAN_CTL, NX_CAN_READ(base, CAN_CTL) & ~1U);
        if (!wait_init(controller, false)) {
            domain->retained = true;
            domain->active_mask |= (uint8_t)(1U << controller->index);
            controller->snapshot.started = true;
            controller->snapshot.closing = true;
        }
        return NX_ERROR_IO;
    }
    if (!unowned_idle(controller, false) || !drain_receive(controller)) {
        /* Remain in acknowledged init instead of transmitting a stale frame. */
        domain->active_mask |= (uint8_t)(1U << controller->index);
        controller->snapshot.started = true;
        controller->snapshot.closing = true;
        return NX_ERROR_IO;
    }
    const nx_can_timing_t* timing = &controller->timing;
    uint32_t bit_timing = ((uint32_t)timing->prescaler - 1U) |
                          (((uint32_t)timing->segment1 - 1U) << 16U) |
                          (((uint32_t)timing->segment2 - 1U) << 20U) |
                          (((uint32_t)timing->sjw - 1U) << 24U);
    if (controller->mode != NX_CAN_NORMAL) {
        bit_timing |= 1U << 30U;
    }
    if (controller->mode == NX_CAN_SILENT_LOOPBACK) {
        bit_timing |= 1U << 31U;
    }
    NX_CAN_WRITE(base, CAN_TIMING, bit_timing);
    NX_CAN_WRITE(base, CAN_CTL, NX_CAN_READ(base, CAN_CTL) & ~1U);
    if (NX_CAN_READ(base, CAN_TIMING) != bit_timing ||
        !wait_init(controller, false)) {
        NX_CAN_WRITE(base, CAN_CTL, NX_CAN_READ(base, CAN_CTL) | 1U);
        domain->retained = true;
        domain->active_mask |= (uint8_t)(1U << controller->index);
        controller->snapshot.started = true;
        controller->snapshot.closing = true;
        return NX_ERROR_IO;
    }
    controller->snapshot = (nx_can_state_t){.started = true};
    controller->publishers = 0U;
    controller->head = 0U;
    controller->count = 0U;
    controller->lost = 0U;
    controller->loss_flags = 0U;
    for (unsigned i = 0; i < 3U; ++i) {
        controller->slots[i] = (nx_can_mailbox_t){0};
    }
    domain->active_mask |= (uint8_t)(1U << controller->index);
    NX_CAN_WRITE(base, CAN_TX_STATUS, 0x00010101U);
    NX_CAN_WRITE(base, CAN_STATUS, 4U);
    NX_CAN_WRITE(base, CAN_INTERRUPTS, CAN_IRQ_MASK);
    for (unsigned i = 0; i < 4U; ++i) {
        NX_CAN_IRQ_CONFIGURE(controller, i);
        NX_CAN_IRQ_SET(controller, i, true);
    }
    nx_arch_dsb();
    bool enabled = NX_CAN_READ(base, CAN_INTERRUPTS) == CAN_IRQ_MASK;
    for (unsigned i = 0; i < 4U; ++i) {
        enabled = NX_CAN_IRQ_ENABLED(controller, i) &&
                  NX_CAN_IRQ_PRIORITY(controller, i) ==
                      controller->sources[i].priority &&
                  enabled;
    }
    if (!enabled) {
        controller->snapshot.closing = true;
        domain->retained = true;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
/** \brief Preserve bounded loss metadata without overwriting any older event.
 */
static void note_loss(nx_can_controller_t* controller, uint32_t flags) {
    if (controller->lost == 0U) {
        controller->loss_time = nx_time_now_us();
    }
    if (controller->lost == UINT64_MAX) {
        controller->loss_flags |= NX_CAN_LOSS_SATURATED;
    } else {
        ++controller->lost;
    }
    controller->loss_flags |= flags;
}
/** \brief A pending boundary is queued before the next retained frame. */
static void push_loss(nx_can_controller_t* controller) {
    if (controller->lost == 0U || controller->count == controller->capacity) {
        return;
    }
    size_t tail = (controller->head + controller->count) % controller->capacity;
    controller->events[tail] =
        (nx_can_event_t){.timestamp_us = controller->loss_time,
                         .lost = controller->lost,
                         .flags = controller->loss_flags,
                         .kind = NX_CAN_EVENT_LOSS};
    ++controller->count;
    controller->lost = 0U;
    controller->loss_flags = 0U;
}
/** \brief Snapshot error state independently from any receive-ring loan. */
static void errors(nx_can_controller_t* controller) {
    uint32_t error = NX_CAN_READ(controller->registers, CAN_ERROR);
    controller->snapshot.transmit_errors = (uint8_t)(error >> 16U);
    controller->snapshot.receive_errors = (uint8_t)(error >> 24U);
    controller->snapshot.last_error = (uint8_t)((error >> 4U) & 7U);
    controller->snapshot.bus_off |= (error & 4U) != 0U;
    if ((NX_CAN_READ(controller->registers, CAN_STATUS) & 4U) != 0U) {
        NX_CAN_WRITE(controller->registers, CAN_STATUS, 4U);
    }
}
/** \brief Abort retains the descriptor until mailbox completion is proven. */
static void drain(nx_can_controller_t* controller, unsigned mailbox,
                  nx_result_t result, nx_time_us_t now) {
    nx_can_mailbox_t* slot = &controller->slots[mailbox];
    if (slot->terminal == NX_SUCCESS) {
        slot->terminal = result;
        slot->drain_deadline =
            nx_deadline_after(now, controller->drain_timeout_us);
        (void)nx_request_transition(&slot->request->base, NX_REQUEST_DRAINING);
    }
    NX_CAN_WRITE(controller->registers, CAN_TX_STATUS,
                 1U << (mailbox * 8U + 7U));
}
/** \brief RQCP+empty+TXRQ-clear and W1C readback precede request detachment. */
static void mailboxes(nx_can_controller_t* controller, nx_time_us_t now) {
    uintptr_t base = controller->registers;
    for (unsigned i = 0; i < 3U; ++i) {
        nx_can_mailbox_t* slot = &controller->slots[i];
        if (slot->request == NULL) {
            continue;
        }
        uint32_t status = NX_CAN_READ(base, CAN_TX_STATUS);
        uint32_t complete = 1U << (i * 8U);
        uint32_t empty = 1U << (26U + i);
        unsigned address = CAN_MAILBOX0 + i * 16U;
        if ((status & (complete | empty)) == (complete | empty) &&
            (NX_CAN_READ(base, address) & 1U) == 0U) {
            nx_can_tx_request_t* request = slot->request;
            nx_result_t result = (status & (complete << 1U)) != 0U ? NX_SUCCESS
                                 : slot->terminal != NX_SUCCESS ? slot->terminal
                                 : (status & (complete << 2U)) != 0U
                                     ? NX_ERROR_ARBITRATION
                                     : NX_ERROR_IO;
            size_t bytes =
                result == NX_SUCCESS && request->frame.kind == NX_CAN_DATA
                    ? request->frame.dlc
                    : 0U;
            NX_CAN_WRITE(base, CAN_TX_STATUS, complete);
            nx_arch_dsb();
            if ((NX_CAN_READ(base, CAN_TX_STATUS) & complete) != 0U) {
                (void)nx_request_transition(&request->base,
                                            NX_REQUEST_QUARANTINED);
                continue;
            }
            slot->request = NULL;
            --controller->snapshot.pending;
            nx_request_settle(&request->base, result, bytes);
            continue;
        }
        if (slot->terminal == NX_SUCCESS) {
            if (controller->snapshot.bus_off) {
                drain(controller, i, NX_ERROR_IO, now);
            } else if (controller->snapshot.closing) {
                drain(controller, i, NX_ERROR_CANCELLED, now);
            } else if (nx_deadline_expired(slot->request->base.deadline, now)) {
                drain(controller, i, NX_ERROR_TIMEOUT, now);
            }
        } else if (nx_deadline_expired(slot->drain_deadline, now)) {
            (void)nx_request_transition(&slot->request->base,
                                        NX_REQUEST_QUARANTINED);
        }
    }
}
/** \brief Release at most the initial three facts per FIFO, six total. */
static void receive(nx_can_controller_t* controller) {
    uintptr_t base = controller->registers;
    for (unsigned fifo = 0; fifo < 2U; ++fifo) {
        unsigned status_offset = CAN_FIFO0 + fifo * 4U;
        uint32_t status = NX_CAN_READ(base, status_offset);
        bool overrun = (status & 16U) != 0U;
        if (overrun) {
            NX_CAN_WRITE(base, status_offset, 16U);
        }
        unsigned budget = status & 3U;
        for (unsigned i = 0; i < budget; ++i) {
            unsigned address = CAN_RECEIVE0 + fifo * 16U;
            uint32_t identifier = NX_CAN_READ(base, address);
            uint32_t property = NX_CAN_READ(base, address + 4U);
            uint32_t lower = NX_CAN_READ(base, address + 8U);
            uint32_t upper = NX_CAN_READ(base, address + 12U);
            nx_can_event_t event = {.timestamp_us = nx_time_now_us(),
                                    .kind = NX_CAN_EVENT_FRAME,
                                    .fifo = (uint8_t)fifo,
                                    .filter = (uint8_t)(property >> 8U),
                                    .raw_dlc = (uint8_t)(property & 15U)};
            event.frame.format =
                (identifier & 4U) != 0U ? NX_CAN_EXTENDED : NX_CAN_STANDARD;
            event.frame.kind =
                (identifier & 2U) != 0U ? NX_CAN_REMOTE : NX_CAN_DATA;
            event.frame.id = event.frame.format == NX_CAN_EXTENDED
                                 ? identifier >> 3U
                                 : identifier >> 21U;
            event.frame.dlc = event.raw_dlc > 8U ? 8U : event.raw_dlc;
            if (event.raw_dlc > 8U) {
                event.flags |= NX_CAN_DLC_NORMALIZED;
            }
            if (event.frame.kind == NX_CAN_DATA) {
                for (unsigned byte = 0; byte < event.frame.dlc; ++byte) {
                    uint32_t word = byte < 4U ? lower : upper;
                    event.frame.data[byte] =
                        (uint8_t)(word >> ((byte % 4U) * 8U));
                }
            }
            NX_CAN_WRITE(base, status_offset, 32U);
            nx_arch_dsb();
            if ((NX_CAN_READ(base, status_offset) & 32U) != 0U) {
                /* Unacknowledged release cannot make a duplicate frame READY.
                 */
                controller->snapshot.closing = true;
                return;
            }
            push_loss(controller);
            if (controller->count < controller->capacity) {
                size_t tail = (controller->head + controller->count) %
                              controller->capacity;
                controller->events[tail] = event;
                ++controller->count;
            } else {
                note_loss(controller, NX_CAN_LOSS_SOFTWARE);
            }
        }
        if (overrun) {
            /* RFLM keeps these initially queued frames; the dropped newcomer
             * follows them. Later arrival timestamps remain unqualified. */
            note_loss(controller, NX_CAN_LOSS_HARDWARE_LOWER_BOUND);
        }
    }
}
/** \brief IRQ handles constant hardware budgets and signals after unlocking. */
void nx_can_controller_irq(nx_can_controller_t* controller) {
    if (controller == NULL) {
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (!controller->snapshot.started || controller->publishers == UINT32_MAX) {
        nx_arch_irq_restore(mask);
        return;
    }
    ++controller->publishers;
    const nx_irq_wake_t* wake = controller->wake;
    errors(controller);
    mailboxes(controller, now);
    if (!unowned_idle(controller, false)) {
        controller->snapshot.closing = true;
    }
    if (!controller->snapshot.closing) {
        receive(controller);
    }
    nx_arch_irq_restore(mask);
    (void)nx_irq_wake_signal(wake);
    mask = nx_arch_irq_save();
    --controller->publishers;
    nx_arch_irq_restore(mask);
}
/** \brief Reject before admission; only accepted frames touch a TX mailbox. */
static nx_result_t submit(void* context, nx_can_tx_request_t* request) {
    nx_can_controller_t* controller = context;
    if (controller == NULL || request == NULL ||
        !nx_can_frame_valid(&request->frame)) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    size_t ring_size = controller->capacity * sizeof(nx_can_event_t);
    if (overlap(request, sizeof(*request), controller, sizeof(*controller)) ||
        overlap(request, sizeof(*request), controller->events, ring_size) ||
        overlap(request, sizeof(*request), controller->domain,
                sizeof(*controller->domain)) ||
        overlap(request, sizeof(*request), controller->domain->filters,
                controller->domain->filter_count * sizeof(nx_can_filter_t))) {
        return NX_ERROR_INVALID;
    }
    nx_time_us_t now = nx_time_now_us();
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (!controller->snapshot.started || controller->snapshot.closing ||
        controller->snapshot.bus_off) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(request->base.deadline, now)) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_TIMEOUT;
    }
    unsigned mailbox = 3U;
    uint32_t status = NX_CAN_READ(controller->registers, CAN_TX_STATUS);
    for (unsigned i = 0; i < 3U; ++i) {
        if (controller->slots[i].request == NULL &&
            (status & (1U << (26U + i))) != 0U &&
            (status & (1U << (i * 8U))) == 0U) {
            mailbox = i;
            break;
        }
    }
    if (mailbox == 3U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(mask);
        return result;
    }
    controller->slots[mailbox] = (nx_can_mailbox_t){
        .request = request, .drain_deadline = NX_DEADLINE_NEVER};
    ++controller->snapshot.pending;
    const nx_can_frame_t* frame = &request->frame;
    uint32_t identifier = frame->id
                          << (frame->format == NX_CAN_STANDARD ? 21U : 3U);
    identifier |= frame->format == NX_CAN_EXTENDED ? 4U : 0U;
    identifier |= frame->kind == NX_CAN_REMOTE ? 2U : 0U;
    uint32_t lower = 0U;
    uint32_t upper = 0U;
    if (frame->kind == NX_CAN_DATA) {
        for (unsigned i = 0; i < frame->dlc; ++i) {
            if (i < 4U) {
                lower |= (uint32_t)frame->data[i] << (i * 8U);
            } else {
                upper |= (uint32_t)frame->data[i] << ((i - 4U) * 8U);
            }
        }
    }
    uintptr_t base = controller->registers;
    unsigned address = CAN_MAILBOX0 + mailbox * 16U;
    NX_CAN_WRITE(base, address, identifier);
    NX_CAN_WRITE(base, address + 4U, frame->dlc);
    NX_CAN_WRITE(base, address + 8U, lower);
    NX_CAN_WRITE(base, address + 12U, upper);
    nx_arch_dsb();
    NX_CAN_WRITE(base, address, identifier | 1U);
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Find a live descriptor before dereferencing cancellation storage. */
static nx_result_t cancel(void* context, nx_can_tx_request_t* request) {
    nx_can_controller_t* controller = context;
    if (controller == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_time_us_t now = nx_time_now_us();
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    for (unsigned i = 0; i < 3U; ++i) {
        if (controller->slots[i].request == request) {
            mailboxes(controller, now);
            if (controller->slots[i].request != NULL) {
                drain(controller, i, NX_ERROR_CANCELLED, now);
                mailboxes(controller, now);
            }
            nx_arch_irq_restore(mask);
            return NX_SUCCESS;
        }
    }
    nx_arch_irq_restore(mask);
    return NX_ERROR_STATE;
}
/** \brief Poll with the same bounded mechanism as an IRQ without replay. */
static nx_result_t service(void* context) {
    nx_can_controller_t* controller = context;
    if (controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!controller->snapshot.started) {
        return NX_ERROR_STATE;
    }
    nx_can_controller_irq(controller);
    return NX_SUCCESS;
}
/** \brief Copy one value under each short exclusion; drain older facts first.
 */
static nx_result_t read_events(void* context, nx_can_event_t* events,
                               size_t capacity, size_t* count) {
    nx_can_controller_t* controller = context;
    if (controller == NULL || events == NULL || count == NULL ||
        capacity == 0U) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    *count = 0U;
    if (!controller->snapshot.started) {
        return NX_ERROR_STATE;
    }
    while (*count < capacity) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        push_loss(controller);
        if (controller->count == 0U) {
            nx_arch_irq_restore(mask);
            break;
        }
        nx_can_event_t event = controller->events[controller->head];
        controller->head = (controller->head + 1U) % controller->capacity;
        --controller->count;
        nx_arch_irq_restore(mask);
        events[*count] = event;
        ++*count;
    }
    return *count == 0U ? NX_ERROR_EMPTY : NX_SUCCESS;
}
/** \brief Authoritative bus-off state does not depend on event storage. */
static nx_result_t state(void* context, nx_can_state_t* output) {
    nx_can_controller_t* controller = context;
    if (controller == NULL || output == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    *output = controller->snapshot;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Detach all hardware publishers before dropping any wake/ring loan. */
static nx_result_t stop(void* context) {
    nx_can_controller_t* controller = context;
    if (controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!controller->snapshot.started) {
        return NX_SUCCESS;
    }
    nx_time_us_t now = nx_time_now_us();
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    controller->snapshot.closing = true;
    if (controller->publishers != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    mailboxes(controller, now);
    for (unsigned i = 0; i < 3U; ++i) {
        if (controller->slots[i].request != NULL) {
            drain(controller, i, NX_ERROR_CANCELLED, now);
        }
    }
    mailboxes(controller, now);
    if (controller->snapshot.pending != 0U || !unowned_idle(controller, true)) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    uintptr_t base = controller->registers;
    NX_CAN_WRITE(base, CAN_INTERRUPTS, 0U);
    for (unsigned i = 0; i < 4U; ++i) {
        NX_CAN_IRQ_SET(controller, i, false);
    }
    nx_arch_dsb();
    bool detached = NX_CAN_READ(base, CAN_INTERRUPTS) == 0U;
    for (unsigned i = 0; i < 4U; ++i) {
        detached = !NX_CAN_IRQ_ENABLED(controller, i) && detached;
    }
    nx_arch_irq_restore(mask);
    NX_CAN_WRITE(base, CAN_CTL, NX_CAN_READ(base, CAN_CTL) | 1U);
    if (!detached || !wait_init(controller, true) ||
        !drain_receive(controller)) {
        return NX_ERROR_BUSY;
    }
    mask = nx_arch_irq_save();
    if (controller->publishers != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    controller->snapshot.started = false;
    controller->wake = NULL;
    controller->domain->active_mask &= (uint8_t) ~(1U << controller->index);
    if (controller->domain->active_mask == 0U) {
        controller->domain->retained = false;
    }
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Explicit controller-local recovery preserves peer filters/clocks. */
static nx_result_t recover(void* context) {
    nx_can_controller_t* controller = context;
    if (controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (!controller->snapshot.started || controller->snapshot.closing) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_STATE;
    }
    if (controller->snapshot.pending != 0U || controller->publishers != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    controller->snapshot.closing = true;
    nx_arch_irq_restore(mask);
    uintptr_t base = controller->registers;
    NX_CAN_WRITE(base, CAN_CTL, NX_CAN_READ(base, CAN_CTL) | 1U);
    if (!wait_init(controller, true)) {
        controller->snapshot.closing = true;
        return NX_ERROR_IO;
    }
    NX_CAN_WRITE(base, CAN_CTL, NX_CAN_READ(base, CAN_CTL) & ~1U);
    if (!wait_init(controller, false)) {
        controller->snapshot.closing = true;
        return NX_ERROR_IO;
    }
    mask = nx_arch_irq_save();
    errors(controller);
    controller->snapshot.closing = false;
    controller->snapshot.bus_off = (NX_CAN_READ(base, CAN_ERROR) & 4U) != 0U;
    nx_arch_irq_restore(mask);
    return controller->snapshot.bus_off ? NX_ERROR_BUSY : NX_SUCCESS;
}
/** \brief Validate every actual publisher before retaining a new wake target.
 */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_can_controller_t* controller = context;
    if (controller == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!controller->snapshot.started || controller->snapshot.closing) {
        return NX_ERROR_STATE;
    }
    nx_irq_policy_t policy = controller->irq_policy;
    if (wake != NULL && wake->calls_kernel) {
        if (policy.kernel == NX_IRQ_KERNEL_PRIMASK && syscall_ceiling != 0U) {
            return NX_ERROR_INVALID;
        }
        if (policy.kernel == NX_IRQ_KERNEL_BASEPRI &&
            syscall_ceiling > policy.syscall_ceiling) {
            policy.syscall_ceiling = syscall_ceiling;
        }
    }
    for (unsigned i = 0; i < 4U; ++i) {
        nx_irq_source_t source = controller->sources[i];
        source.priority = NX_CAN_IRQ_PRIORITY(controller, i);
        source.priority_group = NX_CAN_IRQ_GROUP(controller);
        nx_result_t result = nx_irq_wake_validate(wake, &policy, &source);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (controller->publishers != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    controller->wake = wake;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief One readonly table for all instances of this classic mechanism. */
const nx_can_ops_t nx_classic_can_ops = {submit, cancel, service, read_events,
                                         state,  stop,   recover, attach_wake};
