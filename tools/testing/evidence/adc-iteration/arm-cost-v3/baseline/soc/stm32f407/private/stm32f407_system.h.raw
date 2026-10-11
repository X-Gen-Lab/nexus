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

typedef struct {
    RCC_TypeDef* rcc;
    FLASH_TypeDef* flash;
    PWR_TypeDef* power;
    TIM_TypeDef* timer;
    uint64_t overflow_us;
    uint32_t reset_cause;
    uint32_t core_hz;
    uint32_t remaining_effects;
    bool started;
} nx_stm32_system_t;

typedef struct {
    int primary;
    int cleanup;
    uint32_t remaining_effects;
} nx_stm32_start_result_t;

/**
 * \brief           Establish HSE8/PLL168 and TIM2 at 1 MHz.
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

/**
 * \brief           Release the timebase and switch back to HSI.
 *
 * \param[in,out]   system: Started context; consumers already quiescent.
 *
 * \param[in]       poll_limit: Nonzero maximum polls per phase.
 *
 * \return          Zero after cleanup; failure retains clock effects.
 */
int nx_stm32_system_stop(nx_stm32_system_t* system, uint32_t poll_limit);

/**
 * \brief           Read TIM2 microseconds, including one pending wrap.
 *
 * \param[in,out]   system: Started context; no dynamic reclocking.
 *
 * \return          Monotonic microseconds, or zero before start.
 *
 * \details         Task/configurable IRQ uses short PRIMASK exclusion.
 *                  TIM2 IRQ must run once per 2^32 microseconds.
 *                  NMI/HardFault callers are unsupported.
 */
uint64_t nx_stm32_system_now(nx_stm32_system_t* system);

/**
 * \brief           Handle the reserved TIM2 overflow interrupt.
 *
 * \param[in,out]   system: Static context bound to the vector.
 */
void nx_stm32_system_timer_irq(nx_stm32_system_t* system);

/**
 * \brief           Set CPU state before data/bss initialization.
 *
 * \details         Official startup calls this before RAM initialization.
 *                  It only writes CPU/RCC registers and enables the FPU.
 */
void SystemInit(void);

/** \brief Reserved TIM2 vector; no other owner may use this timer. */
void TIM2_IRQHandler(void);

/**
 * \brief           Check device group and exact Flash density.
 *
 * \param[in]       expected_flash_bytes: Reviewed VE/ZG density.
 *
 * \return          SUCCESS or INVALID/IO for mismatched silicon facts.
 *
 * \note            Package and F405/F407 identity require fixture inspection;
 *                  these parts share a device group ID.
 */
nx_result_t nx_stm32_validate_identity(uint32_t expected_flash_bytes);

#endif
