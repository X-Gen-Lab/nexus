/**
 * \file            early_warning.h
 * \brief           Private F103RB window-watchdog backup witness.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_WWDG_EARLY_WARNING_H
#define NEXUS_STM32F1_WWDG_EARLY_WARNING_H

#include "nexus/io/watchdog.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Zero-initialized cold receipt. The witness itself lives in BKP DR1.
 */
typedef struct {
    uint8_t effects;
} nx_stm32f1_wwdg_ewi_t;

/** \brief Acquire exclusive backup gates and write access before activation. */
nx_result_t nx_stm32f1_wwdg_ewi_prepare(nx_stm32f1_wwdg_ewi_t* warning);
/** \brief Consume the prior boot's observed IRQ marker, preserving foreign
 * data. */
nx_result_t nx_stm32f1_wwdg_ewi_take(const nx_stm32f1_wwdg_ewi_t* warning,
                                     bool* observed);
/** \brief Seal the backup receipt and enable the already activated WWDG's IRQ.
 */
nx_result_t nx_stm32f1_wwdg_ewi_arm(nx_stm32f1_wwdg_ewi_t* warning,
                                    const nx_watchdog_port_t* watchdog,
                                    uint8_t priority);
/** \brief Record the actual IRQ without reloading or delaying hardware reset.
 */
nx_result_t nx_stm32f1_wwdg_ewi_irq(const nx_stm32f1_wwdg_ewi_t* warning);
/** \brief Return pre-arm effects. After a sticky write attempt, only reset
 * releases. */
nx_result_t nx_stm32f1_wwdg_ewi_close(nx_stm32f1_wwdg_ewi_t* warning);

#ifdef __cplusplus
}
#endif
#endif
