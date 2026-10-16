/**
 * \file            gd32f303_system.h
 * \brief           Private GD32F303RCT6 IRC8 clock and TIMER1 ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_GD32F303_SYSTEM_H
#define NX_GD32F303_SYSTEM_H
#include "gd32f30x.h"
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief One reserved 16-bit timer; no dynamic clock or sleep continuation. */
typedef struct {
    uint64_t overflow_us;
    uint32_t remaining_effects;
    uint32_t core_hz;
    uint32_t reset_cause;
    nx_result_t timer_error;
    bool started;
} nx_gd32f303_system_t;
typedef struct {
    nx_result_t primary;
    nx_result_t cleanup;
    uint32_t remaining_effects;
} nx_gd32f303_start_result_t;
extern nx_gd32f303_system_t g_nx_gd32f303_system;
/** \brief Reject a foreign clock, live reserved timer or pending vector.
 * \param[in] system: Static cold context, privileged unmasked task only.
 * \return SUCCESS or error before hardware writes; no running-state adoption.
 */
nx_result_t nx_gd32f303_system_preflight(const nx_gd32f303_system_t* system);
/** \brief Start IRC8 8MHz and reserved TIMER1 at 1MHz with bounded checks.
 * \param[in,out] system: Static state retained after incomplete cleanup.
 * \param[in] poll_limit: Positive finite timer-update observation budget.
 * \return Primary, cleanup and actual remaining effects; no scheduler start.
 */
nx_gd32f303_start_result_t
nx_gd32f303_system_start(nx_gd32f303_system_t* system, uint32_t poll_limit);
/** \brief Stop only after all device, task and time-domain consumers join.
 * \param[in,out] system: Same cold owner; failed readback retains ownership.
 * \return SUCCESS only after timer source, vector and owned gate withdraw.
 */
nx_result_t nx_gd32f303_system_stop(nx_gd32f303_system_t* system);
/** \brief Snapshot 16-bit time plus one pending wrap under the saved mask.
 * \param[in,out] system: Live epoch, privileged task/configurable IRQ only.
 * \return Microseconds; zero outside a live epoch is not readiness proof.
 * \note TIMER1 overflow must be serviced within 65536us, including masks.
 *       No NMI/HardFault, dynamic-clock, deep-sleep or missed-wrap recovery.
 */
uint64_t nx_gd32f303_system_now(nx_gd32f303_system_t* system);
/** \brief Extend only after successful W0C; failed ack is terminal fail-stop.
 */
void nx_gd32f303_system_timer_irq(nx_gd32f303_system_t* system);
/** \brief CPU-only setup; official reset startup calls after RAM init. */
void SystemInit(void);
/** \brief Exact vector for the reserved monotonic TIMER1 owner. */
void TIMER1_IRQHandler(void);
#ifndef NX_GD303_READ
#define NX_GD303_READ(reg) (*(reg))
#endif
#ifndef NX_GD303_WRITE
#define NX_GD303_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_GD303_DENSITY
#define NX_GD303_DENSITY() (*(volatile const uint32_t*)(uintptr_t)0x1ffff7e0U)
#endif
#ifndef NX_GD303_POLL
#define NX_GD303_POLL() ((void)0)
#endif
#ifndef NX_GD303_IRQ_AVAILABLE
#define NX_GD303_IRQ_AVAILABLE(irq)                                            \
    (NVIC_GetEnableIRQ((IRQn_Type)(irq)) == 0U &&                              \
     NVIC_GetPendingIRQ((IRQn_Type)(irq)) == 0U &&                             \
     NVIC_GetActive((IRQn_Type)(irq)) == 0U)
#endif
#ifndef NX_GD303_IRQ_CONTROL
static inline nx_result_t nx_gd32f303_irq_control(int irq, bool enable,
                                                  uint8_t priority) {
    if (irq < 0 || irq >= 60 || priority >= 16U)
        return NX_ERROR_INVALID;
    if (enable) {
        NVIC_SetPriority((IRQn_Type)irq, priority);
        NVIC_ClearPendingIRQ((IRQn_Type)irq);
        NVIC_EnableIRQ((IRQn_Type)irq);
    } else {
        NVIC_DisableIRQ((IRQn_Type)irq);
        NVIC_ClearPendingIRQ((IRQn_Type)irq);
    }
    nx_arch_dsb();
    nx_arch_isb();
    return NVIC_GetEnableIRQ((IRQn_Type)irq) == (enable ? 1U : 0U) &&
                   (enable ? NVIC_GetPriority((IRQn_Type)irq) == priority
                           : NVIC_GetPendingIRQ((IRQn_Type)irq) == 0U)
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
#define NX_GD303_IRQ_CONTROL(irq, enable, priority)                            \
    nx_gd32f303_irq_control((irq), (enable), (priority))
#endif
#ifndef NX_GD303_IRQ_PRIORITY
#define NX_GD303_IRQ_PRIORITY(irq) NVIC_GetPriority((IRQn_Type)(irq))
#define NX_GD303_IRQ_GROUP()       NVIC_GetPriorityGrouping()
#endif
#ifdef __cplusplus
}
#endif
#endif
