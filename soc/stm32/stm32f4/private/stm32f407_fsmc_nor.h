/**
 * \file            stm32f407_fsmc_nor.h
 * \brief           Explicit CFI/command-set NOR state in the unique FSMC owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_FSMC_NOR_H
#define NEXUS_STM32F407_FSMC_NOR_H
#include "nexus/io/flash_async.h"
#include "stm32f407_fsmc.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Caller-authorized physical geometry and command wiring.
 * \note Command set 1=Intel/Sharp, 2=AMD/Fujitsu. Addresses are explicit byte
 *       offsets (not word indices), including unlock/CFI query locations.
 *       cfi=true verifies QRY, command set, density and every erase region;
 *       false authorizes exactly the supplied command set/geometry instead.
 *       No fitted-part/PCB or power-loss qualification is implied.
 */
typedef struct {
    const nx_flash_geometry_t* geometry;
    uint32_t unlock1;
    uint32_t unlock2;
    uint32_t cfi_address;
    uint16_t command_set;
    bool cfi;
    /** Explicit device busy assertion bound; Core quantization adds one us.
     * Zero is reserved for externally proven zero-delay software models. */
    uint32_t ready_delay_us;
} nx_stm32_fsmc_nor_config_t;
/** \brief Stable zero-initialized endpoint sharing one FSMC bank lease.
 * \note Keep request/input/state/config internal and disjoint through actual
 *       idle; neither timeout nor reset command proves an active pulse idle.
 */
typedef struct {
    nx_stm32_fsmc_lease_t lease;
    const nx_stm32_fsmc_nor_config_t* config;
    nx_flash_async_request_t* request;
    nx_time_us_t last_clock;
    nx_time_us_t poll_after;
    size_t done;
    size_t pulse_bytes;
    uint32_t pulse_offset;
    uint16_t pulse_value;
    nx_result_t terminal;
    uint8_t width;
    bool pulse;
    bool closed;
    bool in_call;
} nx_stm32_fsmc_nor_t;
/** \brief Acquire selected NOR bank and optionally verify actual CFI geometry.
 * \param[in,out] nor: Fresh zero stable internal endpoint state.
 * \param[in,out] bus: Prepared unique shared FSMC owner.
 * \param[in] index: Selected bank configuration index.
 * \param[in] config: Stable explicit geometry/command-set/address facts.
 * \return SUCCESS or error; acquired lease stays alive until explicit close
 *         if a post-acquisition probe fails. No erase/program is issued.
 * \note Privileged unmasked Thread only; uses official LL configured bus.
 */
nx_result_t nx_stm32_fsmc_nor_prepare(nx_stm32_fsmc_nor_t* nor,
                                      nx_stm32_fsmc_t* bus, size_t index,
                                      const nx_stm32_fsmc_nor_config_t* config);
/** \brief Close only after request/pulse and every face caller have joined.
 * \param[in,out] nor: Same serialized stable endpoint.
 * \return SUCCESS or BUSY/IO retaining exact bank lease and storage.
 * \note Does not stop the shared FSMC or restore Platform pads.
 */
nx_result_t nx_stm32_fsmc_nor_close(nx_stm32_fsmc_nor_t* nor);
/** \brief Shared immutable same-state synchronous readonly and async faces. */
extern const nx_flash_ops_t nx_stm32_fsmc_nor_flash_ops;
extern const nx_flash_async_ops_t nx_stm32_fsmc_nor_async_ops;
#ifdef __cplusplus
}
#endif
#endif
