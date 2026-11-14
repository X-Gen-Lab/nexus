/**
 * \file            cold.h
 * \brief           Static physical Flash preparation and withdrawal
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_DRIVERS_FLASH_COLD_H
#define NEXUS_STM32F4_DRIVERS_FLASH_COLD_H

#include "drivers/flash/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Bind exact physical Flash facts without touching Flash registers.
 * \param[in,out] port: Serialized idle owner, no outstanding Flash call.
 * \param[in] geometry: Static full-density selected geometry.
 * \param[in] supply_mv: Authored supply assumption for programming width.
 * \return Success or rejection before state or hardware changes.
 * \note Does not authorize a product partition or start a physical pulse.
 */
nx_result_t nx_stm32_flash_prepare(nx_stm32_flash_state_t* port,
                                   const nx_flash_geometry_t* geometry,
                                   uint32_t supply_mv);

/** \brief Observe idle Flash before Platform releases the system.
 * \param[in,out] port: Serialized owner whose callers have joined.
 * \return Success or busy retaining the live physical pulse responsibility.
 */
nx_result_t nx_stm32_flash_close(nx_stm32_flash_state_t* port);

#ifdef __cplusplus
}
#endif
#endif
