/**
 * \file            i2c_v1_async.c
 * \brief           Bounded V1 message IRQ/DMA, receive windows and wire drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_v1_async.h"
#include "nexus/arch/arch.h"

/* The two SDK wrappers assert every layout and mask before using this
 * algorithm. There is no per-byte virtual register access in firmware. */
_Static_assert(NX_I2C_MAX_MESSAGES <= UINT8_MAX &&
                   NX_I2C_MAX_MESSAGE_BYTES <= UINT16_MAX &&
                   NX_I2C_MAX_MESSAGES * NX_I2C_MAX_MESSAGE_BYTES <= UINT16_MAX,
               "bounded I2C controller indices fit their exact storage");

#define CTL0         0U
#define CTL1         1U
#define ADDR0        2U
#define DATA         4U
#define STAT0        5U
#define STAT1        6U
#define CKCFG        7U
#define RT           8U
#define ENABLE       (UINT32_C(1) << 0U)
#define START        (UINT32_C(1) << 8U)
#define STOP         (UINT32_C(1) << 9U)
#define ACK          (UINT32_C(1) << 10U)
#define POS          (UINT32_C(1) << 11U)
#define ERROR_IRQ    (UINT32_C(1) << 8U)
#define EVENT_IRQ    (UINT32_C(1) << 9U)
#define BUFFER_IRQ   (UINT32_C(1) << 10U)
#define DMA_ON       (UINT32_C(1) << 11U)
#define DMA_LAST     (UINT32_C(1) << 12U)
#define SB           (UINT32_C(1) << 0U)
#define ADDRESS      (UINT32_C(1) << 1U)
#define BTF          (UINT32_C(1) << 2U)
#define RXNE         (UINT32_C(1) << 6U)
#define TXE          (UINT32_C(1) << 7U)
#define BUS_ERROR    (UINT32_C(1) << 8U)
#define ARBITRATION  (UINT32_C(1) << 9U)
#define NACK         (UINT32_C(1) << 10U)
#define OVERRUN      (UINT32_C(1) << 11U)
#define MASTER       (UINT32_C(1) << 0U)
#define BUS_BUSY     (UINT32_C(1) << 1U)
#define DMA_IRQS     UINT32_C(0x1E)
#define DMA_ENABLE   UINT32_C(1)
#define DMA_FLAGS    UINT32_C(0x3D)
#define DMA_ERRORS   UINT32_C(0x0D)
#define DMA_COMPLETE UINT32_C(0x20)

#ifndef NX_I2C_V1_READ_DATA
#define NX_I2C_V1_READ_DATA(state) ((uint8_t)(state)->registers[DATA])
#endif
#ifndef NX_I2C_V1_WRITE_DATA
#define NX_I2C_V1_WRITE_DATA(state, value) ((state)->registers[DATA] = (value))
#endif
#ifndef NX_I2C_V1_ADDRESS_CLEARED
#define NX_I2C_V1_ADDRESS_CLEARED(state) ((void)(state))
#endif
#ifndef NX_I2C_V1_DMA_DISABLE
#define NX_I2C_V1_DMA_DISABLE(control, mask) (*(control) &= ~(uint32_t)(mask))
#endif

/** \brief Cold selected engines must not alias or lack reviewed register facts.
 */
static bool engine_valid(const nx_i2c_v1_dma_engine_t* engine) {
    return engine->control != NULL && engine->count != NULL &&
           engine->peripheral != NULL && engine->memory != NULL &&
           engine->fifo != NULL && engine->flags != NULL &&
           engine->clear != NULL && engine->flag_shift <= 26U &&
           (engine->configuration & DMA_ENABLE) == 0U;
}

/** \brief Withdrawal is bounded; failure to clear EN retains the whole loan. */
static void detach_engine(const nx_i2c_v1_dma_engine_t* engine) {
    NX_I2C_V1_DMA_DISABLE(engine->control, DMA_ENABLE | DMA_IRQS);
    nx_arch_dsb();
    *engine->clear = DMA_FLAGS << engine->flag_shift;
}

