/**
 * \file            stm32f407_window_watchdog.h
 * \brief           Exclusive WWDG window and irreversible activation owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_WINDOW_WATCHDOG_H
#define NEXUS_STM32F407_WINDOW_WATCHDOG_H
#include "nexus/io/watchdog.h"
#include "stm32f4xx.h"

/** \brief Only retained cold receipts and the selected reload occupy RAM. */
typedef struct {
    uint16_t saved_cfr;
    uint8_t window_ticks;
    uint8_t reload;
    uint8_t effects;
    uint8_t divider;
} nx_stm32_window_watchdog_state_t;

#ifdef __cplusplus
extern "C" {
#endif
extern const nx_watchdog_ops_t nx_stm32_window_watchdog_ops;
/** \brief Read-only admission; window_ticks is in 1..62, never a timeout. */
nx_result_t
nx_stm32_window_watchdog_admit(const nx_stm32_window_watchdog_state_t* port,
                               uint8_t window_ticks);
/** \brief Acquire a stopped controller without activating its watchdog. */
nx_result_t
nx_stm32_window_watchdog_prepare(nx_stm32_window_watchdog_state_t* port,
                                 uint8_t window_ticks);
/** \brief Restore only unactivated preparation; otherwise retain all effects.
 */
nx_result_t
nx_stm32_window_watchdog_stop(nx_stm32_window_watchdog_state_t* port);
#ifdef __cplusplus
}
#endif
#endif
