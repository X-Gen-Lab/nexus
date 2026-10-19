/**
 * \file            lptim.c
 * \brief           Exact U575 LPTIM2 finite RUN one-shot state machine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32u575_lptim.h"
/* Bind the reviewed U575 full LPTIM generation and exact LPTIM2 vector. */
_Static_assert(LPTIM2_IRQn == 68 && LPTIM_ISR_UE == (1U << 7U) &&
                   offsetof(LPTIM_TypeDef, RCR) == 0x28U &&
                   offsetof(LPTIM_TypeDef, CCMR1) == 0x2cU &&
                   offsetof(LPTIM_TypeDef, CCR2) == 0x34U,
               "U575 LPTIM2 generation differs from the reviewed register IP");
#ifndef NX_U575_LPTIM2
#define NX_U575_LPTIM2 LPTIM2
#endif
#ifndef NX_U575_LPTIM_NOW
#define NX_U575_LPTIM_NOW() nx_time_now_us()
#endif
#ifndef NX_U575_LPTIM_EXCEPTION
#define NX_U575_LPTIM_EXCEPTION() nx_arch_exception_number()
#endif
#ifndef NX_U575_LPTIM_IRQ_ENABLED
#define NX_U575_LPTIM_IRQ_ENABLED()       (NVIC_GetEnableIRQ(LPTIM2_IRQn) != 0U)
#define NX_U575_LPTIM_IRQ_PENDING()       (NVIC_GetPendingIRQ(LPTIM2_IRQn) != 0U)
#define NX_U575_LPTIM_IRQ_ACTIVE()        (NVIC_GetActive(LPTIM2_IRQn) != 0U)
#define NX_U575_LPTIM_IRQ_PRIORITY()      NVIC_GetPriority(LPTIM2_IRQn)
#define NX_U575_LPTIM_IRQ_SET_PRIORITY(p) NVIC_SetPriority(LPTIM2_IRQn, (p))
#define NX_U575_LPTIM_IRQ_ENABLE()        NVIC_EnableIRQ(LPTIM2_IRQn)
#define NX_U575_LPTIM_IRQ_DISABLE()       NVIC_DisableIRQ(LPTIM2_IRQn)
#define NX_U575_LPTIM_IRQ_CLEAR()         NVIC_ClearPendingIRQ(LPTIM2_IRQn)
#endif
/* RM0456 Rev7 58.4.8/11/13/14/16 and 58.7: PCLK1/4, immediate updates,
 * CCR1=0, ARR=ticks-1>=1, RCR=0. UE proves the full period, not early ARRM.
 * No external trigger, channel output, DMA or low-power mode is selected. */
