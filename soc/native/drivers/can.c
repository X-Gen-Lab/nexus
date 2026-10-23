/**
 * \file            can.c
 * \brief           Explicit Native W1C, abort and bounded FIFO hardware model
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "can.h"

/** \brief Native storage begins at its register window, without a registry. */
static nx_native_can_registers_t* hardware(uintptr_t base) {
    return (nx_native_can_registers_t*)base;
}
/** \brief Mirror only the current front mailbox and three-deep FIFO count. */
static void receive_front(nx_native_can_registers_t* model, unsigned fifo) {
    unsigned offset = (0x1B0U + fifo * 16U) / 4U;
    uint8_t head = model->receive_head[fifo];
    for (unsigned i = 0; i < 4U; ++i) {
        model->registers[offset + i] = model->receive[fifo][head][i];
    }
    unsigned status = (0x0CU + fifo * 4U) / 4U;
    model->registers[status] =
        (model->registers[status] & ~3U) | model->receive_count[fifo];
}
/** \brief Native reads observe existing facts without fabricating progress. */
uint32_t nx_native_can_read(uintptr_t base, unsigned offset) {
    if (base == 0U || offset >= 800U || offset % 4U != 0U) {
        return 0U;
    }
    return hardware(base)->registers[offset / 4U];
}
/** \brief Model documented write-only side effects at the real store boundary.
 */
void nx_native_can_write(uintptr_t base, unsigned offset, uint32_t value) {
    if (base == 0U || offset >= 800U || offset % 4U != 0U) {
        return;
    }
    nx_native_can_registers_t* model = hardware(base);
    if (offset == 0U) {
        model->registers[0] = value;
        if ((value & 1U) != 0U && !model->hold_enter) {
            model->registers[1] |= 1U;
        } else if ((value & 1U) == 0U && !model->hold_exit) {
            model->registers[1] &= ~1U;
            /* Leaving acknowledged init models controller-local bus-off reset.
             */
            model->registers[0x18U / 4U] &= ~4U;
        }
    } else if (offset == 4U) {
        model->registers[1] &= ~(value & 0x1CU);
    } else if (offset == 8U) {
        for (unsigned i = 0; i < 3U; ++i) {
            uint32_t complete = 1U << (i * 8U);
            if ((value & complete) != 0U && !model->hold_completion_clear) {
                model->registers[2] &= ~(15U << (i * 8U));
            }
            if ((value & (1U << (i * 8U + 7U))) != 0U && !model->hold_abort &&
                (model->registers[(0x180U + i * 16U) / 4U] & 1U) != 0U) {
                model->registers[(0x180U + i * 16U) / 4U] &= ~1U;
                model->registers[2] |= complete | (1U << (26U + i));
            }
        }
    } else if (offset == 0x0CU || offset == 0x10U) {
        unsigned fifo = offset == 0x0CU ? 0U : 1U;
        model->registers[offset / 4U] &= ~(value & 0x18U);
        if ((value & 32U) != 0U && model->hold_fifo_release) {
            model->registers[offset / 4U] |= 32U;
            return;
        }
        if ((value & 32U) != 0U) {
            model->registers[offset / 4U] &= ~32U;
        }
        if ((value & 32U) != 0U && model->receive_count[fifo] != 0U) {
            model->receive_head[fifo] =
                (uint8_t)((model->receive_head[fifo] + 1U) % 3U);
            --model->receive_count[fifo];
            receive_front(model, fifo);
        }
    } else if (offset == 0x14U && value == 0U && model->hold_interrupt_clear) {
        return;
    } else {
        model->registers[offset / 4U] = value;
        if (offset >= 0x180U && offset <= 0x1A0U &&
            (offset - 0x180U) % 16U == 0U && (value & 1U) != 0U) {
            unsigned mailbox = (offset - 0x180U) / 16U;
            model->registers[2] &= ~(1U << (26U + mailbox));
        }
    }
}
/** \brief Explicit selected IRQ state has a testable disable acknowledgement.
 */
