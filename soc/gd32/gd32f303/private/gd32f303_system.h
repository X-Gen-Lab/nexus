/**
 * \file            gd32f303_system.h
 * \brief           Private GD32F303RCT6 IRC8 and cascaded timer ownership
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
/** \brief Two reserved16-bit timers; no dynamic clock/deep STOP continuation.
 */
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
/** \brief Reject a foreign clock, either reserved timer or pending vector.
 * \param[in] system: Static cold context, privileged unmasked task only.
 * \return SUCCESS or error before hardware writes; no running-state adoption.
 */
nx_result_t nx_gd32f303_system_preflight(const nx_gd32f303_system_t* system);
/** \brief Start IRC8 and TIMER1/TIMER4 as a32-bit1MHz hardware cascade.
 * \param[in,out] system: Static state retained after incomplete cleanup.
 * \param[in] poll_limit: Positive finite update budget for each counter.
 * \return Primary, cleanup and actual remaining effects; no scheduler start.
 */
nx_gd32f303_start_result_t
nx_gd32f303_system_start(nx_gd32f303_system_t* system, uint32_t poll_limit);
/** \brief Stop only after all device, task and time-domain consumers join.
 * \param[in,out] system: Same cold owner; failed readback retains ownership.
 * \return SUCCESS only after timer source, vector and owned gate withdraw.
 */
nx_result_t nx_gd32f303_system_stop(nx_gd32f303_system_t* system);
/** \brief Snapshot hardware high/low/high and one pending32-bit wrap.
 * \param[in,out] system: Live epoch, privileged task/configurable IRQ only.
 * \return Microseconds; zero outside a live epoch is not readiness proof.
 * \note TIMER4 overflow must be serviced within4294967296us, including
 *       Flash stalls and masks. Low TIMER1 IRQs may coalesce without time loss.
 *       No NMI/HardFault, dynamic-clock, deep-sleep or missed-wrap recovery.
 */
uint64_t nx_gd32f303_system_now(nx_gd32f303_system_t* system);
/** \brief Ack low W0C only; hardware owns carries, failed ack is fail-stop.
 */
void nx_gd32f303_system_timer_irq(nx_gd32f303_system_t* system);
/** \brief Acknowledge high-cascade overflow before extending2^32us.
 * \param[in,out] system: Live static time-domain owner, bounded IRQ only.
 * \note Failed acknowledgement isolates and parks without returning owners.
 */
void nx_gd32f303_system_cascade_irq(nx_gd32f303_system_t* system);
/** \brief CPU-only setup; official reset startup calls after RAM init. */
void SystemInit(void);
/** \brief Exact vector for the reserved monotonic TIMER1 owner. */
void TIMER1_IRQHandler(void);
/** \brief Exact high-word overflow vector; no low-word service gap issue. */
void TIMER4_IRQHandler(void);
/** \brief Exact selected cold resources; no register script or discovery. */
#define NX_GD32F303_PLATFORM_UART   (1U << 0U)
#define NX_GD32F303_PLATFORM_SPI    (1U << 1U)
#define NX_GD32F303_PLATFORM_ADC    (1U << 2U)
#define NX_GD32F303_PLATFORM_PWM    (1U << 3U)
#define NX_GD32F303_PLATFORM_FLASH  (1U << 4U)
#define NX_GD32F303_PLATFORM_DMA_TX (1U << 5U)
#define NX_GD32F303_PLATFORM_DMA_RX (1U << 6U)
/** \brief Immutable maintained GPIOA and controller selection. */
typedef struct {
    uint16_t pins;
    uint8_t peripherals;
    uint8_t exti_lines;
} nx_gd32f303_platform_config_t;
/** \brief One selected GPIOA bank lease; never a maximum instance pool. */
typedef struct {
    uint32_t saved_ctl[2];
    uint16_t mask;
    uint16_t saved_latch;
    bool gate_added;
    bool retained;
    bool captured;
} nx_gd32f303_pin_lease_t;
/** \brief Reject a powered foreign pin before any global clock effect.
 * \param[in] mask: Exact selected PA0..PA7/PA9/PA10 mask; no unpowered MMIO
 * probe. \return Task SUCCESS/error without writes; acquisition rechecks after
 * power.
 */
nx_result_t nx_gd32f303_pins_preflight(uint32_t mask);
/** \brief Acquire GPIOA exact mask after checking every powered pin/lock.
 * \param[in,out] lease: Static state through successful release.
 * \param[in] mask: Exact selected subset of PA0..PA7/PA9/PA10; no SWJ
 * ownership. \return Cold task SUCCESS or error; retained means release must be
 * retried. \note Preserves preexisting clock and all unrelated pins. No PCB
 * policy.
 */
nx_result_t nx_gd32f303_pins_acquire(nx_gd32f303_pin_lease_t* lease,
                                     uint32_t mask);
/** \brief Restore selected nibbles/latch before withdrawing an added gate.
 * \param[in,out] lease: Same lease after all pin users/IRQs have joined.
 * \return SUCCESS after actual readback; failure preserves unfinished owner.
 */
nx_result_t nx_gd32f303_pins_release(nx_gd32f303_pin_lease_t* lease);
/** \brief Named unfinished leases, independent of physical register bits. */
#define NX_GD32F303_OWN_AFIO   (1U << 0U)
#define NX_GD32F303_OWN_USART0 (1U << 6U)
#define NX_GD32F303_OWN_SPI0   (1U << 4U)
#define NX_GD32F303_OWN_ADC0   (1U << 1U)
#define NX_GD32F303_OWN_TIMER2 (1U << 2U)
#define NX_GD32F303_OWN_DMA0   (1U << 3U)
/** \brief One compact receipt, retained until all readbacks succeed. */
typedef struct {
    nx_gd32f303_pin_lease_t pins;
    uint8_t owned;
} nx_gd32f303_platform_resources_t;
/**
 * \brief Inspect exact selected hardware without acquiring or changing it.
 * \param[in] config: Immutable maintained selection, privileged unmasked task.
 * \return SUCCESS or CONTEXT/INVALID/BUSY; rejection has no register writes.
 * \note System admission remains separate and precedes time-domain startup.
 */
nx_result_t
nx_gd32f303_platform_preflight(const nx_gd32f303_platform_config_t* config);
/**
 * \brief Acquire only selected gates and one pin receipt after System starts.
 * \param[in,out] resources: Cold static owner; no outstanding resource receipt.
 * \param[in] config: Immutable selection, read only during this call.
 * \return SUCCESS/error; failure can retain gates and requires release retry.
 * \note One privileged unmasked bootstrap task; no active device consumers.
 */
nx_result_t nx_gd32f303_platform_resources_acquire(
    nx_gd32f303_platform_resources_t* resources,
    const nx_gd32f303_platform_config_t* config);
/**
 * \brief Restore pins before returning only gates added by this exact owner.
 * \param[in,out] resources: Same receipt after all device/IRQ users join.
 * \return SUCCESS after readback; failure retains unfinished cleanup duties.
 * \note Privileged unmasked task; retry does not revoke any device borrow.
 */
nx_result_t nx_gd32f303_platform_resources_release(
    nx_gd32f303_platform_resources_t* resources);
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
