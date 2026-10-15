/**
 * \file            stm32u575_system.h
 * \brief           Private TZEN0 MSIS4MHz ownership and independent TIM2 time
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32U575_SYSTEM_H
#define NX_STM32U575_SYSTEM_H
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#include "stm32u575xx.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_STM32U575_EFFECT_TIMER 1U
typedef struct nx_stm32u575_system {
    RCC_TypeDef* rcc;
    FLASH_TypeDef* flash;
    TIM_TypeDef* timer;
    ICACHE_TypeDef* icache;
    DCACHE_TypeDef* dcache;
    uint64_t overflow_us;
    uint32_t core_hz;
    uint32_t reset_cause;
    nx_result_t timer_error;
    volatile uint32_t remaining_effects;
    volatile bool started;
} nx_stm32u575_system_t;
typedef struct {
    nx_result_t primary;
    nx_result_t cleanup;
    uint32_t remaining_effects;
} nx_stm32u575_start_result_t;
extern nx_stm32u575_system_t g_nx_stm32u575_system;
/**
 * \brief Reject incompatible TZEN, clock, cache and reserved TIM2 owners.
 * \param[in] system: Static privileged cold context without consumers.
 * \return SUCCESS or a rejection before the first hardware write.
 * \note Reset-like MSIS4MHz only; no bootloader adoption or option writes.
 */
nx_result_t nx_stm32u575_system_preflight(const nx_stm32u575_system_t* system);
/**
 * \brief Acquire TIM2/IRQ45 at1MHz under the verified fixed clock profile.
 * \param[in,out] system: Static privileged bootstrap owner.
 * \param[in] poll_limit: Positive bounded phase budget; never elapsed cycles.
 * \return Primary/cleanup errors and effects retained after failed cleanup.
 */
nx_stm32u575_start_result_t
nx_stm32u575_system_start(nx_stm32u575_system_t* system, uint32_t poll_limit);
/**
 * \brief Withdraw timer/IRQ/gate before releasing the time domain.
 * \param[in,out] system: All peripheral/OS consumers have already joined.
 * \param[in] poll_limit: Positive bounded budget.
 * \return Success on observed withdrawal; failure retains ownership.
 */
nx_result_t nx_stm32u575_system_stop(nx_stm32u575_system_t* system,
                                     uint32_t poll_limit);
/**
 * \brief Snapshot one pending32bit wrap with preserved incoming mask.
 * \param[in,out] system: Live privileged owner; configurable IRQ or task.
 * \return Microseconds, or zero before start or after a timebase fault.
 * \note No NMI/HardFault, STOP continuity or dynamic clock support.
 *       TIM2 IRQ must run at least once every2^32 microseconds.
 */
uint64_t nx_stm32u575_system_now(nx_stm32u575_system_t* system);
/**
 * \brief Observe overflow acknowledgement before counting the wrap.
 * \note Refused ACK invalidates time and retains timer cleanup ownership.
 *       Finite source withdrawal is attempted without polling or kernel calls.
 *       Always retain a terminal mask, observe the fault and park, including
 *       successful isolation. A returning observer cannot resume old deadline
 *       consumers or authorize reclamation of any retained loan.
 */
void nx_stm32u575_system_timer_irq(nx_stm32u575_system_t* system);
/** \brief Observe density and TZEN0; package/PCB identity remains external. */
nx_result_t nx_stm32u575_validate_identity(void);
/** \brief Pre-RAM CPU-only setup, never initialized RAM or clock globals. */
void SystemInit(void);
/** \brief Reserved vector for the one explicit timer owner. */
void TIM2_IRQHandler(void);
#ifndef NX_U575_READ
#define NX_U575_READ(reg) (*(reg))
#endif
#ifndef NX_U575_WRITE
#define NX_U575_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_U575_SYSTEM_POLL
#define NX_U575_SYSTEM_POLL(system) ((void)(system))
#endif
#ifndef NX_U575_IRQ_AVAILABLE
#define NX_U575_IRQ_AVAILABLE(irq)                                             \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) == 0U &&                              \
     NVIC_GetPendingIRQ((IRQn_Type)(irq)) == 0U &&                             \
     NVIC_GetActive((IRQn_Type)(irq)) == 0U)
#endif
#ifndef NX_U575_IRQ_CONTROL
static inline nx_result_t nx_stm32u575_irq_control(int irq, bool enabled,
                                                   uint8_t priority) {
    if (irq < 0 || irq >= 126 || priority >= 16U)
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
    return NVIC_GetEnableIRQ((IRQn_Type)irq) == (enabled ? 1U : 0U) &&
                   (enabled ? NVIC_GetPriority((IRQn_Type)irq) == priority
                            : NVIC_GetPendingIRQ((IRQn_Type)irq) == 0U)
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
#define NX_U575_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_stm32u575_irq_control((irq), (enabled), (priority))
#endif
#ifdef __cplusplus
}
#endif
#endif
