/**
 * \file            stm32f407_timer_output.h
 * \brief           Shared F407 output withdrawal, break and deadtime core
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_TIMER_OUTPUT_H
#define NX_STM32F407_TIMER_OUTPUT_H
#include "nexus/arch/arch.h"
#include "stm32f4xx.h"
#ifndef NX_TIMER_WRITE
#define NX_TIMER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#ifndef NX_TIMER_READ
#define NX_TIMER_READ(reg) (reg)
#endif

/** \brief Decode tDTS ticks without applying the counter prescaler. */
static inline uint32_t nx_stm32_timer_deadtime_ticks(uint32_t code) {
    if (code < 128U)
        return code;
    if (code < 192U)
        return (64U + (code & 63U)) * 2U;
    if (code < 224U)
        return (32U + (code & 31U)) * 8U;
    return (32U + (code & 31U)) * 16U;
}
/** \brief Reject an unrepresentable delay instead of rounding protection. */
static inline bool nx_stm32_timer_deadtime_code(uint32_t ticks,
                                                uint32_t* code) {
    for (uint32_t candidate = 0U; candidate <= 255U; ++candidate)
        if (nx_stm32_timer_deadtime_ticks(candidate) == ticks) {
            *code = candidate;
            return true;
        }
    return false;
}
/** \brief Observe the actual configured BKIN polarity at the GPIO input. */
static inline bool nx_stm32_timer_break_active(const volatile uint32_t* input,
                                               uint32_t mask, bool high) {
    return ((*input & mask) != 0U) == high;
}
/** \brief Latch before acknowledging BIF; observing a fault only removes MOE.
 */
static inline bool nx_stm32_timer_break_observe(TIM_TypeDef* timer,
                                                const volatile uint32_t* input,
                                                uint32_t mask, bool high,
                                                bool* fault) {
    if ((NX_TIMER_READ(timer->SR) & TIM_SR_BIF) == 0U &&
        !nx_stm32_timer_break_active(input, mask, high))
        return false;
    *fault = true;
    NX_TIMER_WRITE(timer->BDTR,
                   NX_TIMER_READ(timer->BDTR) & ~(uint32_t)TIM_BDTR_MOE);
    NX_TIMER_WRITE(timer->SR, ~(uint32_t)TIM_SR_BIF);
    nx_arch_dsb();
    return true;
}
/** \brief Withdraw outputs before clearing the counter; no automatic restart.
 */
static inline bool nx_stm32_timer_output_stop(TIM_TypeDef* timer, bool advanced,
                                              uint32_t channels,
                                              uint32_t sources) {
    if (advanced)
        NX_TIMER_WRITE(timer->BDTR,
                       NX_TIMER_READ(timer->BDTR) & ~(uint32_t)TIM_BDTR_MOE);
    NX_TIMER_WRITE(timer->CCER, NX_TIMER_READ(timer->CCER) & ~channels);
    NX_TIMER_WRITE(timer->DIER, NX_TIMER_READ(timer->DIER) & ~sources);
    NX_TIMER_WRITE(timer->CR1,
                   NX_TIMER_READ(timer->CR1) & ~(uint32_t)TIM_CR1_CEN);
    nx_arch_dsb();
    return (!advanced || (NX_TIMER_READ(timer->BDTR) & TIM_BDTR_MOE) == 0U) &&
           (NX_TIMER_READ(timer->DIER) & sources) == 0U &&
           (NX_TIMER_READ(timer->CCER) & channels) == 0U &&
           (NX_TIMER_READ(timer->CR1) & TIM_CR1_CEN) == 0U;
}
#endif /* NX_STM32F407_TIMER_OUTPUT_H */
