/**
 * \file            exti.c
 * \brief           GD32 static EXTI line ownership and bounded shared vectors
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"

static nx_gd32_exti_state_t* s_lines[16];

/** \brief           Map GPIO lines to their exact silicon vector. */
static IRQn_Type vector(unsigned line) {
    if (line < 5u) {
        return (IRQn_Type)((unsigned)EXTI0_IRQn + line);
    }
    return line < 10u ? EXTI5_9_IRQn : EXTI10_15_IRQn;
}

/** \brief           Bind after duplicate line and shared priority checks. */
nx_result_t nx_gd32_exti_initialize(nx_gd32_exti_state_t* port, unsigned line,
                                    unsigned gpio_index, nx_exti_edge_t edges,
                                    nx_exti_event_t* events, size_t capacity,
                                    unsigned priority) {
    if (!port || !events || !capacity ||
        capacity > SIZE_MAX / sizeof(*events) || capacity > SIZE_MAX / 2u ||
        line > 15u || gpio_index > 8u || edges < NX_EXTI_RISING ||
        edges > NX_EXTI_BOTH || priority > 15u) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    IRQn_Type irq = vector(line);
    if (s_lines[line]) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
    for (unsigned i = 0u; i < 16u; ++i) {
        if (s_lines[i] && vector(i) == irq &&
            NVIC_GetPriority(irq) != priority) {
            nx_gd32_critical_leave(saved);
            return NX_ERROR_BUSY;
        }
    }
    uint32_t mask = UINT32_C(1) << line;
    RCU_AHB1EN |= UINT32_C(1) << gpio_index;
    rcu_periph_clock_enable(RCU_SYSCFG);
    gpio_mode_set(GPIOA + gpio_index * 0x400u, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
                  mask);
    volatile uint32_t* selection =
        (volatile uint32_t*)(uintptr_t)(SYSCFG + 8u + (line / 4u) * 4u);
    unsigned shift = (line % 4u) * 4u;
    *selection = (*selection & ~(15u << shift)) | (gpio_index << shift);
    EXTI_INTEN &= ~mask;
    EXTI_RTEN =
        (EXTI_RTEN & ~mask) | ((edges & NX_EXTI_RISING) != 0u ? mask : 0u);
    EXTI_FTEN =
        (EXTI_FTEN & ~mask) | ((edges & NX_EXTI_FALLING) != 0u ? mask : 0u);
    EXTI_PD = mask;
    *port = (nx_gd32_exti_state_t){.events = events,
                                   .capacity = capacity,
                                   .line = (uint8_t)line,
                                   .gpio_index = (uint8_t)gpio_index,
                                   .edges = edges,
                                   .initialized = true};
    s_lines[line] = port;
    NVIC_SetPriority(irq, priority);
    NVIC_EnableIRQ(irq);
    EXTI_INTEN |= mask;
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief           Dispatch a finite line subset in ascending line order. */
static void dispatch(uint32_t vector_mask) {
    uint32_t pending = EXTI_PD & EXTI_INTEN & vector_mask;
    nx_time_us_t now = nx_time_now_us();
    for (unsigned line = 0u; line < 16u; ++line) {
        uint32_t mask = UINT32_C(1) << line;
        nx_gd32_exti_state_t* port = s_lines[line];
        if ((pending & mask) == 0u || !port || !port->initialized) {
            continue;
        }
        EXTI_PD = mask;
        if (port->count == port->capacity || port->lost) {
            if (port->lost != UINT32_MAX) {
                ++port->lost;
            }
            (void)nx_irq_wake_signal(port->wake);
            continue;
        }
        nx_exti_edge_t edge = port->edges;
        if (edge == NX_EXTI_BOTH) {
            uint32_t input = GPIO_ISTAT(GPIOA + port->gpio_index * 0x400u);
            edge = (input & mask) != 0u ? NX_EXTI_RISING : NX_EXTI_FALLING;
        }
        size_t tail = port->head + port->count;
        if (tail >= port->capacity) {
            tail -= port->capacity;
        }
        port->events[tail] = (nx_exti_event_t){now, NX_EXTI_EVENT_COALESCED,
                                               (uint8_t)line, edge};
        ++port->count;
        (void)nx_irq_wake_signal(port->wake);
    }
}

/** \brief           Return queue order then a loss marker before new events. */
nx_result_t nx_gd32_exti_read(void* context, nx_exti_event_t* events,
                              size_t capacity, size_t* count) {
    nx_gd32_exti_state_t* port = context;
    if (!port || !port->initialized || !events || !capacity || !count) {
        return NX_ERROR_INVALID;
    }
    *count = 0u;
    while (*count < capacity) {
        uint32_t saved = nx_gd32_critical_enter();
        if (port->count) {
            events[(*count)++] = port->events[port->head];
            if (++port->head == port->capacity) {
                port->head = 0u;
            }
            --port->count;
        } else if (port->lost) {
            events[(*count)++] = (nx_exti_event_t){
                nx_time_now_us(), NX_EXTI_EVENT_LOSS, port->line, port->edges};
            port->lost = 0u;
        } else {
            nx_gd32_critical_leave(saved);
            break;
        }
        nx_gd32_critical_leave(saved);
    }
    return *count ? NX_SUCCESS : NX_ERROR_EMPTY;
}

/** \brief           Preserve shared line sources and retain storage on BUSY. */
nx_result_t nx_gd32_exti_stop(void* context) {
    nx_gd32_exti_state_t* port = context;
    if (!port || !port->initialized || port->line > 15u ||
        s_lines[port->line] != port) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    IRQn_Type irq = vector(port->line);
    uint32_t bit = UINT32_C(1) << ((unsigned)irq % 32u);
    if ((NVIC->IABR[(unsigned)irq / 32u] & bit) != 0u) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
    uint32_t mask = UINT32_C(1) << port->line;
    EXTI_INTEN &= ~mask;
    EXTI_PD = mask;
    nx_gd32_peripheral_barrier();
    s_lines[port->line] = NULL;
    bool shared = false;
    for (unsigned i = 0u; i < 16u; ++i) {
        shared |= s_lines[i] != NULL && vector(i) == irq;
    }
    if (!shared) {
        NVIC_DisableIRQ(irq);
        NVIC_ClearPendingIRQ(irq);
    }
    port->wake = NULL;
    port->initialized = false;
    port->count = 0u;
    port->lost = 0u;
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief           Service one dedicated line. */
void EXTI0_IRQHandler(void) {
    dispatch(1u);
}
/** \brief           Service one dedicated line. */
void EXTI1_IRQHandler(void) {
    dispatch(2u);
}
/** \brief           Service one dedicated line. */
void EXTI2_IRQHandler(void) {
    dispatch(4u);
}
/** \brief           Service one dedicated line. */
void EXTI3_IRQHandler(void) {
    dispatch(8u);
}
/** \brief           Service one dedicated line. */
void EXTI4_IRQHandler(void) {
    dispatch(16u);
}
/** \brief           Service five bounded shared lines. */
void EXTI5_9_IRQHandler(void) {
    dispatch(0x3E0u);
}
/** \brief           Service six bounded shared lines. */
void EXTI10_15_IRQHandler(void) {
    dispatch(0xFC00u);
}

/** \brief Validate shared-vector priority before attaching a borrowed sink. */
static nx_result_t gd32_exti_attach_wake(void* context,
                                         const nx_irq_wake_t* wake,
                                         uint8_t syscall_ceiling) {
    nx_gd32_exti_state_t* port = context;
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    if (!port->initialized) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_STATE;
    }
    nx_result_t result =
        nx_gd32_irq_wake_validate(wake, vector(port->line), syscall_ceiling);
    if (result == NX_SUCCESS) {
        port->wake = wake;
    }
    nx_gd32_critical_leave(saved);
    return result;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_exti_ops_t nx_gd32_exti_ops = {
    .read = nx_gd32_exti_read,
    .stop = nx_gd32_exti_stop,
    .attach_wake = gd32_exti_attach_wake,
};
