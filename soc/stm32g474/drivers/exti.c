/**
 * \file            exti.c
 * \brief           G474 exact PA0 and PA1 EXTI line ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_provider.h"

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool span(const void* pointer, size_t bytes) {
    return pointer != NULL && bytes != 0U &&
           (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool hardware_alias(const nx_stm32g474_exti_state_t* p,
                           const void* pointer, size_t bytes) {
    return overlap(pointer, bytes, p, sizeof(*p)) ||
           overlap(pointer, bytes, p->registers, sizeof(*p->registers)) ||
           overlap(pointer, bytes, p->mux, sizeof(*p->mux)) ||
           overlap(pointer, bytes, p->gpio, sizeof(*p->gpio));
}
static bool output_alias(const nx_stm32g474_exti_state_t* p,
                         const void* pointer, size_t bytes) {
    return !span(pointer, bytes) || hardware_alias(p, pointer, bytes) ||
           overlap(pointer, bytes, p->storage,
                   p->capacity * sizeof(*p->storage)) ||
           (p->wake != NULL &&
            overlap(pointer, bytes, p->wake, sizeof(*p->wake)));
}
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_G474_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_G474_READ(reg) & mask) == (value & mask);
}
static IRQn_Type vector(const nx_stm32g474_exti_state_t* p) {
    return p->line == 0U ? EXTI0_IRQn : EXTI1_IRQn;
}

nx_result_t nx_stm32g474_exti_initialize(nx_stm32g474_exti_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->registers == NULL || p->mux == NULL ||
        p->gpio == NULL || p->capacity == 0U || p->capacity > 4096U ||
        !span(p->storage, p->capacity * sizeof(*p->storage)) ||
        (uintptr_t)p->storage % _Alignof(nx_exti_event_t) != 0U ||
        p->edge < NX_EXTI_RISING || p->edge > NX_EXTI_BOTH || p->priority > 15U)
        return NX_ERROR_INVALID;
    if (p->line > 1U || p->registers != EXTI || p->mux != SYSCFG ||
        p->gpio != GPIOA)
        return NX_ERROR_UNSUPPORTED;
    if (hardware_alias(p, p->storage, p->capacity * sizeof(*p->storage)))
        return NX_ERROR_INVALID;
    uint32_t bit = 1U << p->line;
    uint32_t shift = (uint32_t)p->line * 2U;
    if (p->retained || p->initialized || p->wake != NULL ||
        p->wake_publishers != 0U || !NX_G474_IRQ_AVAILABLE(vector(p)) ||
        ((NX_G474_READ(&p->registers->IMR1) |
          NX_G474_READ(&p->registers->EMR1) |
          NX_G474_READ(&p->registers->RTSR1) |
          NX_G474_READ(&p->registers->FTSR1) |
          NX_G474_READ(&p->registers->PR1)) &
         bit) != 0U)
        return NX_ERROR_BUSY;
    if (!g_nx_stm32g474_system.started || g_nx_stm32g474_system.rcc == NULL ||
        (NX_G474_READ(&g_nx_stm32g474_system.rcc->AHB2ENR) &
         RCC_AHB2ENR_GPIOAEN) == 0U ||
        (NX_G474_READ(&g_nx_stm32g474_system.rcc->APB2ENR) &
         RCC_APB2ENR_SYSCFGEN) == 0U)
        return NX_ERROR_STATE;
    uint32_t mode = (NX_G474_READ(&p->gpio->MODER) >> shift) & 3U;
    if ((mode != 0U && mode != 3U) ||
        (NX_G474_READ(&p->gpio->LCKR) & bit) != 0U)
        return NX_ERROR_BUSY;
    nx_irq_source_t source = {(int16_t)vector(p), p->priority,
                              (uint8_t)NX_G474_IRQ_GROUP()};
    nx_result_t policy = nx_irq_wake_validate(NULL, &p->irq_policy, &source);
    if (policy != NX_SUCCESS)
        return policy;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    uint32_t mux_shift = (uint32_t)p->line * 4U;
    p->saved_mux =
        (uint8_t)((NX_G474_READ(&p->mux->EXTICR[0]) >> mux_shift) & 15U);
    p->retained = true;
    p->closing = false;
    p->head = p->tail = p->count = 0U;
    p->loss = false;
    /* Platform owns the full original GPIO lease. Configure no pull: external
     * electrical bias and debounce are explicit product/Board responsibilities.
     */
    bool valid = write_checked(&p->gpio->PUPDR,
                               NX_G474_READ(&p->gpio->PUPDR) & ~(3U << shift),
                               3U << shift);
    valid = write_checked(&p->gpio->MODER,
                          NX_G474_READ(&p->gpio->MODER) & ~(3U << shift),
                          3U << shift) &&
            valid;
    valid =
        write_checked(&p->mux->EXTICR[0],
                      NX_G474_READ(&p->mux->EXTICR[0]) & ~(15U << mux_shift),
                      15U << mux_shift) &&
        valid;
    /* Triggers were admitted disabled, so a refused initial W1C is observable
     * without treating a new physical edge as an acknowledgement failure. */
    NX_G474_WRITE(&p->registers->PR1, bit);
    nx_arch_dsb();
    valid = (NX_G474_READ(&p->registers->PR1) & bit) == 0U && valid;
    if (valid) {
        uint32_t rising = (p->edge & NX_EXTI_RISING) != 0U ? bit : 0U;
        uint32_t falling = (p->edge & NX_EXTI_FALLING) != 0U ? bit : 0U;
        valid = write_checked(&p->registers->RTSR1,
                              NX_G474_READ(&p->registers->RTSR1) | rising, bit);
        valid =
            write_checked(&p->registers->FTSR1,
                          NX_G474_READ(&p->registers->FTSR1) | falling, bit) &&
            valid;
    }
    if (valid) {
        p->initialized = true;
        valid = write_checked(&p->registers->IMR1,
                              NX_G474_READ(&p->registers->IMR1) | bit, bit);
        if (valid)
            valid =
                NX_G474_IRQ_CONTROL(vector(p), true, p->priority) == NX_SUCCESS;
    }
    if (!valid) {
        p->closing = true;
        p->initialized = false;
    }
    nx_arch_irq_restore(saved);
    return valid ? NX_SUCCESS : NX_ERROR_IO;
}

