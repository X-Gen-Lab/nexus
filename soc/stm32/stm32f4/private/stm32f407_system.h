/**
 * \file            stm32f407_system.h
 * \brief           STM32F407 private clock and monotonic time infrastructure
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F407_SYSTEM_H
#define NX_STM32F407_SYSTEM_H

#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#include "stm32f407xx.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define NX_STM32_SYSTEM_HAS_TIME_EPOCH 1

typedef struct {
    RCC_TypeDef* rcc;
    FLASH_TypeDef* flash;
    PWR_TypeDef* power;
    TIM_TypeDef* timer;
    uint64_t overflow_us;
    uint64_t time_epoch;
    uint32_t reset_cause;
    uint32_t core_hz;
    volatile uint32_t remaining_effects;
    uint32_t saved_pll;
    uint32_t saved_flash;
    uint32_t saved_power;
    /** \brief Terminal IO diagnostic; never permission to resume or reclaim.
     */
    volatile nx_result_t timer_error;
    bool owned_power_clock;
    bool power_snapshotted;
    volatile bool started;
    bool reset_captured;
} nx_stm32_system_t;

typedef struct {
    int primary;
    int cleanup;
    uint32_t remaining_effects;
} nx_stm32_start_result_t;

/**
 * \brief           Inspect reset-like clocks and an unowned TIM2 without
 * writes. \param[in]       system: Static register-bound context, no external
 * owners. \return          SUCCESS or INVALID/CONTEXT/BUSY before acquisition.
 * \note            Privileged Thread mode with all incoming masks clear.
 *                  GPIO/CCM and an existing PWR interface clock may be shared.
 *                  Unknown live clocks, bootloader handoff and foreign timer
 *                  owners are not adopted. No scheduler or IRQ is changed.
 */
nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t* system);

/**
 * \brief           Establish the selected static clock and TIM2 at 1 MHz.
 *
 * \param[in,out]   system: Exclusive context with bound registers.
 *
 * \param[in]       poll_limit: Nonzero maximum polls per boot phase.
 *
 * \return          Primary/cleanup failures and remaining clock effects.
 *
 * \details         Task startup only; no running peripheral consumers.
 *                  Bootstrap has poll bounds, not invented elapsed time.
 *                  Failed source switching retains the live clock effects.
 */
nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t* system,
                                              uint32_t poll_limit);

/** \brief Restore a stopped system under the exclusive deep-power guard.
 * \param[in,out] system: Stopped owner, all old time/clock borrowers joined.
 * \param[in] poll_limit: Nonzero bootstrap bound per restoration phase.
 * \return Complete primary/cleanup/effects receipt, as for ordinary startup.
 * \note Private power owner only, privileged Thread with PRIMASK=1 and other
 *       masks clear. The closed domain excludes wake publishers through the
 *       new epoch establishment; this never resumes old deadlines or OS time.
 */
nx_stm32_start_result_t nx_stm32_system_restore(nx_stm32_system_t* system,
                                                uint32_t poll_limit);

/**
 * \brief           Release the timebase and switch back to HSI.
 *
 * \param[in,out]   system: Started context; consumers already quiescent.
 *
 * \param[in]       poll_limit: Nonzero maximum polls per phase.
 *
 * \return          Zero after owned PLL/Flash/PWR/timer fields are restored;
 *                  failure retains the exact cleanup responsibility.
 * \note            Unrelated power fields and pre-existing PWR gates survive.
 *                  Consumers join before withdrawal; the live time epoch ends
 *                  before any timer/IRQ reference is removed.
 *                  An active RNG/IRQ source retains its explicit PLL48 lease;
 *                  stop refuses it before any write or epoch invalidation.
 *                  The RNG owner must withdraw RNGEN before retrying.
 */
int nx_stm32_system_stop(nx_stm32_system_t* system, uint32_t poll_limit);

/**
 * \brief           Read TIM2 microseconds, including one pending wrap.
 *
 * \param[in,out]   system: Started context; no dynamic reclocking.
 *
 * \return          Monotonic microseconds, or zero outside a live epoch.
 *
 * \details         Task/configurable IRQ uses short PRIMASK exclusion.
 *                  TIM2 IRQ must run once per 2^32 microseconds.
 *                  NMI/HardFault callers are unsupported. An observed overflow
 *                  acknowledgement fault ends the epoch; old deadlines cannot
 *                  be recovered by cleanup or restart. Zero is not readiness.
 */
