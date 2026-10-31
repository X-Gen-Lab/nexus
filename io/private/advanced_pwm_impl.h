/**
 * \file            advanced_pwm_impl.h
 * \brief           Compile-time specialized complementary PWM and break
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "timer_advanced_controller.h"
/* One specialization per translation unit. The wrapper owns all required
 * NX bindings, including exports and post-include cold helpers. This template
 * removes only its own helper/default macros; caller-supplied ports survive. */
#if !defined(NX_ADV_CR1) || !defined(NX_ADV_CR2) || !defined(NX_ADV_SMCR) ||   \
    !defined(NX_ADV_RCR) || !defined(NX_ADV_PSC) || !defined(NX_ADV_ARR) ||    \
    !defined(NX_ADV_CCR) || !defined(NX_ADV_CCMR) || !defined(NX_ADV_CCER) ||  \
    !defined(NX_ADV_DIER) || !defined(NX_ADV_EGR) || !defined(NX_ADV_SR) ||    \
    !defined(NX_ADV_BDTR) || !defined(NX_ADV_INITIALIZE) ||                    \
    !defined(NX_ADV_IRQ) || !defined(NX_ADV_RELEASE) ||                        \
    !defined(NX_ADV_OPS) || !defined(NX_ADV_HZ) ||                             \
    !defined(NX_ADV_CLOCK_READY) || !defined(NX_ADV_INPUT_VALID)
#error                                                                         \
    "Advanced PWM implementation requires one complete concrete specialization"
#endif
#ifndef NX_TIMER_WRITE
#define NX_ADV_DEFAULT_TIMER_WRITE 1
#define NX_TIMER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
/** \brief Task controls never borrow CPU privilege from an interface lookup. */
static bool advanced_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr();
}
/** \brief Reject control/config/output overlap before mutable state writes. */
static bool advanced_overlap(const void* a, size_t an, const void* b,
                             size_t bn) {
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (x < y + bn && y < x + an);
}
/** \brief Decode exact tDTS ticks; the timer PSC does not participate. */
static uint32_t deadtime_ticks(uint32_t code) {
    if (code < 128U) {
        return code;
    }
    if (code < 192U) {
        return (64U + (code & 63U)) * 2U;
    }
    if (code < 224U) {
        return (32U + (code & 31U)) * 8U;
    }
    return (32U + (code & 31U)) * 16U;
}
/** \brief A cold bounded search rejects counts the hardware cannot represent.
 */
