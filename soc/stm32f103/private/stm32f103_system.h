/**
 * \file            stm32f103_system.h
 * \brief           Private reset HSI8 ownership and 16-bit TIM2 time domain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F103_SYSTEM_H
#define NX_STM32F103_SYSTEM_H
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#include "stm32f103xb.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_STM32F103_EFFECT_GATE  1U
#define NX_STM32F103_EFFECT_TIMER 2U
typedef struct {
    RCC_TypeDef* rcc;
    FLASH_TypeDef* flash;
    TIM_TypeDef* timer;
    uint64_t overflow_us;
    uint32_t core_hz;
    uint32_t reset_cause;
    volatile uint32_t remaining_effects;
    volatile nx_result_t timer_error;
    volatile bool started;
} nx_stm32f103_system_t;
typedef struct {
    nx_result_t primary;
    nx_result_t cleanup;
    uint32_t remaining_effects;
} nx_stm32f103_start_result_t;
extern nx_stm32f103_system_t g_nx_stm32f103_system;
/**
 * \brief Inspect reset-like HSI8, idle TIM2 and IRQ before any writes.
 * \param[in] system: Static context, privileged unmasked bootstrap task.
 * \return SUCCESS or CONTEXT/INVALID/BUSY without effects.
 * \note No running bootloader, PLL, HSE or divided bus profile is adopted.
 */
nx_result_t nx_stm32f103_system_preflight(const nx_stm32f103_system_t* system);
/**
 * \brief Acquire only reserved TIM2; clocks remain reset HSI8/div1.
 * \param[in,out] system: Static cold context; all consumers are absent.
 * \param[in] poll_limit: Positive finite UG acknowledgement read budget.
 * \return Primary/cleanup errors and retained effects on failed rollback.
 * \note TIM2 is 16-bit. Overflow IRQ must run at least once per 65.536ms.
 *       HSI tolerance affects real microseconds and baud; no calibration claim.
 */
nx_stm32f103_start_result_t
nx_stm32f103_system_start(nx_stm32f103_system_t* system, uint32_t poll_limit);
/**
 * \brief Withdraw timer source and IRQ before releasing its owned gate.
 * \param[in,out] system: All deadline/device/task consumers already joined.
 * \param[in] poll_limit: Positive bounded budget, retained for API consistency.
 * \return SUCCESS after readback; error retains remaining effects.
 */
nx_result_t nx_stm32f103_system_stop(nx_stm32f103_system_t* system,
                                     uint32_t poll_limit);
/**
 * \brief Snapshot the extended 1MHz counter with one pending-wrap correction.
 * \param[in,out] system: Live task/configurable IRQ context; no NMI/HardFault.
 * \return Microseconds, zero outside a successfully started clock epoch.
 * \note No clock change/deep-sleep continuity. IRQ/masking gap <65.536ms.
 *       Observed W0C failure ends the epoch and parks while retaining owners.
 */
uint64_t nx_stm32f103_system_now(nx_stm32f103_system_t* system);
/** \brief Acknowledge one owned W0C overflow or enter terminal fault handling.
 */
void nx_stm32f103_system_timer_irq(nx_stm32f103_system_t* system);
/**
 * \brief Check the CPU-readable factory Flash size for the declared target.
 * \note ES096 2.2.2 prevents normal software IDCODE/REV_ID observation.
 *       A matching 128KiB density does not identify device group or package.
 */
nx_result_t nx_stm32f103_validate_identity(void);
/** \brief Pre-RAM CPU-only VTOR/priority grouping; never starts a scheduler. */
void SystemInit(void);
/** \brief One static reserved TIM2 owner. */
void TIM2_IRQHandler(void);
#ifndef NX_F103_READ
#define NX_F103_READ(reg) (*(reg))
#endif
#ifndef NX_F103_WRITE
#define NX_F103_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_F103_SYSTEM_POLL
#define NX_F103_SYSTEM_POLL(system) ((void)(system))
#endif
#ifndef NX_F103_IRQ_AVAILABLE
#define NX_F103_IRQ_AVAILABLE(irq)                                             \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) == 0U &&                              \
     NVIC_GetPendingIRQ((IRQn_Type)(irq)) == 0U &&                             \
     NVIC_GetActive((IRQn_Type)(irq)) == 0U)
#endif
#ifndef NX_F103_IRQ_CONTROL
static inline nx_result_t nx_stm32f103_irq_control(int irq, bool enabled,
                                                   uint8_t priority) {
    if (irq < 0 || irq >= 43 || priority >= 16U) {
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
    if (NVIC_GetEnableIRQ((IRQn_Type)irq) != (enabled ? 1U : 0U) ||
        (enabled && NVIC_GetPriority((IRQn_Type)irq) != priority) ||
        (!enabled && NVIC_GetPendingIRQ((IRQn_Type)irq) != 0U)) {
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
#define NX_F103_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_stm32f103_irq_control((irq), (enabled), (priority))
#endif
#ifdef __cplusplus
}
#endif
#endif
