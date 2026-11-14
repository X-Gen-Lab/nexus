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
#include "nexus/io/dma.h"
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
#ifndef NEXUS_QSPI_PROGRAM_DMA
#define NEXUS_QSPI_PROGRAM_DMA 0U
#endif
#ifndef NEXUS_QSPI_QUAD_READ
#define NEXUS_QSPI_QUAD_READ 0U
#endif
#ifndef NEXUS_QSPI_CPU_MAP
#define NEXUS_QSPI_CPU_MAP 0U
#endif
#if NEXUS_QSPI_CPU_MAP
typedef struct nx_stm32_qspi_mapped_config nx_stm32_qspi_mapped_config_t;
#endif
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_qspi.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Only the selected program-DMA mode allocates this mutable extension.
 * \note DMA2 stream7/channel3 has one task-serviced owner with NVIC disabled.
 *       Memory completion never establishes controller or external Flash idle.
 */
typedef struct {
    DMA_HandleTypeDef transport;
    nx_time_us_t drain_deadline;
    bool captured;
    bool prepared;
    bool active;
} nx_stm32_qspi_dma_t;
/** \brief Immutable DMA storage and memory-domain connection for this owner. */
typedef struct {
    nx_stm32_qspi_dma_t* state;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
} nx_stm32_qspi_dma_config_t;
/** \brief Exact selected serial endpoint under one immutable ClockPlan.
 * \note Platform retains GPIO clocks/snapshots; QSPI IRQ starts disabled,
 *       inactive and unpended, remains disabled through this task-only mode.
 *       Pins: PB6/NCS AF10, PF10/CLK AF9, PF8/IO0 and PF9/IO1 AF10.
 *       Selected quad read adds PF7/IO2 and PF6/IO3 AF9, using the SIQ
 *       software reference and observed QE1/SUS0 without any status mutation.
 *       Optional program DMA has its own static extension and disabled vector.
 *       Read/status/verify stay PIO. Quad changes only synchronous read to
 *       6Bh/24-bit address/eight dummy cycles/four data lines. Status, verify
 *       and 02h page program stay single-line. No XIP, QPI/DDR or implicit
 *       protection changes. CPU_MAP selects a separate readonly loan face;
 *       it has no ordinary Flash/FlashAsync face and excludes program DMA.
 *       FlashAsync.stop seals requests and joins them, retaining this bank
 *       and its readonly Flash face; only Platform cold stop releases them.
 *       ES0321 2.4.5: indirect phases forbid all 0x90000000 aperture accesses,
 *       including external CPU/IRQ/DMA/debug accesses. CPU_MAP grants only
 *       explicit CPU reader loans while mapped; all other masters remain
 *       excluded. Withholding a pointer alone does not establish isolation.
 *       External integration provides rated stable power/powerup readiness
 *       Four-pad integration holds WP#/HOLD# or the exact suffix's RESET# at
 *       allowed inactive levels. Quad requires both actual IO2/IO3 wires,
 *       rather than inactive straps on their former functions. Neither
 *       software reference establishes a fitted suffix or physical wiring.
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
    const nx_stm32_qspi_dma_config_t* dma;
#if NEXUS_QSPI_CPU_MAP
    const nx_stm32_qspi_mapped_config_t* mapping;
#endif
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
#if NEXUS_QSPI_PROGRAM_DMA
/** \brief Admit the exact disabled DMA vector without register writes.
 * \param[in] config: Validated immutable optional engine configuration.
 * \return SUCCESS or BUSY retaining all foreign resources.
 */
nx_result_t nx_stm32_qspi_dma_admit(const nx_stm32_qspi_dma_config_t* config);
/** \brief Prepare one DMA2 stream7/channel3 after its shared clock is held.
 * \param[in,out] owner: Original exclusive QSPI owner with static config.
 * \return SUCCESS or retained failure requiring cold close.
 */
nx_result_t nx_stm32_qspi_dma_prepare(nx_stm32_qspi_t* owner);
/** \brief Start one already-admitted byte-wide program page transfer.
 * \param[in,out] owner: Original owner; complete buffer is already borrowed.
 * \param[in] data: The current page within the retained readable DMA domain.
 * \return SUCCESS or retained failure; no completion or borrow release.
 */
nx_result_t nx_stm32_qspi_dma_transmit(nx_stm32_qspi_t* owner, uint8_t* data);
/** \brief Dispatch only the disabled vector's owned latched DMA facts.
 * \param[in,out] owner: Serialized task-service owner, never an IRQ call.
 * \return SUCCESS or STATE if the owned clock/vector is no longer usable.
 */
nx_result_t nx_stm32_qspi_dma_service(nx_stm32_qspi_t* owner);
/** \brief Join the actual stream and clear addresses/callback references.
 * \param[in,out] owner: Original owner, retaining storage on failed join.
 * \return SUCCESS, BUSY or IO; neither error releases any borrow.
 */
nx_result_t nx_stm32_qspi_dma_withdraw(nx_stm32_qspi_t* owner);
/** \brief Restore the admitted cold stream after all transfers have joined.
 * \param[in,out] owner: Original owner before Platform releases DMA2 clock.
 * \return SUCCESS or retained failure; never resets the shared DMA engine.
 */
nx_result_t nx_stm32_qspi_dma_stop(nx_stm32_qspi_t* owner);
#endif
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