enum {
    IDLE,
    ENABLE,
    ENABLE_WAIT,
    ARR,
    ARR_WAIT,
    CMP,
    CMP_WAIT,
    DIER,
    DIER_WAIT,
    LAUNCH,
    RUN,
    WITHDRAW,
    FREEZE,
    RESET
};
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32u575_lptim_state_t* s) {
    return s != NULL && s->registers == NX_U575_LPTIM2 &&
           g_nx_stm32u575_system.rcc != NULL;
}
static bool live(const nx_stm32u575_lptim_state_t* s, bool gate) {
    const nx_stm32u575_system_t* system = &g_nx_stm32u575_system;
    RCC_TypeDef* r = system->rcc;
    uint32_t ranges = NX_U575_READ(&r->ICSCR1);
    uint32_t range =
        (ranges & RCC_ICSCR1_MSIRGSEL) == 0U
            ? (NX_U575_READ(&r->CSR) & RCC_CSR_MSISSRANGE) >>
                  RCC_CSR_MSISSRANGE_Pos
            : (ranges & RCC_ICSCR1_MSISRANGE) >> RCC_ICSCR1_MSISRANGE_Pos;
    return system->started && system->core_hz == 4000000U &&
           system->timer_error == NX_SUCCESS && range == 4U &&
           (NX_U575_READ(&r->CR) & (RCC_CR_MSISON | RCC_CR_MSISRDY)) ==
               (RCC_CR_MSISON | RCC_CR_MSISRDY) &&
           NX_U575_READ(&r->CFGR1) == 0U && NX_U575_READ(&r->CFGR2) == 0U &&
           NX_U575_READ(&r->CFGR3) == 0U &&
           (NX_U575_READ(&r->CCIPR1) & RCC_CCIPR1_LPTIM2SEL) == 0U &&
           ((NX_U575_READ(&r->APB1RSTR2) & RCC_APB1RSTR2_LPTIM2RST) == 0U ||
            s->reset_asserted) &&
           (!gate ||
            (NX_U575_READ(&r->APB1ENR2) & RCC_APB1ENR2_LPTIM2EN) != 0U);
}
static bool checked(volatile uint32_t* reg, uint32_t value) {
    NX_U575_WRITE(reg, value);
    nx_arch_dsb();
    return NX_U575_READ(reg) == value;
}
static bool external(const nx_stm32u575_lptim_state_t* s, const void* p,
                     size_t bytes, size_t alignment) {
    uintptr_t begin = (uintptr_t)p;
    if (p == NULL || begin % alignment != 0U || begin > UINTPTR_MAX - bytes)
        return false;
#define OVERLAP(base, length)                                                  \
    (begin < (uintptr_t)(base) + (length) && (uintptr_t)(base) < begin + bytes)
    bool result =
        !OVERLAP(s, sizeof(*s)) &&
        !OVERLAP(s->registers, sizeof(*s->registers)) &&
        !OVERLAP(&g_nx_stm32u575_system, sizeof(g_nx_stm32u575_system)) &&
        !OVERLAP(g_nx_stm32u575_system.rcc, sizeof(RCC_TypeDef)) &&
        !OVERLAP(g_nx_stm32u575_system.timer, sizeof(TIM_TypeDef)) &&
        !OVERLAP(g_nx_stm32u575_system.flash, sizeof(FLASH_TypeDef)) &&
        !OVERLAP(g_nx_stm32u575_system.icache, sizeof(ICACHE_TypeDef)) &&
        !OVERLAP(g_nx_stm32u575_system.dcache, sizeof(DCACHE_TypeDef));
#undef OVERLAP
    return result;
}
static void fail(nx_stm32u575_lptim_state_t* s, nx_result_t cause) {
    /* IRQ publishes only facts. Serialize the first task cause against those
     * facts, preserving the incoming mask without masking a service phase. */
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->terminal_set) {
        s->terminal = s->tainted ? NX_ERROR_STATE : cause;
        s->terminal_set = true;
    }
    if (s->stage < WITHDRAW)
        s->stage = WITHDRAW;
    if (s->active != NULL)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
    nx_arch_irq_restore(guard);
}
static nx_result_t held(nx_stm32u575_lptim_state_t* s) {
    if (s->active != NULL)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_QUARANTINED);
    return NX_ERROR_BUSY;
}
/** \brief A foreign clock/reset observation cannot be repaired by bit restore.
 */
