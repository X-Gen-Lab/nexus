/**
 * \file            stm32f407_basic_cold.h
 * \brief           Static GPIO, Flash and watchdog construction contracts
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_BASIC_COLD_H
#define NEXUS_STM32F407_BASIC_COLD_H
#include "stm32f407_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Immutable exact single-bank GPIO authorization. */
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    uint32_t initial;
    bool output;
} nx_stm32_gpio_config_t;

/** \brief Bind and initialize GPIO after Platform has admitted its pads.
 * \param[in,out] port: Serialized cold owner; live rebind is rejected.
 * \param[in] config: Static exact bank, mask, direction and initial level.
 * \return Success or error; malformed configuration changes no hardware.
 * \note Platform retains the original pin snapshots and shared bank clock.
 */
nx_result_t nx_stm32_gpio_prepare(nx_stm32_gpio_state_t* port,
                                  const nx_stm32_gpio_config_t* config);

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

/** \brief Bind the fixed IWDG without starting irreversible counting.
 * \param[in,out] port: Cold owner without retained preparation or activation.
 * \param[in] poll_limit: Nonzero bounded preparation polling budget.
 * \return Success or error without acquiring an oscillator or changing MMIO.
 * \note Existing watchdog stop retains its irreversible activation contract.
 */
nx_result_t nx_stm32_watchdog_prepare(nx_stm32_watchdog_state_t* port,
                                      uint32_t poll_limit);

#ifdef __cplusplus
}
#endif
#endif
