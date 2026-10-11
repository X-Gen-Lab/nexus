/**
 * \file            exti.c
 * \brief           Static EXTI line dispatch with bounded observable event loss
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"

#ifndef NX_STM32_CLEAR_FLAGS
#define NX_STM32_CLEAR_FLAGS(reg, mask) ((reg) = (mask))
#endif

#ifndef NX_STM32_NVIC_DISABLE
#define NX_STM32_NVIC_DISABLE(irq) NVIC_DisableIRQ(irq)
#define NX_STM32_NVIC_CLEAR(irq)   NVIC_ClearPendingIRQ(irq)
#endif

/** \brief Configure one exclusive line, leaving other shared-vector lines
 * intact. */
nx_result_t nx_stm32_exti_initialize(nx_stm32_exti_state_t* port,
                                     SYSCFG_TypeDef* mux, uint8_t gpio_index) {
    if (port == NULL || port->registers == NULL || port->gpio == NULL ||
        mux == NULL || port->line > 15U || gpio_index > 8U ||
        port->storage == NULL || port->capacity == 0U ||
        port->edge < NX_EXTI_RISING || port->edge > NX_EXTI_BOTH) {
        return NX_ERROR_INVALID;
    }
    uint32_t bit = 1U << port->line;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (port->initialized || (port->registers->IMR & bit) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    unsigned offset = (port->line % 4U) * 4U;
    mux->EXTICR[port->line / 4U] =
        (mux->EXTICR[port->line / 4U] & ~(15U << offset)) |
        ((uint32_t)gpio_index << offset);
    port->registers->RTSR = (port->registers->RTSR & ~bit) |
                            ((port->edge & NX_EXTI_RISING) != 0 ? bit : 0U);
    port->registers->FTSR = (port->registers->FTSR & ~bit) |
                            ((port->edge & NX_EXTI_FALLING) != 0 ? bit : 0U);
    NX_STM32_CLEAR_FLAGS(port->registers->PR, bit);
    port->head = 0U;
    port->tail = 0U;
    port->count = 0U;
    port->loss = 0U;
    port->initialized = true;
    port->registers->IMR |= bit;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Visit at most sixteen configured lines in deterministic array order.
 */
void nx_stm32_exti_dispatch(nx_stm32_exti_state_t* const* ports, size_t count,
                            uint32_t vector_mask) {
    if (ports == NULL || count > 16U) {
        return;
    }
    for (size_t i = 0U; i < count; ++i) {
        nx_stm32_exti_state_t* port = ports[i];
        if (port == NULL || !port->initialized) {
            continue;
        }
        uint32_t bit = 1U << port->line;
        if ((port->registers->PR & port->registers->IMR & vector_mask & bit) ==
            0U) {
            continue;
        }
        NX_STM32_CLEAR_FLAGS(port->registers->PR, bit);
        if (port->count == port->capacity || port->loss != 0U) {
            port->loss = 1U;
            (void)nx_irq_wake_signal(port->wake);
            continue;
        }
        nx_exti_edge_t edge = port->edge;
        if (edge == NX_EXTI_BOTH) {
            edge = (port->gpio->IDR & bit) != 0U ? NX_EXTI_RISING
                                                 : NX_EXTI_FALLING;
        }
        port->storage[port->head] =
            (nx_exti_event_t){.timestamp_us = nx_time_now_us(),
                              .flags = NX_EXTI_EVENT_COALESCED,
                              .line = port->line,
                              .edge = edge};
        if (++port->head == port->capacity) {
            port->head = 0U;
        }
        ++port->count;
        (void)nx_irq_wake_signal(port->wake);
    }
}

/** \brief Consume events and expose a pending loss even if no new edge arrives.
 */
nx_result_t nx_stm32_exti_read(void* context, nx_exti_event_t* events,
                               size_t capacity, size_t* count) {
    nx_stm32_exti_state_t* port = context;
    if (port == NULL || events == NULL || capacity == 0U || count == NULL ||
        !port->initialized) {
        return NX_ERROR_INVALID;
    }
    *count = 0U;
    while (*count < capacity) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        if (port->count == 0U) {
            nx_arch_irq_restore(saved);
            break;
        }
        nx_exti_event_t event = port->storage[port->tail];
        if (++port->tail == port->capacity) {
            port->tail = 0U;
        }
        --port->count;
        nx_arch_irq_restore(saved);
        events[(*count)++] = event;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    bool loss = *count < capacity && port->count == 0U && port->loss != 0U;
    if (loss) {
        port->loss = 0U;
    }
    nx_arch_irq_restore(saved);
    if (loss) {
        events[(*count)++] = (nx_exti_event_t){.timestamp_us = nx_time_now_us(),
                                               .flags = NX_EXTI_EVENT_LOSS,
                                               .line = port->line,
                                               .edge = port->edge};
    }
    return *count == 0U ? NX_ERROR_EMPTY : NX_SUCCESS;
}

/** \brief Mask and acknowledge only this line before its storage is released.
 */
nx_result_t nx_stm32_exti_stop(void* context) {
    nx_stm32_exti_state_t* port = context;
    if (port == NULL || port->registers == NULL || port->line > 15U) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    uint32_t bit = 1U << port->line;
    port->registers->IMR &= ~bit;
    NX_STM32_CLEAR_FLAGS(port->registers->PR, bit);
    nx_arch_dsb();
    port->wake = NULL;
    port->initialized = false;
    IRQn_Type irq = port->line <= 4U
                        ? (IRQn_Type)((int)EXTI0_IRQn + (int)port->line)
                        : (port->line <= 9U ? EXTI9_5_IRQn : EXTI15_10_IRQn);
    uint32_t vector_mask =
        port->line <= 4U ? bit : (port->line <= 9U ? 0x3E0U : 0xFC00U);
    if ((port->registers->IMR & vector_mask) == 0U) {
        NX_STM32_NVIC_DISABLE(irq);
        NX_STM32_NVIC_CLEAR(irq);
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Validate shared-vector priority before attaching a borrowed sink. */
static nx_result_t stm32_exti_attach_wake(void* context,
                                          const nx_irq_wake_t* wake,
                                          uint8_t syscall_ceiling) {
    nx_stm32_exti_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!port->initialized) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    IRQn_Type irq = port->line <= 4U
                        ? (IRQn_Type)((int)EXTI0_IRQn + (int)port->line)
                        : (port->line <= 9U ? EXTI9_5_IRQn : EXTI15_10_IRQn);
    nx_result_t result = nx_stm32_irq_wake_validate(wake, irq, syscall_ceiling);
    if (result == NX_SUCCESS) {
        port->wake = wake;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_exti_ops_t nx_stm32_exti_ops = {
    .read = nx_stm32_exti_read,
    .stop = nx_stm32_exti_stop,
    .attach_wake = stm32_exti_attach_wake,
};