static void taint(nx_stm32u575_lptim_state_t* s) {
    s->tainted = true;
    fail(s, NX_ERROR_STATE);
    NX_U575_LPTIM_IRQ_DISABLE();
    nx_arch_dsb();
    (void)held(s);
}
static bool counter(const nx_stm32u575_lptim_state_t* s, uint16_t* value) {
    uint32_t previous = NX_U575_READ(&s->registers->CNT) & 65535U;
    for (unsigned n = 0U; n < 8U; ++n) {
        uint32_t current = NX_U575_READ(&s->registers->CNT) & 65535U;
        if (previous == current) {
            *value = (uint16_t)current;
            return true;
        }
        previous = current;
    }
    return false;
}
static nx_result_t sample(const void* raw, nx_lptim_sample_t* out) {
    nx_stm32u575_lptim_state_t* s = (nx_stm32u575_lptim_state_t*)raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s) ||
        !external(s, out, sizeof(*out), _Alignof(nx_lptim_sample_t)))
        return NX_ERROR_INVALID;
    if (!s->opened || s->tainted)
        return NX_ERROR_STATE;
    if (!live(s, true)) {
        if (s->active != NULL)
            taint(s);
        return NX_ERROR_STATE;
    }
    if (s->reset_asserted)
        return NX_ERROR_BUSY;
    if (s->active != NULL &&
        (uintptr_t)out < (uintptr_t)s->active + sizeof(*s->active) &&
        (uintptr_t)s->active < (uintptr_t)out + sizeof(*out))
        return NX_ERROR_INVALID;
    uint16_t count = 0U;
    if (s->launched && !s->reset_asserted && !counter(s, &count))
        return NX_ERROR_BUSY;
    nx_lptim_sample_t value = {
        .domain = {s, s->generation},
        .tick_hz = 1000000U,
        .counter = count,
        .running = s->launched && !s->event &&
                   (NX_U575_READ(&s->registers->CR) & LPTIM_CR_ENABLE) != 0U &&
                   (NX_U575_READ(&s->registers->ISR) & LPTIM_ISR_UE) == 0U};
    *out = value;
    return NX_SUCCESS;
}
static nx_result_t submit(void* raw, nx_lptim_request_t* request) {
    nx_stm32u575_lptim_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s) ||
        !external(s, request, sizeof(*request), _Alignof(nx_lptim_request_t)))
        return NX_ERROR_INVALID;
    if (!s->opened || s->closing || s->tainted)
        return NX_ERROR_STATE;
    if (!live(s, true)) {
        if (s->active != NULL)
            taint(s);
        return NX_ERROR_STATE;
    }
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    if (nx_request_state(&request->base) != NX_REQUEST_READY ||
        request->domain.identity != s ||
        request->domain.generation != s->generation)
        return NX_ERROR_STATE;
    if (request->ticks == 0U || request->ticks > 65536U)
        return NX_ERROR_INVALID;
    if (request->ticks == 1U)
        return NX_ERROR_UNSUPPORTED;
    if (s->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    if (nx_deadline_expired(request->base.deadline, NX_U575_LPTIM_NOW()))
        return NX_ERROR_TIMEOUT;
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        s->active = request;
        s->stage = ENABLE;
        s->launched = s->event = s->terminal_set = false;
        s->progress = 0U;
    }
    return result;
}
static nx_result_t cancel(void* raw, nx_lptim_request_t* request) {
    nx_stm32u575_lptim_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s))
        return NX_ERROR_INVALID;
    if (request == NULL || request != s->active)
        return NX_ERROR_STATE;
    fail(s, NX_ERROR_CANCELLED);
    return NX_SUCCESS;
}
static bool acknowledge(nx_stm32u575_lptim_state_t* s, uint32_t flag) {
    NX_U575_WRITE(&s->registers->ICR, flag);
    nx_arch_dsb();
    return (NX_U575_READ(&s->registers->ISR) & flag) == 0U;
}
static void wait_update(nx_stm32u575_lptim_state_t* s, uint32_t flag) {
    if ((NX_U575_READ(&s->registers->ISR) & flag) != 0U) {
        if (acknowledge(s, flag))
            ++s->stage;
        else
            fail(s, NX_ERROR_IO);
    } else if (--s->polls == 0U)
        fail(s, NX_ERROR_IO);
}
/** \brief Reset is exclusive to this admitted timer, never a shared bus reset.
 */
