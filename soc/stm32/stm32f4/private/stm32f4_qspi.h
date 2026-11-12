/**
 * \file            stm32f4_qspi.h
 * \brief           One statically selected indirect W25Q128JV Flash endpoint
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F4_QSPI_H
#define NEXUS_STM32F4_QSPI_H
#include "nexus/io/external_bus.h"
#include "nexus/io/flash_async.h"
#include "stm32f407_platform_cold.h"
/* Generated static storage may share a TU with another private HAL profile.
 * Declare this exact Handle ABI explicitly; never depend on whichever global
 * stm32f4xx_hal_conf.h another controller happened to include first. */
#ifndef USE_HAL_QSPI_REGISTER_CALLBACKS
#define USE_HAL_QSPI_REGISTER_CALLBACKS 1U
#endif
#if USE_HAL_QSPI_REGISTER_CALLBACKS != 1U
#error "Selected QSPI storage requires the per-handle callback ABI"
#endif
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_qspi.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exact single-line serial endpoint under one immutable ClockPlan.
 * \note Platform retains GPIO clocks/snapshots; QSPI IRQ starts disabled,
 *       inactive and unpended, remains disabled through this task-only mode.
 *       Pins: PB6/NCS AF10, PF10/CLK AF9, PF8/IO0 and PF9/IO1 AF10.
 *       No DMA, mapping, XIP, QPI/DDR or implicit protection changes.
 *       FlashAsync.stop seals requests and joins them, retaining this bank
 *       and its readonly Flash face; only Platform cold stop releases them.
 *       ES0321 2.4.5: all 0x90000000 aperture accesses are forbidden throughout
 *       this lifetime, including external CPU/IRQ/DMA/debug accesses. Merely
 *       withholding a mapped pointer does not satisfy that integration duty.
 *       External integration provides rated stable power/powerup readiness
 *       and holds WP#/HOLD# or the exact suffix's RESET# at allowed inactive
 *       levels. This four-pad software reference does not establish wiring.
 */
typedef struct {
    const nx_stm32_platform_pin_config_t* pins;
    size_t pin_count;
    nx_stm32_system_t* system;
    uint32_t hclk_hz;
    uint32_t read_budget_us;
    uint32_t prepare_budget_us;
    uint32_t stop_budget_us;
    uint8_t prescaler;
    uint8_t cs_high_cycles;
} nx_stm32_qspi_config_t;
/** \brief Private single-transport owner; requests remain caller-owned. */
typedef struct {
    QSPI_HandleTypeDef transport;
    const nx_stm32_qspi_config_t* config;
    nx_flash_async_request_t* request;
    uint64_t system_epoch;
    nx_time_us_t last_clock;
    uint32_t saved_cr;
    uint32_t saved_dcr;
    uint32_t saved_ccr;
    uint32_t saved_dlr;
    uint32_t saved_abr;
    size_t completed;
    size_t chunk;
    size_t verified;
    uint8_t verify[32];
    nx_result_t terminal;
    uint8_t phase;
    uint8_t status;
    bool complete;
    bool transport_error;
    bool gate_added;
    bool captured;
    bool prepared;
    bool draining;
    bool pulse_uncertain;
    bool wire_active;
    bool async_stopped;
} nx_stm32_qspi_t;
/** \brief Read-only static admission before System/GPIO resource effects.
 * \param[in] owner: Original idle stable exclusive controller owner.
 * \param[in] config: Immutable exact endpoint and static System binding.
 * \return SUCCESS or exact rejection without mutation/timer/register effects.
 * \note Privileged unmasked Thread. Actual live resources are admitted later.
 */
nx_result_t nx_stm32_qspi_admit(const nx_stm32_qspi_t* owner,
                                const nx_stm32_qspi_config_t* config);
/** \brief Start with the configured finite budget, entirely in hand-written C.
 * \param[in,out] owner: Original persistent exclusive controller owner.
 * \param[in] config: Immutable endpoint and already live original System.
 * \return SUCCESS or precise admission, finite-time or retained cold failure.
 * \note Privileged unmasked Thread. Saturation cannot select infinite wait.
 */
nx_result_t nx_stm32_qspi_start(nx_stm32_qspi_t* owner,
                                const nx_stm32_qspi_config_t* config);
/** \brief Prepare the selected controller and validate the exact JEDEC part.
 * \param[in,out] owner: Zero-initialized persistent exclusive owner.
 * \param[in] config: Immutable endpoint and live System binding.
 * \param[in] deadline: Finite absolute microseconds in that System epoch.
 * \return SUCCESS or retained cold failure; rejected admission has no effect.
 * \note Privileged unmasked Thread; state/config/pins outlive successful stop.
 */
nx_result_t nx_stm32_qspi_prepare(nx_stm32_qspi_t* owner,
                                  const nx_stm32_qspi_config_t* config,
                                  nx_time_us_t deadline);
/** \brief Withdraw the original controller only after actual Flash idle.
 * \param[in,out] owner: Original exclusive persistent owner.
 * \return SUCCESS only after source/buffer/controller/gate withdrawal.
 * \note No pulse replay, memory-map access, foreign adoption or shared reset.
 */
nx_result_t nx_stm32_qspi_stop(nx_stm32_qspi_t* owner);
extern const nx_flash_ops_t nx_stm32_qspi_flash_ops;
extern const nx_flash_async_ops_t nx_stm32_qspi_async_ops;
extern const nx_external_bus_ops_t nx_stm32_qspi_bus_ops;
#ifdef __cplusplus
}
#endif
#endif
