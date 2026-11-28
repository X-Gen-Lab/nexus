/**
 * \file            alarm.h
 * \brief           Private F103xB retained RTC raw comparator session.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F1_RTC_ALARM_H
#define NEXUS_STM32F1_RTC_ALARM_H

#include "drivers/rtc/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Uncopied state, initially zero, for one exclusive RTC/EXTI17 owner.
 */
typedef struct {
    uint32_t previous;
    nx_stm32f1_rtc_t backup;
    uint8_t priority;
    volatile uint8_t effects;
} nx_stm32f1_rtc_alarm_t;

/** \brief Adopt retained LSE and claim an unused EXTI17 alarm vector.
 * \note Call in serialized privileged Thread. Close even after failed prepare
 *       if a cleanup can still be pending.
 */
nx_result_t nx_stm32f1_rtc_alarm_prepare(nx_stm32f1_rtc_alarm_t* state,
                                         uint8_t priority);

/** \brief Replace the retained write-only comparator with a future raw value.
 * \note The previous comparator is unrecoverable. The replacement survives
 *       stop, with EXTI17 withdrawn. No divider, time unit or epoch is implied.
 *       Call again when the counter advances. The first sample and unchanged
 *       samples return empty. Keep at least four raw ticks of lead time.
 */
nx_result_t nx_stm32f1_rtc_alarm_arm(nx_stm32f1_rtc_alarm_t* state,
                                     uint32_t target);

/** \brief Consume one event corroborated by both RTC and EXTI17. */
nx_result_t nx_stm32f1_rtc_alarm_take(nx_stm32f1_rtc_alarm_t* state);

/** \brief Withdraw and drain only the owned alarm delivery path. */
nx_result_t nx_stm32f1_rtc_alarm_stop(nx_stm32f1_rtc_alarm_t* state);

/** \brief Restore the vector priority and release the backup interface. */
nx_result_t nx_stm32f1_rtc_alarm_close(nx_stm32f1_rtc_alarm_t* state);

/** \brief Quiesce only the dedicated alarm vector pending foreground drain. */
void nx_stm32f1_rtc_alarm_irq(nx_stm32f1_rtc_alarm_t* state);

#ifdef __cplusplus
}
#endif
#endif