uint64_t nx_stm32_system_now(nx_stm32_system_t* system);

/**
 * \brief Adapt the selected system clock without confusing zero with failure.
 * \param[in,out] context: Exact stable selected g_nx_stm32_system address.
 * \return Monotonic microseconds, including valid zero; UINT64_MAX for an
 *         unbound/stopped/failed epoch before or during the snapshot.
 * \note Same execution and lifetime contract as nx_stm32_system_now. No
 *       callback starts or repairs the clock, nor substitutes a new epoch.
 */
uint64_t nx_stm32_system_clock_read(void* context);

/**
 * \brief           Handle the reserved TIM2 overflow interrupt.
 *
 * \param[in,out]   system: Static context bound to the vector.
 * \note            Confirm W0C before extending time. Failure latches IO and
 *                  withdraws DIER/CEN/NVIC once while retaining ownership.
 *                  The lost time domain is always terminal: after the bounded
 *                  withdrawal attempt, retain the IRQ mask and every owner,
 *                  observe the platform fault and park if the observer returns.
 *                  Isolation never permits old deadline consumers to resume;
 *                  no request or optional low-power loan is released.
 */
void nx_stm32_system_timer_irq(nx_stm32_system_t* system);

/**
 * \brief           Set CPU state before data/bss initialization.
 *
 * \details         Official startup calls this before RAM initialization.
 *                  CPU/RCC setup precedes main DATA/BSS and constructors. An
 *                  explicitly selected CCM domain uses its validated linker
 *                  plan and cold local stack for copy/clear; clock or plan
 *                  refusal parks before ordinary RAM initialization.
 */
void SystemInit(void);

/** \brief Reserved TIM2 vector; no other owner may use this timer. */
void TIM2_IRQHandler(void);

/** \brief Terminal CSS/NMI dispatch; no old time borrower may resume.
 * \param[in,out] system: Exact stable owner, accessible in privileged NMI.
 * \note Marks time failed before acknowledgement, retains all effects, masks
 *       configurable IRQs, observes and parks. Never reads the stopped timer
 *       as elapsed time. Selected CSS owns the generated NMI vector.
 */
void nx_stm32_system_css_irq(nx_stm32_system_t* system);

/** \brief Establish hard-float access and the exact Flash vector before RAM.
 * \param[in,out] scb: Accessible privileged SCB register block.
 * \param[in] mpu_control: Accessible MPU block, required disabled at cold
 * startup. \param[in] vectors: Exact linker vector, in internal Flash, aligned
 * to512. \return True after barriers/readback, false before unsupported
 * mutation. \note Reset-stack/Flash-only algorithm; no globals, libc or OS.
 * Does not disable an inherited MPU or erase evidence of foreign RCC ownership.
 */
bool nx_stm32_system_cpu_boot(SCB_Type* scb,
                              volatile const uint32_t* mpu_control,
                              uintptr_t vectors);

/** \brief Read modulo-32-bit milliseconds from the existing Nexus epoch.
 * \return Zero outside a live epoch; no HAL or SysTick timebase is started.
 * \note Selected private HAL calls occur only after Platform timebase startup.
 *       Cold bootstrap uses its own finite poll limits, never this clock.
 */
uint32_t HAL_GetTick(void);

/**
 * \brief           Check observed device group, reviewed cut and Flash density.
 *
 * \param[in]       expected_flash_bytes: Reviewed VE/ZG density.
 *
 * \return          SUCCESS; INVALID for an invalid expected density, IO for
 *                  die/density mismatch, UNSUPPORTED for an unreviewed cut.
 *
 * \note            Package and F405/F407 identity require fixture inspection;
 *                  these parts share a device group ID. The actual complete
 *                  IDCODE is read once; no configured revision substitutes for
 *                  observation. Unknown revisions never inherit compatibility.
 */
nx_result_t nx_stm32_validate_identity(uint32_t expected_flash_bytes);

#endif
