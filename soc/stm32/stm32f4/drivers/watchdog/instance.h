/**
 * \file            instance.h
 * \brief           Private STM32F4 independent watchdog owner and methods
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_WATCHDOG_INSTANCE_H
#define NX_STM32F4_WATCHDOG_INSTANCE_H

#include "nexus/io/watchdog.h"
#include "private/model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    IWDG_TypeDef* registers;
    FLASH_TypeDef* flash;
    DBGMCU_TypeDef* debug;
    RCC_TypeDef* rcc;
    nx_watchdog_state_t state;
    uint32_t poll_limit;
    uint32_t saved_debug;
    bool lsi_owned;
    bool debug_owned;
    bool preparation_retained;
} nx_stm32_watchdog_state_t;

extern const nx_watchdog_ops_t nx_stm32_watchdog_ops;
nx_result_t nx_stm32_watchdog_enable(void* context, uint32_t timeout_us,
                                     bool debug_freeze,
                                     nx_watchdog_state_t* state);
nx_result_t nx_stm32_watchdog_feed(void* context);
nx_result_t nx_stm32_watchdog_state(const void* context,
                                    nx_watchdog_state_t* state);
/** \brief Retry owned preparation cleanup; enabled IWDG stays irreversible. */
nx_result_t nx_stm32_watchdog_stop(nx_stm32_watchdog_state_t* port);

#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F4_WATCHDOG_INSTANCE_H */
