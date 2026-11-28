/**
 * \file            stm32f103rb_wwdg_warning.h
 * \brief           Explicit NUCLEO-F103RB retained window-watchdog witness.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F103RB_WWDG_EARLY_WARNING_H
#define NEXUS_STM32F103RB_WWDG_EARLY_WARNING_H

#include "nexus/io/watchdog.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Borrow BKP_DR1 after Platform starts and before watchdog activation.
 * \note Close any partial start before stopping Platform.
 */
nx_result_t nx_stm32f103rb_wwdg_warning_prepare(void);

/** \brief Read and consume the retained early-warning IRQ witness. */
nx_result_t nx_stm32f103rb_wwdg_warning_take(bool* observed);

/** \brief Arm the warning for the already activated factory watchdog.
 * \note A reset is required after any attempt that reaches the EWI write.
 */
nx_result_t nx_stm32f103rb_wwdg_warning_arm(const nx_watchdog_port_t* watchdog,
                                            uint8_t irq_priority);

/** \brief Release the backup word and clocks before a sticky arm attempt. */
nx_result_t nx_stm32f103rb_wwdg_warning_close(void);

#ifdef __cplusplus
}
#endif
#endif