static bool reset(nx_stm32u575_lptim_state_t* s) {
    RCC_TypeDef* r = g_nx_stm32u575_system.rcc;
    if (!s->reset_asserted) {
        uint32_t asserted =
            NX_U575_READ(&r->APB1RSTR2) | RCC_APB1RSTR2_LPTIM2RST;
        if (!checked(&r->APB1RSTR2, asserted))
            return false;
        s->reset_asserted = true;
    }
    if (!checked(&r->APB1RSTR2, NX_U575_READ(&r->APB1RSTR2) &
                                    ~(uint32_t)RCC_APB1RSTR2_LPTIM2RST))
        return false;
    s->reset_asserted = false;
    return NX_U575_READ(&s->registers->CR) == 0U &&
           NX_U575_READ(&s->registers->DIER) == 0U &&
           NX_U575_READ(&s->registers->ISR) == 0U;
}
static nx_result_t service(void* raw) {
    nx_stm32u575_lptim_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s))
        return NX_ERROR_INVALID;
    if (s->active == NULL)
        return NX_SUCCESS;
    if (s->tainted || !live(s, true)) {
        taint(s);
        return held(s);
    }
    nx_time_us_t now = NX_U575_LPTIM_NOW();
    if (!s->terminal_set && nx_deadline_expired(s->active->base.deadline, now))
        fail(s, NX_ERROR_TIMEOUT);
    LPTIM_TypeDef* p = s->registers;
    bool good = true;
    switch (s->stage) {
        case ENABLE:
            good = checked(&p->CFGR, LPTIM_CFGR_PRESC_1) &&
                   checked(&p->CR, LPTIM_CR_ENABLE);
            /* Start after the checked write: earlier CPU/APB work grants no
             * kernel cycles. Three quanta cover two full1MHz cycles and the
             * unknown phase of this monotonic snapshot. */
            s->ready_at = nx_deadline_after(NX_U575_LPTIM_NOW(), 3U);
            s->stage = ENABLE_WAIT;
            break;
        case ENABLE_WAIT:
            if (nx_deadline_expired(s->ready_at, now))
                ++s->stage;
            break;
        case ARR:
            good = acknowledge(s, LPTIM_ISR_ARROK);
            if (good)
                NX_U575_WRITE(&p->ARR, s->active->ticks - 1U);
            s->polls = 32U;
            ++s->stage;
            break;
        case ARR_WAIT:
            wait_update(s, LPTIM_ISR_ARROK);
            break;
        case CMP:
            good = acknowledge(s, LPTIM_ISR_CMP1OK);
            if (good)
                NX_U575_WRITE(&p->CCR1, 0U);
            s->polls = 32U;
            ++s->stage;
            break;
        case CMP_WAIT:
            wait_update(s, LPTIM_ISR_CMP1OK);
            break;
        case DIER:
            good = acknowledge(s, LPTIM_ISR_DIEROK);
            if (good)
                NX_U575_WRITE(&p->DIER, LPTIM_DIER_UEIE);
            s->polls = 32U;
            ++s->stage;
            break;
        case DIER_WAIT:
            wait_update(s, LPTIM_ISR_DIEROK);
            break;
        case LAUNCH:
            s->priority_owned = true;
            NX_U575_LPTIM_IRQ_SET_PRIORITY(5U);
            NX_U575_LPTIM_IRQ_ENABLE();
            nx_arch_dsb();
            good = NX_U575_LPTIM_IRQ_ENABLED() &&
                   NX_U575_LPTIM_IRQ_PRIORITY() == 5U;
            if (good) {
                s->launched = true;
                NX_U575_WRITE(&p->CR, LPTIM_CR_ENABLE | LPTIM_CR_SNGSTRT);
                nx_arch_dsb();
                s->ready_at = nx_deadline_after(NX_U575_LPTIM_NOW(),
                                                s->active->ticks + 10U);
                ++s->stage;
            }
            break;
        case RUN:
            if (s->event || (NX_U575_READ(&p->ISR) & LPTIM_ISR_UE) != 0U)
                fail(s, NX_SUCCESS);
            else if (nx_deadline_expired(s->ready_at, now))
                fail(s, NX_ERROR_IO);
            break;
        case WITHDRAW:
            NX_U575_LPTIM_IRQ_DISABLE();
            nx_arch_dsb();
            nx_arch_isb();
            if (NX_U575_LPTIM_IRQ_ENABLED() || !checked(&p->CR, 0U))
                return held(s);
            /* Allow the kernel disable to cross its clock boundary before
             * stable CNT sampling. No Stop-mode entry occurs in this mode. */
            s->ready_at = nx_deadline_after(NX_U575_LPTIM_NOW(), 3U);
            ++s->stage;
            break;
        case FREEZE: {
            if (!nx_deadline_expired(s->ready_at, now))
                break;
            uint16_t count = 0U;
            if (!counter(s, &count))
                return held(s);
            s->progress = s->launched ? count : 0U;
            if (s->launched &&
                (s->event || (NX_U575_READ(&p->ISR) & LPTIM_ISR_UE) != 0U))
                s->progress = s->active->ticks;
            ++s->stage;
            break;
        }
        case RESET:
            if (!reset(s))
                return held(s);
            NX_U575_LPTIM_IRQ_CLEAR();
            nx_arch_dsb();
            nx_arch_isb();
            if (NX_U575_LPTIM_IRQ_PENDING() || NX_U575_LPTIM_IRQ_ACTIVE())
                return held(s);
            {
                nx_lptim_request_t* request = s->active;
                nx_result_t result = s->terminal;
                uint32_t progress = s->progress;
                s->active = NULL;
                s->stage = IDLE;
                s->launched = s->event = s->terminal_set = false;
                ++s->generation;
                nx_request_settle(&request->base, result, progress);
            }
            return NX_SUCCESS;
        default:
            fail(s, NX_ERROR_IO);
            break;
    }
    if (!good)
        fail(s, NX_ERROR_IO);
    return NX_ERROR_BUSY;
}
static nx_result_t stop(void* raw) {
    nx_stm32u575_lptim_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s))
        return NX_ERROR_INVALID;
    s->closing = true;
    if (s->active != NULL) {
        fail(s, NX_ERROR_CANCELLED);
        (void)service(s);
        if (s->active != NULL)
            return NX_ERROR_BUSY;
    }
    if (!s->retained)
        return NX_SUCCESS;
    if (s->tainted)
        return NX_ERROR_BUSY;
    if (s->priority_owned) {
        NX_U575_LPTIM_IRQ_SET_PRIORITY(s->saved_priority);
        if (NX_U575_LPTIM_IRQ_PRIORITY() != s->saved_priority)
            return NX_ERROR_BUSY;
        s->priority_owned = false;
    }
    RCC_TypeDef* r = g_nx_stm32u575_system.rcc;
    if (s->gate_added) {
        if (!checked(&r->APB1ENR2, NX_U575_READ(&r->APB1ENR2) &
                                       ~(uint32_t)RCC_APB1ENR2_LPTIM2EN))
            return NX_ERROR_BUSY;
        s->gate_added = false;
    }
    s->opened = s->retained = false;
    return NX_SUCCESS;
}
const nx_lptim_ops_t nx_stm32u575_lptim_ops = {sample, submit, cancel, service,
                                               stop};
