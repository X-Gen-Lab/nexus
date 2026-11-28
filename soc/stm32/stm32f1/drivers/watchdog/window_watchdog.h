/**
 * \file            window_watchdog.h
 * \brief           Private STM32F103xB window watchdog owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_WINDOW_WATCHDOG_H
#define NEXUS_STM32F1_WINDOW_WATCHDOG_H

#include "nexus/io/watchdog.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Zero-initialized owner retained until reset after activation. */
typedef struct {
    uint16_t config;
    uint8_t window_ticks;
    uint8_t reload;
    uint8_t effects;
} nx_stm32f1_wwdg_t;

extern const nx_watchdog_ops_t nx_stm32f1_wwdg_ops;

/** \brief Admit an exclusive cold owner without writes. */
nx_result_t nx_stm32f1_wwdg_admit(const nx_stm32f1_wwdg_t* port,
                                  uint8_t window_ticks);

/** \brief Acquire the gate and prove reset-wide polling admission. */
nx_result_t nx_stm32f1_wwdg_prepare(nx_stm32f1_wwdg_t* port,
                                    uint8_t window_ticks);

/** \brief Restore any reversible state; an activation requires reset. */
nx_result_t nx_stm32f1_wwdg_close(nx_stm32f1_wwdg_t* port);

#ifdef __cplusplus
}
#endif
#endif
