/**
 * \file            stm32h743_clock.h
 * \brief           Private cold admission for the optional HSI/PLL1 profile
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_CLOCK_H
#define NX_STM32H743_CLOCK_H
#include "stm32h743_system.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Read prerequisites for the optional nominal CPU384/HCLK192 profile.
 * \param[in] system: Exact exclusive cold context after trusted reset.
 * \return SUCCESS or INVALID/CONTEXT/BUSY/STATE/UNSUPPORTED without mutation.
 * \note Boot ownership must establish the real reset-only call sequence,
 *       before SysTick or a scheduler has ever been enabled. A caller bool
 *       cannot establish this precondition. CTRL is never read because that
 *       would consume COUNTFLAG; zero LOAD/VAL do not prove ENABLE is clear.
 *       This diagnostic is not an independent authorization to start clocks.
 *       Admit only H743 revisionV2003 with its factory-default HSI trim.
 *       Y/W1003 cannot be distinguished; X2001 lacks an explicitly reviewed
 *       electrical chapter. Both are rejected without fallback. Require reset
 * PLL1 configuration, idle Flash banks, ready LDO/VOS3, Cache-off and reset
 * clock consumers. This observation does not start a PLL, own hardware, prove
 * oscillator measurement or qualify a board. It cannot adopt bootloader state
 * or prove hidden history merely from matching readable register values.
 */
nx_result_t nx_stm32h743_pll384_preflight(const nx_stm32h743_system_t* system);
/** \brief Selected-only responsibility for the one fixed PLL1 clock owner. */
typedef struct {
    uint32_t remaining_effects;
} nx_stm32h743_pll384_state_t;
/**
 * \brief Establish nominal CPU384/HCLK192/APB96 before any clock consumer.
 * \param[in,out] state: Zeroed exclusive static state for the global System.
 * \param[in] poll_limit: Nonzero bounded hardware observation budget.
 * \return Primary/cleanup result and retained effects on failed restoration.
 * \note Inherits the real cold-reset precondition of preflight. No timebase,
 *       cache, scheduler or I/O consumer may already own this clock domain.
 *       Acquire VOS1 and shared Flash timing before increasing clocks. A
 *       failed readback never releases an unproved hardware responsibility.
 */
nx_stm32h743_start_result_t
nx_stm32h743_pll384_start(nx_stm32h743_pll384_state_t* state,
                          uint32_t poll_limit);
/**
 * \brief Restore only acquired clock fields after all clock consumers join.
 * \param[in,out] state: Same owner, retained until every restore is observed.
 * \param[in] poll_limit: Nonzero bounded hardware observation budget.
 * \return SUCCESS after release, or error with responsibility retained.
 * \note Stop devices, DMA, IRQ users, owned caches and timebase first. Prove
 *       actual HSI selection before disabling PLL1 or reducing bus, Flash or
 *       voltage protections. An already released owner performs no MMIO.
 */
nx_result_t nx_stm32h743_pll384_stop(nx_stm32h743_pll384_state_t* state,
                                     uint32_t poll_limit);
/** \brief Read the exact selected V-cut clock, mux, power and Flash profile.
 * \note This is register admission, not measured frequency or ownership.
 *       The unmultiplied HSI design envelope is62..65MHz; PLL output adds
 *       the RM0433 Rev9 PLL-ready +/-2percent bound. No peer/line proof.
 */
bool nx_stm32h743_pll384_clocks_ready(void);
/** \brief Require the same retained ready owner as well as actual registers. */
bool nx_stm32h743_pll384_owner_ready(const nx_stm32h743_pll384_state_t* state);
/** \brief Acquire only the TIM2 time domain, preserving the clock lease.
 * \note TIM2 consumes192MHz, not CPU384MHz or APB96MHz. Cache-on is not
 *       supported by this profile. System remaining_effects has its own
 *       namespace; cleanup never releases the separate joint clock owner.
 */
nx_stm32h743_start_result_t
nx_stm32h743_pll384_system_start(nx_stm32h743_pll384_state_t* state,
                                 uint32_t cache_mask, uint32_t poll_limit);
#ifndef NX_H743_CLOCK_POLL
#define NX_H743_CLOCK_POLL() ((void)0)
#endif
#ifdef __cplusplus
}
#endif
#endif
