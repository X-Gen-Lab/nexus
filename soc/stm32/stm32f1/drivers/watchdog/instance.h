/**
 * \file            instance.h
 * \brief           Private STM32F103xB independent watchdog owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_WATCHDOG_INSTANCE_H
#define NEXUS_STM32F1_WATCHDOG_INSTANCE_H

#include "nexus/io/watchdog.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Zero-initialized singleton; generation provides one owner. */
typedef struct {
    uint8_t effects;
} nx_stm32f1_watchdog_state_t;

extern const nx_watchdog_ops_t nx_stm32f1_watchdog_ops;

/** \brief Publish the owner from privileged unmasked Thread with no MMIO
 * writes. */
nx_result_t nx_stm32f1_watchdog_prepare(nx_stm32f1_watchdog_state_t* port);

/** \brief Retry owned LSI cleanup without changing running watchdog state.
 * \note Privileged unmasked Thread; returns UNSUPPORTED on a retained
 * irreversible software or option-byte start. Caller must keep the existing
 * receipt and decide whether feeding or resetting is appropriate.
 */
nx_result_t nx_stm32f1_watchdog_close(nx_stm32f1_watchdog_state_t* port);

#ifdef __cplusplus
}
#endif
#endif
