/**
 * \file            stm32g474_system.h
 * \brief           Private fixed G474 clock ownership and independent TIM2 time
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32G474_SYSTEM_H
#define NX_STM32G474_SYSTEM_H
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#include "stm32g474xx.h"
#ifndef NX_G474_IRQ_AVAILABLE
/* System and generated cold code share one hardware admission boundary. */
#define NX_G474_IRQ_AVAILABLE(irq)                                             \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) == 0U &&                              \
     NVIC_GetPendingIRQ((IRQn_Type)(irq)) == 0U &&                             \
     NVIC_GetActive((IRQn_Type)(irq)) == 0U)
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define NX_STM32G474_EFFECT_CLOCK 1U
#define NX_STM32G474_EFFECT_TIMER 2U
typedef struct nx_stm32g474_system {
    RCC_TypeDef* rcc;
    FLASH_TypeDef* flash;
    PWR_TypeDef* power;
    TIM_TypeDef* timer;
    uint64_t overflow_us;
    uint32_t core_hz;
    volatile uint32_t remaining_effects;
    uint32_t reset_cause;
    uint32_t saved_pll;
    uint32_t saved_flash;
    uint32_t saved_power;
    uint32_t saved_boost;
    /** \brief Terminal IO diagnostic; never permission to resume or reclaim.
     */
    volatile nx_result_t timer_error;
    bool owned_power_clock;
    bool power_snapshotted;
    bool timer_quiet;
    volatile bool started;
} nx_stm32g474_system_t;
typedef struct {
    nx_result_t primary;
    nx_result_t cleanup;
    uint32_t remaining_effects;
} nx_stm32g474_start_result_t;
extern nx_stm32g474_system_t g_nx_stm32g474_system;
/**
 * \brief Read-only reset-like clock, Flash and reserved TIM2 admission.
 * \param[in] system: Static privileged cold context with no external owners.
 * \return SUCCESS or INVALID/CONTEXT/BUSY before any hardware changes.
 * \note Unknown running profiles and bootloader handoff are not adopted.
 */
nx_result_t nx_stm32g474_system_preflight(const nx_stm32g474_system_t* system);
/**
 * \brief Select only HSE24/PLLM6/N85/R2 with a bounded boot transaction.
 * \param[in,out] system: Static privileged bootstrap context; no consumers.
 * \param[in] poll_limit: Nonzero maximum reads per hardware phase.
 * \return Primary/cleanup errors and resources still retained after failure.
 * \note Requires reset-like HSI16 selected, idle PLL/HSE and unused TIM2.
 *       Unknown clocks fail admission without writes. No elapsed-time promise
 *       is inferred from poll count. Clock rollback precedes voltage changes.
 */
nx_stm32g474_start_result_t
nx_stm32g474_system_start(nx_stm32g474_system_t* system, uint32_t poll_limit);
/**
 * \brief Withdraw TIM2/IRQ before restoring HSI16 and owned clock fields.
 * \param[in,out] system: All peripheral/task consumers already joined.
 * \param[in] poll_limit: Positive bounded phase budget.
 * \return Success after actual readback; failure retains remaining effects.
 */
nx_result_t nx_stm32g474_system_stop(nx_stm32g474_system_t* system,
                                     uint32_t poll_limit);
/**
 * \brief Snapshot independent 1MHz TIM2 with one pending wrap correction.
 * \param[in,out] system: Live context, privileged task/configurable IRQ.
 * \return Extended microseconds; zero outside a successfully started epoch.
 * \note No NMI/HardFault, dynamic clock or deep-sleep continuity contract.
 *       Overflow IRQ must run at least once every 2^32 microseconds. Observed
 *       acknowledgement failure ends the epoch and invalidates old deadlines.
 *       Cleanup/restart never recovers its elapsed time; zero is not readiness.
 */
uint64_t nx_stm32g474_system_now(nx_stm32g474_system_t* system);
/**
 * \brief Bounded reserved-vector overflow handler; no kernel calls.
 * \param[in,out] system: Static context bound to the reserved vector.
 * \note Confirm W0C before extension. Failure latches IO and attempts each
 *       DIER/CEN/NVIC isolation once; timer/clock ownership remains retained.
 *       The lost time domain is always terminal, including observed isolation.
 *       Retain the IRQ mask and every owner, observe the platform fault and
 *       park if the observer returns. No old deadline consumer resumes.
 */
void nx_stm32g474_system_timer_irq(nx_stm32g474_system_t* system);
/** \brief Verify observed category3 silicon and512KiB; not package identity. */
nx_result_t nx_stm32g474_validate_identity(void);
/** \brief CPU-only pre-RAM setup; no initialized global memory access. */
void SystemInit(void);
/** \brief Reserved TIM2 vector bound to the one explicit system instance. */
void TIM2_IRQHandler(void);
#ifndef NX_G474_READ
#define NX_G474_READ(reg) (*(reg))
#endif
#ifndef NX_G474_WRITE
#define NX_G474_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_G474_SYSTEM_POLL
#define NX_G474_SYSTEM_POLL(system) ((void)(system))
#endif
#ifndef NX_G474_BOOST_DELAY
#define NX_G474_BOOST_DELAY(system) nx_stm32g474_boost_delay()
static inline void nx_stm32g474_boost_delay(void) {
    uint32_t remaining = 85U;
    /* M4 single issue:85 dependent SUBS+85 branches require>=170 cycles.
     * At verified85MHz this is>=2us; IRQs can only extend the delay. */
    __asm__ volatile("1: subs %0, %0, #1\n bne 1b"
                     : "+r"(remaining)
                     :
                     : "cc", "memory");
}
#endif
#ifndef NX_G474_IRQ_CONTROL
static inline nx_result_t nx_stm32g474_irq_control(int irq, bool enabled,
                                                   uint8_t priority) {
    if (irq < 0 || irq >= 102 || priority >= 16U)
        return NX_ERROR_INVALID;
    if (enabled) {
        NVIC_SetPriority((IRQn_Type)irq, priority);
        NVIC_ClearPendingIRQ((IRQn_Type)irq);
        NVIC_EnableIRQ((IRQn_Type)irq);
    } else {
        NVIC_DisableIRQ((IRQn_Type)irq);
        NVIC_ClearPendingIRQ((IRQn_Type)irq);
    }
    nx_arch_dsb();
    nx_arch_isb();
    if (NVIC_GetEnableIRQ((IRQn_Type)irq) != (enabled ? 1U : 0U) ||
        (enabled && NVIC_GetPriority((IRQn_Type)irq) != priority) ||
        (!enabled && NVIC_GetPendingIRQ((IRQn_Type)irq) != 0U))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
#define NX_G474_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_stm32g474_irq_control((irq), (enabled), (priority))
#endif
#ifndef NX_G474_IRQ_PRIORITY
#define NX_G474_IRQ_PRIORITY(irq) NVIC_GetPriority((IRQn_Type)(irq))
#endif
#ifndef NX_G474_IRQ_PRIORITY_WRITE
/** \brief Restore one exclusively owned logical priority without enabling it.
 */
static inline void nx_stm32g474_irq_priority_write(int irq, uint32_t priority) {
    NVIC_SetPriority((IRQn_Type)irq, priority);
    nx_arch_dsb();
    nx_arch_isb();
}
#define NX_G474_IRQ_PRIORITY_WRITE(irq, priority)                              \
    nx_stm32g474_irq_priority_write((irq), (priority))
#endif
#ifdef __cplusplus
}
#endif
#endif
