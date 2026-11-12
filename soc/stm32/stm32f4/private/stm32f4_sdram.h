/**
 * \file            stm32f4_sdram.h
 * \brief           One selected SDRAM controller and an explicit CPU data loan
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F4_SDRAM_H
#define NEXUS_STM32F4_SDRAM_H
#include "nexus/core/time.h"
#include "nexus/io/external_bus.h"
#include "nexus/io/memory_window.h"
#include "stm32f407_platform_cold.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Device minimum in two independent units; neither is rounded down. */
typedef struct {
    uint32_t nanoseconds;
    uint8_t cycles;
} nx_stm32_sdram_minimum_t;
/** \brief Datasheet minima, not preencoded controller register values. */
typedef struct {
    nx_stm32_sdram_minimum_t mrd;
    nx_stm32_sdram_minimum_t xsr;
    nx_stm32_sdram_minimum_t ras;
    nx_stm32_sdram_minimum_t rc;
    nx_stm32_sdram_minimum_t wr;
    nx_stm32_sdram_minimum_t rp;
    nx_stm32_sdram_minimum_t rcd;
} nx_stm32_sdram_timing_t;
/** \brief Exact single Bank1 CPU-data configuration under one static clock.
 * \note First profile: Bank1, 8MiB, row12/column8, four internal banks, 16bit.
 *       Platform owns GPIO gates/snapshots and the shared SYSCFG gate through
 *       successful stop. System/ClockPlan is already admitted. Default C0000000
 *       CPU data mapping only; natural alignment and the caller's MPU policy
 *       remain required. The explicit started System and its original time
 *       epoch remain live through stop. No DMA, startup objects, XIP or probe.
 */
typedef struct {
    const nx_stm32_platform_pin_config_t* pins;
    size_t pin_count;
    nx_stm32_system_t* system;
    nx_stm32_sdram_timing_t timing;
    uint32_t hclk_hz;
    uint32_t bytes;
    uint32_t refresh_window_us;
    uint32_t powerup_us;
    uint32_t prepare_budget_us;
    uint32_t stop_budget_us;
    uint8_t row_bits;
    uint8_t column_bits;
    uint8_t internal_banks;
    uint8_t width_bits;
    uint8_t cas;
    uint8_t sdclk_div;
    uint8_t read_pipe;
    uint8_t initial_refreshes;
} nx_stm32_sdram_config_t;
/** \brief Retained cold progress and the original live loan, never a pool. */
typedef struct {
    const nx_stm32_sdram_config_t* config;
    nx_memory_window_lease_t* lease;
    uint64_t epoch;
    uint64_t system_epoch;
    nx_time_us_t last_clock;
    nx_time_us_t powerup_until;
    uint32_t saved_sdcr;
    uint32_t saved_sdtr;
    uint32_t saved_unused_wr;
    uint32_t saved_sdrtr;
    uint8_t phase;
    bool gate_added;
    bool captured;
    bool prepared;
    /** Only this persistent owner may resume its own completed Self-refresh. */
    bool resume_selfref;
} nx_stm32_sdram_t;
/** \brief Read-only static metadata and foreign-owner admission before System.
 * \param[in] owner: Persistent idle exclusive controller owner.
 * \param[in] config: Immutable complete endpoint and static System binding.
 * \return SUCCESS or exact rejection, with all storage and hardware unchanged.
 * \note Privileged unmasked Thread. No started clock, GPIO gates or timer
 *       reads are required; Platform later admits the actual live resources.
 */
nx_result_t nx_stm32_sdram_admit(const nx_stm32_sdram_t* owner,
                                 const nx_stm32_sdram_config_t* config);
/** \brief Start using the exact finite prepare budget in the hand-written C.
 * \param[in,out] owner: Persistent admitted exclusive controller owner.
 * \param[in] config: Immutable endpoint and already started original System.
 * \return SUCCESS or exact admission, finite-time or retained cold failure.
 * \note Privileged unmasked Thread. A saturated deadline never selects wait.
 */
nx_result_t nx_stm32_sdram_start(nx_stm32_sdram_t* owner,
                                 const nx_stm32_sdram_config_t* config);
/** \brief Initialize only the selected bank with a finite absolute deadline.
 * \param[in,out] owner: Zero-initialized persistent exclusive controller owner.
 * \param[in] config: Immutable pins, timing and exact started System binding.
 * \param[in] deadline: Finite absolute microseconds in that live System epoch.
 * \return SUCCESS, admission error or retained cold progress on failure.
 * \note Privileged Thread, IRQ masks clear. Config/state/pads outlive stop;
 *       Platform retains selected GPIO/SYSCFG gates and their snapshots.
 */
nx_result_t nx_stm32_sdram_prepare(nx_stm32_sdram_t* owner,
                                   const nx_stm32_sdram_config_t* config,
                                   nx_time_us_t deadline);
/** \brief Join the original CPU loan, drain commands and withdraw owned clock.
 * \param[in,out] owner: The original exclusive persistent controller owner.
 * \return SUCCESS only after verified command, register and owned gate exit.
 * \note Privileged Thread, IRQ masks clear. The configured finite stop budget
 *       uses the original System epoch. Lost time/loan retains responsibility;
 *       no failure revokes direct CPU users or resets another FMC owner.
 */
nx_result_t nx_stm32_sdram_stop(nx_stm32_sdram_t* owner);
extern const nx_external_bus_ops_t nx_stm32_sdram_bus_ops;
extern const nx_memory_window_ops_t nx_stm32_sdram_window_ops;
#ifdef __cplusplus
}
#endif
#endif