void nx_stm32g474_exti_irq(nx_stm32g474_exti_state_t* p) {
    if (p == NULL || !p->initialized || p->closing)
        return;
    uint32_t bit = 1U << p->line;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if ((NX_G474_READ(&p->registers->PR1) & NX_G474_READ(&p->registers->IMR1) &
         bit) == 0U) {
        nx_arch_irq_restore(saved);
        return;
    }
    /* G4 PR1 is W1C. A new edge may reassert it immediately: never poll or
     * classify that valid reassertion as a fault. One invocation is bounded. */
    NX_G474_WRITE(&p->registers->PR1, bit);
    nx_arch_dsb();
    if (p->count == p->capacity || p->loss) {
        p->loss = true;
    } else {
        nx_exti_edge_t edge = p->edge;
        if (edge == NX_EXTI_BOTH)
            edge = (NX_G474_READ(&p->gpio->IDR) & bit) != 0U ? NX_EXTI_RISING
                                                             : NX_EXTI_FALLING;
        p->storage[p->head] =
            (nx_exti_event_t){.timestamp_us = nx_time_now_us(),
                              .flags = NX_EXTI_EVENT_COALESCED,
                              .line = p->line,
                              .edge = edge};
        if (++p->head == p->capacity)
            p->head = 0U;
        ++p->count;
    }
    const nx_irq_wake_t* wake = p->wake;
    if (wake != NULL)
        ++p->wake_publishers;
    nx_arch_irq_restore(saved);
    if (wake != NULL) {
        (void)nx_irq_wake_signal(wake);
        saved = nx_arch_irq_save();
        --p->wake_publishers;
        nx_arch_irq_restore(saved);
    }
}

