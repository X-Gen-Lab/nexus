/**
 * \file            stm32h743_system.h
 * \brief           Cold H743 HSI64/Cache-off ownership and independent TIM2
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_SYSTEM_H
#define NX_STM32H743_SYSTEM_H
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#include "stm32h743xx.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_STM32H743_EFFECT_TIMER 1U
typedef struct nx_stm32h743_system {
    RCC_TypeDef* rcc;
    FLASH_TypeDef* flash;
    PWR_TypeDef* power;
    TIM_TypeDef* timer;
    SCB_Type* core;
    uint64_t overflow_us;
    uint32_t core_hz;
    uint32_t reset_cause;
    uint32_t remaining_effects;
    nx_result_t timer_error;
    bool started;
} nx_stm32h743_system_t;
typedef struct {
    nx_result_t primary;
    nx_result_t cleanup;
    uint32_t remaining_effects;
} nx_stm32h743_start_result_t;
extern nx_stm32h743_system_t g_nx_stm32h743_system;
/**
 * \brief           Read cold HSI64/LDO-ready/Cache-off and TIM2 prerequisites.
 * \param[in]       system: Exact privileged exclusive context after reset RAM.
 * \return          SUCCESS or INVALID/CONTEXT/BUSY without hardware mutation.
 * \note            No running-clock, cache, scheduler or bootloader adoption.
 */
nx_result_t nx_stm32h743_system_preflight(const nx_stm32h743_system_t* system);
/**
 * \brief           Establish the one 1MHz TIM2 on the fixed HSI64 profile.
 * \param[in,out]   system: Cold privileged task context; consumers quiescent.
 * \param[in]       poll_limit: Positive bounded hardware observation budget.
 * \return          Primary/cleanup result and retained timer responsibility.
 * \note            HSI64 and LDO are reset boot prerequisites, not runtime
 *                  acquisitions. Poll counts are not measured elapsed time.
 */
nx_stm32h743_start_result_t
nx_stm32h743_system_start(nx_stm32h743_system_t* system, uint32_t poll_limit);
/**
 * \brief           Withdraw reserved TIM2/IRQ after all consumers have joined.
 * \param[in,out]   system: Exclusive owner; no scheduler or device consumers.
 * \param[in]       poll_limit: Positive bounded hardware observation budget.
 * \return          Success after readback; failures retain effects for retry.
 * \note            HSI/LDO and boot Flash latency remain required CPU state.
 */
nx_result_t nx_stm32h743_system_stop(nx_stm32h743_system_t* system,
                                     uint32_t poll_limit);
/**
 * \brief           Snapshot 1MHz TIM2 in this successful boot time domain.
 * \param[in,out]   system: Live privileged task/configurable IRQ context.
 * \return          Microseconds or zero outside a successfully live epoch.
 * \note            No NMI/HardFault or deep-STOP continuity. Service overflow
 *                  at least every 2^32 microseconds; only one pending wrap can
 *                  be recovered. Stop/restart invalidates old deadlines.
 *                  ACK failure invalidates the epoch and retains timer_error
 *                  until a new start; zero alone does not prove readiness.
 */
uint64_t nx_stm32h743_system_now(nx_stm32h743_system_t* system);
/**
 * \brief Count only after observed W0C acknowledgement; no kernel call.
 * \note Failed ACK invalidates time and retains cleanup. Failed finite IRQ
 *       withdrawal masks IRQs, observes the terminal fault and parks; a
 *       returning observer never permits progress or loan reclamation.
 */
void nx_stm32h743_system_timer_irq(nx_stm32h743_system_t* system);
/** \brief Check device group/density; PCB, package and revision stay external.
 */
nx_result_t nx_stm32h743_validate_identity(void);
/** \brief CPU-only pre-DATA/BSS setup, only after ExitRun0Mode allows RAM. */
void SystemInit(void);
/**
 * \brief           Mandatory MMIO-only power entry before any RAM write.
 * \note            Exact LDO software reference only. The assembly uses no
 *                  stack, RAM, global state or nested call. Bounded failure
 *                  masks IRQs and retains reason in r0 before terminal loop.
 */
void ExitRun0Mode(void);
/** \brief Reserved TIM2 vector bound to the one statically selected context. */
void TIM2_IRQHandler(void);

#ifndef NX_H743_READ
#define NX_H743_READ(reg) (*(reg))
#endif
#ifndef NX_H743_WRITE
#define NX_H743_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_H743_SYSTEM_POLL
#define NX_H743_SYSTEM_POLL(system) ((void)(system))
#endif
#ifndef NX_H743_IRQ_AVAILABLE
#define NX_H743_IRQ_AVAILABLE(irq)                                             \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) == 0U &&                              \
     NVIC_GetPendingIRQ((IRQn_Type)(irq)) == 0U &&                             \
     NVIC_GetActive((IRQn_Type)(irq)) == 0U)
#endif
#ifndef NX_H743_IRQ_CONTROL
static inline nx_result_t nx_stm32h743_irq_control(int irq, bool enabled,
                                                   uint8_t priority) {
    if (irq < 0 || irq >= 150 || priority >= 16U) {
        return NX_ERROR_INVALID;
    }
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
    return NVIC_GetEnableIRQ((IRQn_Type)irq) == (enabled ? 1U : 0U) &&
                   (enabled ? NVIC_GetPriority((IRQn_Type)irq) == priority
                            : NVIC_GetPendingIRQ((IRQn_Type)irq) == 0U)
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
#define NX_H743_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_stm32h743_irq_control((irq), (enabled), (priority))
#endif
#ifdef __cplusplus
}
#endif
#endif
