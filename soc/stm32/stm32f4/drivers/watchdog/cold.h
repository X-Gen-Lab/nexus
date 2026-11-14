/**
 * \file            cold.h
 * \brief           Static independent watchdog preparation
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_DRIVERS_WATCHDOG_COLD_H
#define NEXUS_STM32F4_DRIVERS_WATCHDOG_COLD_H

#include "drivers/watchdog/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

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