static nx_result_t read_events(void* context, nx_exti_event_t* events,
                               size_t capacity, size_t* count) {
    nx_stm32g474_exti_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || capacity == 0U || capacity > SIZE_MAX / sizeof(*events) ||
        (uintptr_t)events % _Alignof(nx_exti_event_t) != 0U ||
        (uintptr_t)count % _Alignof(size_t) != 0U ||
        output_alias(p, events, capacity * sizeof(*events)) ||
        output_alias(p, count, sizeof(*count)) ||
        overlap(events, capacity * sizeof(*events), count, sizeof(*count)))
        return NX_ERROR_INVALID;
    if (!p->initialized || p->closing)
        return NX_ERROR_STATE;
    *count = 0U;
    while (*count < capacity) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        if (p->count == 0U) {
            bool lost = p->loss;
            p->loss = false;
            nx_arch_irq_restore(saved);
            if (lost)
                events[(*count)++] =
                    (nx_exti_event_t){.timestamp_us = nx_time_now_us(),
                                      .flags = NX_EXTI_EVENT_LOSS,
                                      .line = p->line,
                                      .edge = p->edge};
            break;
        }
        nx_exti_event_t event = p->storage[p->tail];
        if (++p->tail == p->capacity)
            p->tail = 0U;
        --p->count;
        nx_arch_irq_restore(saved);
        events[(*count)++] = event;
    }
    return *count == 0U ? NX_ERROR_EMPTY : NX_SUCCESS;
}

static nx_result_t stop(void* context) {
    nx_stm32g474_exti_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->line > 1U || p->registers == NULL || p->mux == NULL)
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (p->wake_publishers != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    p->closing = true;
    uint32_t bit = 1U << p->line;
    /* Try every independent source and vector withdrawal. A refused leaf
     * retains the ring, wake and saved mux; no partial success releases them.
     */
    bool valid = write_checked(&p->registers->IMR1,
                               NX_G474_READ(&p->registers->IMR1) & ~bit, bit);
    valid = write_checked(&p->registers->RTSR1,
                          NX_G474_READ(&p->registers->RTSR1) & ~bit, bit) &&
            valid;
    valid = write_checked(&p->registers->FTSR1,
                          NX_G474_READ(&p->registers->FTSR1) & ~bit, bit) &&
            valid;
    NX_G474_WRITE(&p->registers->PR1, bit);
    nx_arch_dsb();
    valid = (NX_G474_READ(&p->registers->PR1) & bit) == 0U && valid;
    valid = NX_G474_IRQ_CONTROL(vector(p), false, p->priority) == NX_SUCCESS &&
            valid;
    valid = NX_G474_IRQ_AVAILABLE(vector(p)) && valid;
    if (valid) {
        uint32_t shift = (uint32_t)p->line * 4U;
        valid =
            write_checked(&p->mux->EXTICR[0],
                          (NX_G474_READ(&p->mux->EXTICR[0]) & ~(15U << shift)) |
                              ((uint32_t)p->saved_mux << shift),
                          15U << shift);
    }
    if (valid) {
        p->initialized = false;
        p->retained = false;
        p->wake = NULL;
        p->count = 0U;
        p->loss = false;
    }
    nx_arch_irq_restore(saved);
    return valid ? NX_SUCCESS : NX_ERROR_IO;
}

static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_stm32g474_exti_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL)
        return NX_ERROR_INVALID;
    if (wake != NULL && wake != p->wake && output_alias(p, wake, sizeof(*wake)))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (!p->initialized || p->closing)
        result = NX_ERROR_STATE;
    else if (p->wake_publishers != 0U)
        result = NX_ERROR_BUSY;
    else {
        nx_irq_policy_t policy = p->irq_policy;
        if (wake != NULL && wake->calls_kernel) {
            if (policy.kernel == NX_IRQ_KERNEL_BASEPRI &&
                ceiling > policy.syscall_ceiling)
                policy.syscall_ceiling = ceiling;
            else if (policy.kernel == NX_IRQ_KERNEL_PRIMASK && ceiling != 0U)
                result = NX_ERROR_PERMISSION;
        }
        nx_irq_source_t source = {(int16_t)vector(p),
                                  (uint8_t)NX_G474_IRQ_PRIORITY(vector(p)),
                                  (uint8_t)NX_G474_IRQ_GROUP()};
        if (result == NX_SUCCESS)
            result = nx_irq_wake_validate(wake, &policy, &source);
        if (result == NX_SUCCESS)
            p->wake = wake;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief All exact-line instances share this immutable dispatch table. */
const nx_exti_ops_t nx_stm32g474_exti_ops = {
    .read = read_events,
    .stop = stop,
    .attach_wake = attach_wake,
};