/** \brief Only local master ownership and STOP establish local wire release. */
static bool wire_idle(const nx_i2c_v1_state_t* state) {
    return (state->registers[CTL0] & (START | STOP)) == 0U &&
           (state->registers[STAT1] & MASTER) == 0U;
}

/** \brief Reading STAT0 then STAT1 opens the precisely prepared ACK window. */
static void clear_address(nx_i2c_v1_state_t* state) {
    (void)state->registers[STAT0];
    (void)state->registers[STAT1];
    NX_I2C_V1_ADDRESS_CLEARED(state);
}

/** \brief Request an end only while this controller still owns the wire. */
static void end_message(nx_i2c_v1_state_t* state) {
    state->registers[CTL0] |=
        state->message + 1U < state->active->count ? START : STOP;
}

/** \brief Latch failure and withdraw sources before the later drain proof. */
static void begin_drain(nx_i2c_v1_state_t* state, nx_result_t result) {
    if (state->phase == NX_I2C_V1_DRAIN) {
        if (state->terminal == NX_SUCCESS && result != NX_SUCCESS) {
            state->terminal = result;
            state->faulted = true;
        }
        return;
    }
    state->terminal = result;
    /* Any abort can leave receive pipeline/status residue. A later request
     * requires explicit local recovery, never an implicit replay or reset. */
    state->faulted = result != NX_SUCCESS;
    state->phase = NX_I2C_V1_DRAIN;
    state->registers[CTL1] &=
        ~(ERROR_IRQ | EVENT_IRQ | BUFFER_IRQ | DMA_ON | DMA_LAST);
    if (state->dma != NULL) {
        detach_engine(&state->dma->tx);
        detach_engine(&state->dma->rx);
    }
    /* ARLO releases master ownership to another controller; never inject STOP
     * into that transaction. Successful receive already placed its sole STOP.
     */
    if (result != NX_ERROR_ARBITRATION &&
        (state->registers[STAT1] & MASTER) != 0U &&
        (state->registers[CTL0] & STOP) == 0U) {
        state->registers[CTL0] &= ~START;
        state->registers[CTL0] |= STOP;
    }
    state->drain_deadline = nx_deadline_after(nx_time_now_us(), 1000U);
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    if (state->ack_irqs != NULL) {
        state->ack_irqs(state);
    }
}

/** \brief DMA hardware is programmed before START, peripheral requests later.
 */
static void prepare_message(nx_i2c_v1_state_t* state) {
    state->index = 0U;
    state->phase = NX_I2C_V1_START;
    if (state->dma != NULL) {
        const nx_i2c_message_t* message =
            &state->active->messages[state->message];
        const nx_i2c_v1_dma_engine_t* engine =
            message->read ? &state->dma->rx : &state->dma->tx;
        *engine->clear = DMA_FLAGS << engine->flag_shift;
        *engine->peripheral = (uint32_t)(uintptr_t)&state->registers[DATA];
        *engine->memory = (uint32_t)(uintptr_t)message->data;
        *engine->count = (uint32_t)message->length;
        *engine->fifo = 0U;
        *engine->control = engine->configuration;
        nx_arch_dmb();
        *engine->control |= DMA_ENABLE;
    }
    state->registers[CTL1] = state->peripheral_mhz | ERROR_IRQ | EVENT_IRQ;
    /* Do not change ACK/POS while START can be cleared asynchronously. Their
     * next values are installed after SB and before the ADDR read sequence. */
    state->registers[CTL0] |= START;
}

/** \brief Full message prefixes are reported only after protocol completion. */
static void next_message(nx_i2c_v1_state_t* state) {
    if (state->message + 1U == state->active->count) {
        begin_drain(state, NX_SUCCESS);
        return;
    }
    ++state->message;
    prepare_message(state);
}

/** \brief Admission checks all messages and domains before MMIO or reference.
 */
