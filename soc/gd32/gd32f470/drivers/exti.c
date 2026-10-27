/**
 * \file            exti.c
 * \brief           GD32 static EXTI line ownership and bounded shared vectors
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"

#ifndef NX_GD32_EXTI_CLEAR
#define NX_GD32_EXTI_CLEAR(mask) (EXTI_PD = (mask))
#endif

/** \brief           Map GPIO lines to their exact silicon vector. */
static IRQn_Type vector(unsigned line) {
    if (line < 5u) {
        return (IRQn_Type)((unsigned)EXTI0_IRQn + line);
    }
    return line < 10u ? EXTI5_9_IRQn : EXTI10_15_IRQn;
}

/** \brief Validate the immutable selected domain before register effects. */
static bool domain_valid(const nx_gd32_exti_domain_t* domain,
                         const nx_gd32_exti_state_t* self) {
    if (domain == NULL || domain->ports == NULL || domain->count == 0U ||
        domain->count > 16U)
        return false;
    bool found = false;
    for (size_t i = 0U; i < domain->count; ++i) {
        const nx_gd32_exti_state_t* port = domain->ports[i];
        if (port == NULL ||
            (port->initialized && (port->domain != domain || port->line > 15U)))
            return false;
        found |= port == self;
        for (size_t j = 0U; j < i; ++j)
            if (port == domain->ports[j])
                return false;
    }
    return found;
}

/** \brief Exact shared-vector line mask, independent of selected array size. */
static uint32_t line_vector_mask(unsigned line) {
    return line < 5U ? UINT32_C(1) << line
                     : (line < 10U ? UINT32_C(0x3E0) : UINT32_C(0xFC00));
}

/** \brief           Bind after duplicate line and shared priority checks. */
nx_result_t nx_gd32_exti_initialize(nx_gd32_exti_state_t* port,
                                    const nx_gd32_exti_domain_t* domain,
                                    unsigned line, unsigned gpio_index,
                                    nx_exti_edge_t edges,
                                    nx_exti_event_t* events, size_t capacity,
                                    unsigned priority) {
    if (!port || !events || !capacity ||
        capacity > SIZE_MAX / sizeof(*events) || capacity > SIZE_MAX / 2u ||
        line > 15u || gpio_index > 8u || edges < NX_EXTI_RISING ||
        edges > NX_EXTI_BOTH || priority > 15u || !domain_valid(domain, port)) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t saved = nx_gd32_critical_enter();
    IRQn_Type irq = vector(line);
    if (port->initialized) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
    uint32_t owned = 0U;
    for (size_t i = 0U; i < domain->count; ++i) {
        const nx_gd32_exti_state_t* other = domain->ports[i];
        if (!other->initialized)
            continue;
        if (other->line == line ||
            (vector(other->line) == irq && NVIC_GetPriority(irq) != priority)) {
            nx_gd32_critical_leave(saved);
            return NX_ERROR_BUSY;
        }
        if (vector(other->line) == irq)
            owned |= UINT32_C(1) << other->line;
    }
    uint32_t mask = UINT32_C(1) << line;
    uint32_t irq_bit = UINT32_C(1) << ((unsigned)irq % 32U);
    unsigned irq_word = (unsigned)irq / 32U;
    if ((EXTI_INTEN & line_vector_mask(line) & ~owned) != 0U ||
        (NVIC->IABR[irq_word] & irq_bit) != 0U ||
        (owned == 0U &&
         ((NVIC->ISER[irq_word] | NVIC->ISPR[irq_word]) & irq_bit) != 0U)) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_BUSY;
    }
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
    NX_GD32_EXTI_CLEAR(mask);
    *port = (nx_gd32_exti_state_t){.events = events,
                                   .domain = domain,
                                   .capacity = capacity,
                                   .line = (uint8_t)line,
                                   .gpio_index = (uint8_t)gpio_index,
                                   .edges = edges,
                                   .initialized = true};
    /* Initialized marks owned cleanup storage, not a successful READY result.
     * Complete state and domain publication before an IRQ can observe them. */
    nx_gd32_peripheral_barrier();
    NVIC_SetPriority(irq, priority);
    NVIC_EnableIRQ(irq);
    EXTI_INTEN |= mask;
    nx_gd32_peripheral_barrier();
    uint32_t rising = (edges & NX_EXTI_RISING) != 0U ? mask : 0U;
    uint32_t falling = (edges & NX_EXTI_FALLING) != 0U ? mask : 0U;
    if ((NVIC->ISER[irq_word] & irq_bit) == 0U ||
        NVIC_GetPriority(irq) != priority || (EXTI_INTEN & mask) != mask ||
        (EXTI_RTEN & mask) != rising || (EXTI_FTEN & mask) != falling) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_IO;
    }
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief           Dispatch only the selected states in deterministic order.
 */
void nx_gd32_exti_dispatch(const nx_gd32_exti_domain_t* domain,
                           uint32_t vector_mask) {
    if (domain == NULL || domain->ports == NULL || domain->count == 0U ||
        domain->count > 16U)
        return;
    uint32_t pending = EXTI_PD & EXTI_INTEN & vector_mask;
    if (pending == 0U)
        return;
    nx_time_us_t now = nx_time_now_us();
    for (size_t i = 0U; i < domain->count; ++i) {
        nx_gd32_exti_state_t* port = domain->ports[i];
        if (port == NULL || !port->initialized || port->domain != domain ||
            port->line > 15U) {
            continue;
        }
        unsigned line = port->line;
        uint32_t mask = UINT32_C(1) << line;
        if ((pending & mask) == 0U) {
            continue;
        }
        NX_GD32_EXTI_CLEAR(mask);
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

/** \brief Cold cleanup skips rejected storage but retains live IRQ ownership.
 */
nx_result_t nx_gd32_exti_close(nx_gd32_exti_state_t* port) {
    if (port == NULL)
        return NX_ERROR_INVALID;
    return port->initialized ? nx_gd32_exti_stop(port) : NX_SUCCESS;
}

/** \brief           Withdraw owned sources before releasing borrowed storage.
 * \note            Refused writes retain domain, queue and wake obligations;
 *                  a later serialized stop retries without disturbing peers.
 */
nx_result_t nx_gd32_exti_stop(void* context) {
    nx_gd32_exti_state_t* port = context;
    if (!port || !port->initialized || port->line > 15u ||
        !domain_valid(port->domain, port)) {
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
    EXTI_RTEN &= ~mask;
    EXTI_FTEN &= ~mask;
    NX_GD32_EXTI_CLEAR(mask);
    nx_gd32_peripheral_barrier();
    if (((EXTI_INTEN | EXTI_RTEN | EXTI_FTEN | EXTI_PD) & mask) != 0U) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_IO;
    }
    bool shared = false;
    for (size_t i = 0U; i < port->domain->count; ++i) {
        const nx_gd32_exti_state_t* other = port->domain->ports[i];
        shared |=
            other != port && other->initialized && vector(other->line) == irq;
    }
    if (!shared) {
        NVIC_DisableIRQ(irq);
        NVIC_ClearPendingIRQ(irq);
        nx_gd32_peripheral_barrier();
    }
    unsigned word = (unsigned)irq / 32U;
    if ((NVIC->IABR[word] & bit) != 0U ||
        (!shared && ((NVIC->ISER[word] | NVIC->ISPR[word]) & bit) != 0U)) {
        nx_gd32_critical_leave(saved);
        return NX_ERROR_IO;
    }
    port->wake = NULL;
    port->initialized = false;
    port->domain = NULL;
    port->count = 0u;
    port->lost = 0u;
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
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