void nx_native_can_irq_set(nx_can_controller_t* controller, unsigned vector,
                           bool enabled) {
    nx_native_can_registers_t* model = hardware(controller->registers);
    if (enabled && !model->hold_irq_enable) {
        model->enabled_irqs |= (uint8_t)(1U << vector);
    } else if (!enabled && !model->hold_irq_disable) {
        model->enabled_irqs &= (uint8_t) ~(1U << vector);
    }
}
/** \brief Read back actual modeled vector ownership rather than assume writes.
 */
bool nx_native_can_irq_enabled(nx_can_controller_t* controller,
                               unsigned vector) {
    return (hardware(controller->registers)->enabled_irqs & (1U << vector)) !=
           0U;
}
/** \brief Bind supplied static storage and expose initially empty mailboxes. */
nx_result_t nx_native_can_initialize(nx_native_can_t* instance) {
    if (instance == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!instance->controller.snapshot.started) {
        instance->controller.registers = (uintptr_t)&instance->hardware;
        instance->hardware.registers[2] = 7U << 26U;
    }
    return nx_can_controller_initialize(&instance->controller);
}
/** \brief Completion injection does not execute the provider until IRQ/service.
 */
nx_result_t nx_native_can_complete(nx_native_can_t* instance, unsigned mailbox,
                                   nx_result_t result) {
    if (instance == NULL || mailbox > 2U ||
        (result != NX_SUCCESS && result != NX_ERROR_ARBITRATION &&
         result != NX_ERROR_IO && result != NX_ERROR_CANCELLED)) {
        return NX_ERROR_INVALID;
    }
    nx_native_can_registers_t* model = &instance->hardware;
    unsigned offset = (0x180U + mailbox * 16U) / 4U;
    if ((model->registers[offset] & 1U) == 0U) {
        return NX_ERROR_STATE;
    }
    model->registers[offset] &= ~1U;
    uint32_t status = 1U | (result == NX_SUCCESS             ? 2U
                            : result == NX_ERROR_ARBITRATION ? 4U
                            : result == NX_ERROR_IO          ? 8U
                                                             : 0U);
    model->registers[2] |= status << (mailbox * 8U);
    model->registers[2] |= 1U << (26U + mailbox);
    return NX_SUCCESS;
}
/** \brief Inject a bounded hardware FIFO; overflow remains a lower-bound fact.
 */
nx_result_t nx_native_can_receive(nx_native_can_t* instance, unsigned fifo,
                                  const nx_can_frame_t* frame, uint8_t raw_dlc,
                                  uint8_t filter) {
    if (instance == NULL || fifo > 1U || raw_dlc > 15U ||
        !nx_can_frame_valid(frame)) {
        return NX_ERROR_INVALID;
    }
    nx_native_can_registers_t* model = &instance->hardware;
    if (model->receive_count[fifo] == 3U) {
        model->registers[(0x0CU + fifo * 4U) / 4U] |= 16U;
        return NX_ERROR_OVERFLOW;
    }
    unsigned tail =
        (unsigned)(model->receive_head[fifo] + model->receive_count[fifo]) % 3U;
    uint32_t* mailbox = model->receive[fifo][tail];
    mailbox[0] = frame->id << (frame->format == NX_CAN_STANDARD ? 21U : 3U);
    mailbox[0] |= frame->format == NX_CAN_EXTENDED ? 4U : 0U;
    mailbox[0] |= frame->kind == NX_CAN_REMOTE ? 2U : 0U;
    mailbox[1] = raw_dlc | ((uint32_t)filter << 8U);
    mailbox[2] = 0U;
    mailbox[3] = 0U;
    for (unsigned i = 0; i < frame->dlc; ++i) {
        mailbox[2U + i / 4U] |= (uint32_t)frame->data[i] << ((i % 4U) * 8U);
    }
    ++model->receive_count[fifo];
    receive_front(model, fifo);
    return NX_SUCCESS;
}
/** \brief Error injection survives receive-ring overflow without a callback. */
void nx_native_can_error(nx_native_can_t* instance, bool bus_off, uint8_t tec,
                         uint8_t rec, uint8_t last_error) {
    if (instance == NULL) {
        return;
    }
    instance->hardware.registers[0x18U / 4U] =
        (uint32_t)tec << 16U | (uint32_t)rec << 24U |
        ((uint32_t)last_error & 7U) << 4U | (bus_off ? 4U : 0U);
    instance->hardware.registers[1] |= 4U;
}