static nx_result_t start(void* context, nx_i2c_request_t* request,
                         bool admitted) {
    nx_i2c_v1_endpoint_t* endpoint = context;
    if (endpoint == NULL || endpoint->port == NULL || endpoint->address < 8U ||
        endpoint->address > 119U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_result_t result = nx_i2c_request_validate(request);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_i2c_v1_state_t* state = endpoint->port;
    if (!state->initialized || state->stopping || state->faulted) {
        return NX_ERROR_STATE;
    }
    if (nx_request_state(&request->base) !=
        (admitted ? NX_REQUEST_QUEUED : NX_REQUEST_READY)) {
        return NX_ERROR_STATE;
    }
    if (request->base.deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (state->dma != NULL) {
        for (size_t i = 0U; i < request->count; ++i) {
            const nx_i2c_message_t* message = &request->messages[i];
            result = nx_dma_buffer_validate(
                state->dma->regions, state->dma->region_count, message->data,
                message->length, 1U,
                message->read ? NX_DMA_FROM_DEVICE : NX_DMA_TO_DEVICE);
            if (result != NX_SUCCESS) {
                return result;
            }
        }
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (state->active != NULL || (state->registers[STAT1] & BUS_BUSY) != 0U ||
        !wire_idle(state) ||
        (state->dma != NULL &&
         ((*state->dma->rx.control | *state->dma->tx.control) & DMA_ENABLE) !=
             0U)) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    result = admitted ? nx_request_transition(&request->base, NX_REQUEST_ACTIVE)
                      : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        state->active = request;
        state->address = endpoint->address;
        state->message = 0U;
        state->transferred = 0U;
        state->terminal = NX_SUCCESS;
        state->registers[STAT0] &= ~(NACK | ARBITRATION | BUS_ERROR | OVERRUN);
        prepare_message(state);
    }
    nx_arch_irq_restore(mask);
    return result;
}

/** \brief Admission borrows caller storage without copying descriptors. */
static nx_result_t submit(void* context, nx_i2c_request_t* request) {
    return start(context, request, false);
}

/** \brief A queued adapter loan moves to ACTIVE without being readmitted. */
static nx_result_t start_admitted(void* context, nx_i2c_request_t* request) {
    return start(context, request, true);
}

/** \brief Controller identity is exact even for multiple addressed children. */
static bool on_port(const void* context, const nx_i2c_port_t* port) {
    const nx_i2c_v1_endpoint_t* endpoint = context;
    return endpoint != NULL && port != NULL &&
           port->ops == &nx_i2c_v1_async_ops && port->context == endpoint->port;
}

/** \brief Initialize a selected mode; polling storage is wholly independent. */
nx_result_t nx_i2c_v1_initialize(nx_i2c_v1_state_t* state) {
    if (state == NULL || state->registers == NULL ||
        state->line_inputs == NULL || state->line_mask == 0U ||
        (state->line_mask & ~UINT32_C(0xFFFF)) != 0U ||
        (state->peripheral_mhz != 42U && state->peripheral_mhz != 50U)) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->dma != NULL &&
        (!engine_valid(&state->dma->tx) || !engine_valid(&state->dma->rx) ||
         state->dma->tx.control == state->dma->rx.control ||
         state->dma->regions == NULL || state->dma->region_count == 0U)) {
        return NX_ERROR_INVALID;
    }
    if (state->initialized || state->active != NULL ||
        (state->registers[STAT1] & BUS_BUSY) != 0U || !wire_idle(state) ||
        (state->dma != NULL &&
         ((*state->dma->rx.control | *state->dma->tx.control) & DMA_ENABLE) !=
             0U)) {
        return NX_ERROR_BUSY;
    }
    state->registers[CTL0] = 0U;
    state->registers[CTL1] = state->peripheral_mhz;
    state->registers[CKCFG] = (uint32_t)state->peripheral_mhz * 5U;
    state->registers[RT] = (uint32_t)state->peripheral_mhz + 1U;
    state->registers[ADDR0] = 1U << 14U;
    state->registers[CTL0] = ENABLE;
    state->phase = NX_I2C_V1_IDLE;
    state->stopping = false;
    state->faulted = false;
    state->initialized = true;
    return NX_SUCCESS;
}

/** \brief Detect error before consuming any data or advancing a prefix. */
static bool error(nx_i2c_v1_state_t* state, uint32_t flags) {
    nx_result_t result = (flags & ARBITRATION) != 0U ? NX_ERROR_ARBITRATION
                         : (flags & NACK) != 0U      ? NX_ERROR_NACK
                         : (flags & (BUS_ERROR | OVERRUN)) != 0U ? NX_ERROR_IO
                                                                 : NX_SUCCESS;
    if (result == NX_SUCCESS) {
        return false;
    }
    state->registers[STAT0] &= ~(NACK | ARBITRATION | BUS_ERROR | OVERRUN);
    begin_drain(state, result);
    return true;
}

/** \brief Consume at most two DATA bytes and never wait for a hardware flag. */
void nx_i2c_v1_irq(nx_i2c_v1_state_t* state) {
    if (state == NULL || !state->initialized) {
        return;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (state->active == NULL || state->phase == NX_I2C_V1_DRAIN) {
        nx_arch_irq_restore(mask);
        return;
    }
    const nx_irq_wake_t* wake = state->wake;
    uint32_t flags = state->registers[STAT0];
    if (!error(state, flags)) {
        if (nx_deadline_expired(state->active->base.deadline,
                                nx_time_now_us())) {
            begin_drain(state, NX_ERROR_TIMEOUT);
        } else {
            nx_i2c_message_t* message =
                &state->active->messages[state->message];
            size_t remaining = message->length - state->index;
            if (state->phase == NX_I2C_V1_START && (flags & SB) != 0U) {
                NX_I2C_V1_WRITE_DATA(state, (uint32_t)state->address * 2U |
                                                (message->read ? 1U : 0U));
                state->registers[CTL0] &= ~(START | ACK | POS);
                if (message->read && message->length > 1U) {
                    state->registers[CTL0] |= ACK;
                }
                if (state->dma == NULL && message->read &&
                    message->length == 2U) {
                    state->registers[CTL0] =
                        (state->registers[CTL0] | POS) & ~ACK;
                }
                state->phase = NX_I2C_V1_ADDRESS;
            } else if (state->phase == NX_I2C_V1_ADDRESS &&
                       (flags & ADDRESS) != 0U) {
                if (state->dma != NULL && message->read &&
                    message->length > 1U) {
                    state->registers[CTL1] |= DMA_LAST;
                }
                clear_address(state);
                if (state->dma != NULL) {
                    state->phase = message->read ? NX_I2C_V1_DMA_READ
                                                 : NX_I2C_V1_DMA_WRITE;
                    state->registers[CTL1] |= DMA_ON;
                } else if (!message->read) {
                    state->phase = NX_I2C_V1_WRITE;
                    state->registers[CTL1] |= BUFFER_IRQ;
                } else {
                    state->phase = message->length == 1U   ? NX_I2C_V1_READ_ONE
                                   : message->length == 2U ? NX_I2C_V1_READ_TWO
                                                           : NX_I2C_V1_READ;
                    if (message->length == 1U || message->length > 3U) {
                        state->registers[CTL1] |= BUFFER_IRQ;
                    }
                }
                if (message->read && message->length == 1U) {
                    end_message(state);
                }
            } else if (state->phase == NX_I2C_V1_WRITE && (flags & TXE) != 0U) {
                NX_I2C_V1_WRITE_DATA(state, message->data[state->index]);
                state->registers[CTL1] &= ~BUFFER_IRQ;
                state->phase = NX_I2C_V1_WRITE_ACK;
            } else if (state->phase == NX_I2C_V1_WRITE_ACK &&
                       (flags & BTF) != 0U) {
                ++state->index;
                ++state->transferred;
                if (state->index == message->length) {
                    end_message(state);
                    next_message(state);
                } else {
                    NX_I2C_V1_WRITE_DATA(state, message->data[state->index]);
                }
            } else if (state->phase == NX_I2C_V1_READ_ONE &&
                       (flags & RXNE) != 0U) {
                message->data[state->index++] = NX_I2C_V1_READ_DATA(state);
                ++state->transferred;
                next_message(state);
            } else if ((state->phase == NX_I2C_V1_READ_TWO ||
                        (state->phase == NX_I2C_V1_READ && remaining == 2U)) &&
                       (flags & BTF) != 0U) {
                end_message(state);
                message->data[state->index++] = NX_I2C_V1_READ_DATA(state);
                message->data[state->index++] = NX_I2C_V1_READ_DATA(state);
                state->transferred += 2U;
                next_message(state);
            } else if (state->phase == NX_I2C_V1_READ && remaining == 3U &&
                       (flags & BTF) != 0U) {
                state->registers[CTL0] &= ~ACK;
                message->data[state->index++] = NX_I2C_V1_READ_DATA(state);
                ++state->transferred;
                state->registers[CTL1] &= ~BUFFER_IRQ;
            } else if (state->phase == NX_I2C_V1_READ && remaining > 3U &&
                       (flags & RXNE) != 0U) {
                message->data[state->index++] = NX_I2C_V1_READ_DATA(state);
                ++state->transferred;
                if (remaining == 4U) {
                    state->registers[CTL1] &= ~BUFFER_IRQ;
                }
            } else if (state->phase == NX_I2C_V1_DMA_WRITE_ACK &&
                       (flags & BTF) != 0U) {
                state->transferred =
                    (uint16_t)(state->transferred + message->length);
                end_message(state);
                next_message(state);
            }
        }
    }
    nx_arch_irq_restore(mask);
    (void)nx_irq_wake_signal(wake);
}

/** \brief TC acknowledges memory progress, then separately proves the wire. */
void nx_i2c_v1_dma_irq(nx_i2c_v1_state_t* state, bool receive) {
    if (state == NULL || !state->initialized || state->dma == NULL) {
        return;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    const nx_i2c_v1_dma_engine_t* engine =
        receive ? &state->dma->rx : &state->dma->tx;
    uint32_t flags = (*engine->flags >> engine->flag_shift) & DMA_FLAGS;
    if (state->active == NULL || state->phase == NX_I2C_V1_DRAIN) {
        *engine->clear = DMA_FLAGS << engine->flag_shift;
        nx_arch_irq_restore(mask);
        return;
    }
    const nx_irq_wake_t* wake = state->wake;
    if (error(state, state->registers[STAT0])) {
        /* Peripheral terminal facts outrank an independent DMA TC. */
    } else if (state->phase !=
               (receive ? NX_I2C_V1_DMA_READ : NX_I2C_V1_DMA_WRITE)) {
        *engine->clear = DMA_FLAGS << engine->flag_shift;
    } else if ((flags & DMA_ERRORS) != 0U) {
        begin_drain(state, NX_ERROR_IO);
    } else if ((flags & DMA_COMPLETE) != 0U) {
        detach_engine(engine);
        state->registers[CTL1] &= ~(DMA_ON | DMA_LAST);
        if (*engine->count != 0U || (*engine->control & DMA_ENABLE) != 0U) {
            begin_drain(state, NX_ERROR_IO);
        } else if (nx_deadline_expired(state->active->base.deadline,
                                       nx_time_now_us())) {
            begin_drain(state, NX_ERROR_TIMEOUT);
        } else if (receive) {
            nx_i2c_message_t* message =
                &state->active->messages[state->message];
            state->transferred =
                (uint16_t)(state->transferred + message->length);
            if (message->length > 1U) {
                end_message(state);
            }
            next_message(state);
        } else {
            state->phase = NX_I2C_V1_DMA_WRITE_ACK;
        }
    }
    nx_arch_irq_restore(mask);
    (void)nx_irq_wake_signal(wake);
}

/** \brief Cancellation changes terminal intent, never invalidates memory. */
static nx_result_t cancel(void* context, nx_i2c_request_t* request) {
    nx_i2c_v1_state_t* state = context;
    if (state == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_result_t result = state->active == request ? NX_SUCCESS : NX_ERROR_STATE;
    if (result == NX_SUCCESS) {
        begin_drain(state, NX_ERROR_CANCELLED);
    }
    nx_arch_irq_restore(mask);
    return result;
}

/** \brief Retain quarantine permanently until an explicit hardware proof. */
static void service(void* context) {
    nx_i2c_v1_state_t* state = context;
    if (state == NULL || !state->initialized || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (state->active != NULL) {
        nx_time_us_t now = nx_time_now_us();
        if (state->phase != NX_I2C_V1_DRAIN &&
            nx_deadline_expired(state->active->base.deadline, now)) {
            begin_drain(state, NX_ERROR_TIMEOUT);
        }
        if (state->phase == NX_I2C_V1_DRAIN) {
            (void)error(state, state->registers[STAT0]);
            /* A START pending at cancellation may acquire master after the
             * initial drain attempt. Observe that late ownership and request
             * its STOP instead of releasing or spinning inside the IRQ. */
            if (state->terminal != NX_ERROR_ARBITRATION &&
                (state->registers[STAT1] & MASTER) != 0U &&
                (state->registers[CTL0] & STOP) == 0U) {
                state->registers[CTL0] &= ~START;
                state->registers[CTL0] |= STOP;
            }
            bool dma_idle =
                state->dma == NULL ||
                ((*state->dma->tx.control | *state->dma->rx.control) &
                 (DMA_ENABLE | DMA_IRQS)) == 0U;
            if (dma_idle && wire_idle(state)) {
                nx_i2c_request_t* request = state->active;
                nx_result_t result = state->terminal;
                size_t transferred = state->transferred;
                state->registers[CTL0] &= ~(ACK | POS);
                if (state->ack_irqs != NULL) {
                    state->ack_irqs(state);
                }
                state->active = NULL;
                state->phase = NX_I2C_V1_IDLE;
                nx_request_settle(&request->base, result, transferred);
            } else if (nx_deadline_expired(state->drain_deadline, now)) {
                state->faulted = true;
                (void)nx_request_transition(&state->active->base,
                                            NX_REQUEST_QUARANTINED);
            }
        }
    }
    nx_arch_irq_restore(mask);
}

/** \brief Stop removes producers after the last borrowed object releases. */
static nx_result_t stop(void* context) {
    nx_i2c_v1_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    state->stopping = true;
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
        if (state->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    state->registers[CTL1] = state->peripheral_mhz;
    if (state->dma != NULL) {
        detach_engine(&state->dma->tx);
        detach_engine(&state->dma->rx);
    }
    if (!wire_idle(state) ||
        (state->dma != NULL &&
         ((*state->dma->rx.control | *state->dma->tx.control) & DMA_ENABLE) !=
             0U)) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    if (state->ack_irqs != NULL) {
        state->ack_irqs(state);
    }
    if (state->stop_irqs != NULL) {
        state->stop_irqs(state);
    }
    state->registers[CTL0] = 0U;
    state->wake = NULL;
    state->initialized = false;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}

/** \brief Local reset only; pin authorization and nine clocks remain external.
 */
static nx_result_t recover(void* context) {
    nx_i2c_v1_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->stopping) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (!wire_idle(state) || (state->registers[STAT1] & BUS_BUSY) != 0U ||
        (*state->line_inputs & state->line_mask) != state->line_mask) {
        return NX_ERROR_IO;
    }
    state->registers[CTL0] = 1U << 15U;
    nx_arch_dsb();
    state->registers[CTL0] = 0U;
    state->initialized = false;
    return nx_i2c_v1_initialize(state);
}

/** \brief Attach only while all publishers are unborrowed and policy checked.
 */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_i2c_v1_state_t* state = context;
    if (state == NULL || (wake != NULL && wake->notify == NULL)) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->stopping || state->active != NULL) {
        return NX_ERROR_STATE;
    }
    if (state->validate_wake == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_result_t result = state->validate_wake(state, wake, syscall_ceiling);
    if (result == NX_SUCCESS) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        state->wake = wake;
        nx_arch_irq_restore(mask);
    }
    return result;
}

/** \brief One shared table per selected bounded asynchronous controller mode.
 */
const nx_i2c_ops_t nx_i2c_v1_async_ops = {
    .recover = recover,
    .cancel = cancel,
    .service = service,
    .stop = stop,
    .attach_wake = attach_wake,
};

/** \brief No polling fallback is present in an IRQ/DMA addressed endpoint. */
const nx_i2c_endpoint_ops_t nx_i2c_v1_async_endpoint_ops = {
    .submit = submit,
    .start_admitted = start_admitted,
    .on_port = on_port,
};