static bool deadtime_code(uint32_t ticks, uint32_t* code) {
    for (uint32_t candidate = 0U; candidate <= 255U; ++candidate) {
        if (deadtime_ticks(candidate) == ticks) {
            *code = candidate;
            return true;
        }
    }
    return false;
}
/** \brief Read the reviewed physical BKIN level, never a software guess. */
static bool break_active(const nx_advanced_pwm_controller_t* port) {
    return ((*port->config->break_input & port->config->break_mask) != 0U) ==
           port->config->break_active_high;
}
/** \brief Snapshot observed break before clearing only that W0C flag. */
static void observe_break(nx_advanced_pwm_controller_t* port) {
    if ((NX_ADV_SR & 128U) != 0U || break_active(port)) {
        port->fault = true;
        NX_TIMER_WRITE(NX_ADV_BDTR, NX_ADV_BDTR & ~32768U);
        NX_TIMER_WRITE(NX_ADV_SR, ~128U);
        nx_arch_dsb();
    }
}
/** \brief Reject unknown clocks, unsafe break input and irreversible lock. */
nx_result_t NX_ADV_INITIALIZE(nx_advanced_pwm_controller_t* port,
                              const nx_advanced_pwm_config_t* config) {
    uint32_t code = 0U;
    if (port == NULL || config == NULL ||
        advanced_overlap(port, sizeof(*port), config, sizeof(*config)) ||
        config->break_input == NULL || config->break_mask != 4096U ||
        !NX_ADV_INPUT_VALID(config->break_input) ||
        config->timer_hz != NX_ADV_HZ || config->tick_hz == 0U ||
        config->timer_hz % config->tick_hz != 0U ||
        config->timer_hz / config->tick_hz > 65536U ||
        config->period_ticks == 0U || config->period_ticks > 65535U ||
        !deadtime_code(config->deadtime_ticks, &code)) {
        return NX_ERROR_INVALID;
    }
    if (!advanced_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!config->exclusive) {
        return NX_ERROR_PERMISSION;
    }
    if (port->config != NULL || port->initialized || (NX_ADV_CR1 & 1U) != 0U ||
        NX_ADV_CCER != 0U || NX_ADV_DIER != 0U ||
        (NX_ADV_BDTR & (32768U | 768U)) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (!NX_ADV_CLOCK_READY()) {
        return NX_ERROR_STATE;
    }
    uint32_t protection =
        code | 1024U | 2048U | 4096U | (config->break_active_high ? 8192U : 0U);
    uint32_t idle = (config->idle_high ? 256U : 0U) |
                    (config->idle_complement_high ? 512U : 0U);
    /* The pointer is an acquired-session marker, not a ready interface.
     * Failed cold readback must preserve exact owner cleanup responsibility. */
    port->config = config;
    NX_TIMER_WRITE(NX_ADV_CR1, 128U);
    NX_TIMER_WRITE(NX_ADV_SMCR, 0U);
    NX_TIMER_WRITE(NX_ADV_RCR, 0U);
    NX_TIMER_WRITE(NX_ADV_CR2, idle);
    NX_TIMER_WRITE(NX_ADV_PSC, config->timer_hz / config->tick_hz - 1U);
    NX_TIMER_WRITE(NX_ADV_ARR, config->period_ticks - 1U);
    NX_TIMER_WRITE(NX_ADV_CCR, 0U);
    NX_TIMER_WRITE(NX_ADV_CCMR, 104U);
    NX_TIMER_WRITE(NX_ADV_BDTR, protection);
    NX_TIMER_WRITE(NX_ADV_EGR, 1U);
    NX_TIMER_WRITE(NX_ADV_SR, ~128U);
    nx_arch_dsb();
    if (NX_ADV_CR1 != 128U || NX_ADV_CR2 != idle || NX_ADV_SMCR != 0U ||
        NX_ADV_RCR != 0U ||
        NX_ADV_PSC != config->timer_hz / config->tick_hz - 1U ||
        NX_ADV_ARR != config->period_ticks - 1U || NX_ADV_CCMR != 104U ||
        NX_ADV_CCR != 0U || NX_ADV_BDTR != protection) {
        return NX_ERROR_IO;
    }
    *port =
        (nx_advanced_pwm_controller_t){.config = config, .initialized = true};
    observe_break(port);
    return NX_SUCCESS;
}
/** \brief No mutable PSC/ARR is admitted through the hot duty control. */
static nx_result_t advanced_duty(void* context, uint32_t ticks) {
    nx_advanced_pwm_controller_t* port = context;
    if (port == NULL || !port->initialized ||
        ticks > port->config->period_ticks) {
        return NX_ERROR_INVALID;
    }
    if (!advanced_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (NX_ADV_ARR != port->config->period_ticks - 1U ||
        NX_ADV_PSC != port->config->timer_hz / port->config->tick_hz - 1U) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    NX_TIMER_WRITE(NX_ADV_CCR, ticks);
    nx_arch_dsb();
    if (NX_ADV_CCR != ticks) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_IO;
    }
    port->duty_ticks = ticks;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Never reenables a latched break; actual MOE readback is required. */
static nx_result_t advanced_start(void* context) {
    nx_advanced_pwm_controller_t* port = context;
    if (port == NULL || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (!advanced_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    observe_break(port);
    uint32_t idle = (port->config->idle_high ? 256U : 0U) |
                    (port->config->idle_complement_high ? 512U : 0U);
    uint32_t protection =
        1024U | 2048U | 4096U | (port->config->break_active_high ? 8192U : 0U);
    if (port->fault || !NX_ADV_CLOCK_READY() ||
        NX_ADV_ARR != port->config->period_ticks - 1U ||
        NX_ADV_PSC != port->config->timer_hz / port->config->tick_hz - 1U ||
        (NX_ADV_CR1 & ~1U) != 128U || NX_ADV_CR2 != idle || NX_ADV_SMCR != 0U ||
        NX_ADV_RCR != 0U || NX_ADV_CCMR != 104U ||
        NX_ADV_CCR != port->duty_ticks ||
        (NX_ADV_BDTR & ~(32768U | 255U)) != protection ||
        deadtime_ticks(NX_ADV_BDTR & 255U) != port->config->deadtime_ticks) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_STATE;
    }
    if ((NX_ADV_BDTR & 32768U) != 0U || NX_ADV_CCER != 0U ||
        NX_ADV_DIER != 0U || (NX_ADV_CR1 & 1U) != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    NX_TIMER_WRITE(NX_ADV_EGR, 1U);
    NX_TIMER_WRITE(NX_ADV_CCER, 5U);
    NX_TIMER_WRITE(NX_ADV_DIER, NX_ADV_DIER | 128U);
    NX_TIMER_WRITE(NX_ADV_CR1, NX_ADV_CR1 | 1U);
    nx_arch_dsb();
    /* A pulse during setup may already be inactive while its BIF is pending.
     * Never explicitly raise MOE after observing that preceding break. */
    observe_break(port);
    if (port->fault) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_IO;
    }
    NX_TIMER_WRITE(NX_ADV_BDTR, NX_ADV_BDTR | 32768U);
    nx_arch_dsb();
    observe_break(port);
    if (port->fault || (NX_ADV_BDTR & 32768U) == 0U ||
        (NX_ADV_CCER & 5U) != 5U || (NX_ADV_CR1 & 1U) == 0U ||
        (NX_ADV_DIER & 128U) == 0U) {
        NX_TIMER_WRITE(NX_ADV_BDTR, NX_ADV_BDTR & ~32768U);
        nx_arch_irq_restore(mask);
        return NX_ERROR_IO;
    }
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Disable/read back both electrical source channels before release. */
static nx_result_t advanced_stop(void* context) {
    nx_advanced_pwm_controller_t* port = context;
    if (port == NULL || port->config == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!advanced_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    observe_break(port);
    bool cold = !port->initialized;
    NX_TIMER_WRITE(NX_ADV_BDTR, NX_ADV_BDTR & ~32768U);
    NX_TIMER_WRITE(NX_ADV_CCER, cold ? 0U : NX_ADV_CCER & ~5U);
    NX_TIMER_WRITE(NX_ADV_DIER, cold ? 0U : NX_ADV_DIER & ~128U);
    NX_TIMER_WRITE(NX_ADV_CR1, NX_ADV_CR1 & ~1U);
    nx_arch_dsb();
    bool retained = (NX_ADV_BDTR & 32768U) != 0U ||
                    (NX_ADV_CCER & (cold ? UINT32_MAX : 5U)) != 0U ||
                    (NX_ADV_DIER & (cold ? UINT32_MAX : 128U)) != 0U ||
                    (NX_ADV_CR1 & 1U) != 0U;
    nx_arch_irq_restore(mask);
    return retained ? NX_ERROR_BUSY : NX_SUCCESS;
}
/** \brief End the static session after source and shared publisher detach. */
nx_result_t NX_ADV_RELEASE(nx_advanced_pwm_controller_t* port) {
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!advanced_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->config == NULL) {
        return !port->initialized && port->duty_ticks == 0U && !port->fault
                   ? NX_SUCCESS
                   : NX_ERROR_STATE;
    }
    nx_result_t result = advanced_stop(port);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    NX_TIMER_WRITE(NX_ADV_DIER, 0U);
    NX_TIMER_WRITE(NX_ADV_CCER, 0U);
    nx_arch_dsb();
    if (NX_ADV_DIER != 0U || NX_ADV_CCER != 0U || (NX_ADV_CR1 & 1U) != 0U ||
        (NX_ADV_BDTR & 32768U) != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    *port = (nx_advanced_pwm_controller_t){0};
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Explicit recovery checks both asynchronous break and master enable.
 */
static nx_result_t advanced_clear(void* context) {
    nx_advanced_pwm_controller_t* port = context;
    if (port == NULL || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (!advanced_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if ((NX_ADV_BDTR & 32768U) != 0U || (NX_ADV_CCER & 5U) != 0U ||
        (NX_ADV_CR1 & 1U) != 0U || break_active(port)) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    NX_TIMER_WRITE(NX_ADV_SR, ~128U);
    nx_arch_dsb();
    if ((NX_ADV_SR & 128U) != 0U || break_active(port)) {
        port->fault = true;
        nx_arch_irq_restore(mask);
        return NX_ERROR_IO;
    }
    port->fault = false;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Report actual master enable rather than a cached running boolean. */
static nx_result_t advanced_state(const void* context,
                                  nx_advanced_pwm_state_t* state) {
    const nx_advanced_pwm_controller_t* port = context;
    if (port == NULL || !port->initialized || state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (advanced_overlap(state, sizeof(*state), port, sizeof(*port)) ||
        advanced_overlap(state, sizeof(*state), port->config,
                         sizeof(*port->config)) ||
        advanced_overlap(state, sizeof(*state), &NX_ADV_OPS,
                         sizeof(NX_ADV_OPS)) ||
        advanced_overlap(state, sizeof(*state),
                         (const void*)port->config->break_input,
                         sizeof(*port->config->break_input))) {
        return NX_ERROR_INVALID;
    }
    if (!advanced_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    *state = (nx_advanced_pwm_state_t){
        .period_ticks = port->config->period_ticks,
        .duty_ticks = port->duty_ticks,
        .tick_hz = port->config->tick_hz,
        .deadtime_ticks = port->config->deadtime_ticks,
        .deadtime_hz = port->config->timer_hz,
        .running = (NX_ADV_BDTR & 32768U) != 0U,
        .fault = port->fault || (NX_ADV_SR & 128U) != 0U || break_active(port)};
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Shared-vector dispatch supplies exactly this static controller. */
void NX_ADV_IRQ(nx_advanced_pwm_controller_t* port) {
    if (port != NULL && port->initialized) {
        observe_break(port);
    }
}
/** \brief Optional five-control table; the default PWM ABI is unaffected. */
const nx_advanced_pwm_ops_t NX_ADV_OPS = {.set_duty = advanced_duty,
                                          .start = advanced_start,
                                          .stop = advanced_stop,
                                          .state = advanced_state,
                                          .clear_fault = advanced_clear};

/* The specialization bindings remain owned by the concrete wrapper. */
#ifdef NX_ADV_DEFAULT_TIMER_WRITE
#undef NX_TIMER_WRITE
#undef NX_ADV_DEFAULT_TIMER_WRITE
#endif