nx_result_t nx_stm32u575_lptim_initialize(nx_stm32u575_lptim_state_t* s) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s))
        return NX_ERROR_INVALID;
    if (s->retained || s->active != NULL)
        return NX_ERROR_BUSY;
    if (s->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    if (!live(s, false))
        return NX_ERROR_STATE;
    if (NX_U575_LPTIM_IRQ_ENABLED() || NX_U575_LPTIM_IRQ_PENDING() ||
        NX_U575_LPTIM_IRQ_ACTIVE())
        return NX_ERROR_BUSY;
    RCC_TypeDef* r = g_nx_stm32u575_system.rcc;
    s->gate_added = (NX_U575_READ(&r->APB1ENR2) & RCC_APB1ENR2_LPTIM2EN) == 0U;
    s->retained = true;
    if (s->gate_added && !checked(&r->APB1ENR2, NX_U575_READ(&r->APB1ENR2) |
                                                    RCC_APB1ENR2_LPTIM2EN))
        return NX_ERROR_IO;
    LPTIM_TypeDef* p = s->registers;
    if (NX_U575_READ(&p->CR) != 0U || NX_U575_READ(&p->CFGR) != 0U ||
        NX_U575_READ(&p->CFGR2) != 0U || NX_U575_READ(&p->DIER) != 0U ||
        NX_U575_READ(&p->ISR) != 0U || NX_U575_READ(&p->CCMR1) != 0U ||
        NX_U575_READ(&p->RCR) != 0U || NX_U575_READ(&p->CNT) != 0U ||
        NX_U575_READ(&p->ARR) != 1U || NX_U575_READ(&p->CCR1) != 0U ||
        NX_U575_READ(&p->CCR2) != 0U)
        return NX_ERROR_BUSY;
    s->saved_priority = (uint8_t)NX_U575_LPTIM_IRQ_PRIORITY();
    s->opened = true;
    s->closing = s->tainted = s->reset_asserted = false;
    s->stage = IDLE;
    ++s->generation;
    return NX_SUCCESS;
}
void nx_stm32u575_lptim_irq(nx_stm32u575_lptim_state_t* s) {
    if (NX_U575_LPTIM_EXCEPTION() != 84U || !valid(s) || s->active == NULL ||
        !s->retained)
        return;
    if (!live(s, true)) {
        s->tainted = true;
    } else if ((NX_U575_READ(&s->registers->ISR) & LPTIM_ISR_UE) != 0U)
        s->event = true;
    NX_U575_LPTIM_IRQ_DISABLE();
    nx_arch_dsb();
    nx_arch_isb();
    if (NX_U575_LPTIM_IRQ_ENABLED()) {
        (void)nx_arch_irq_save();
        nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
        for (;;) {
        }
    }
}
