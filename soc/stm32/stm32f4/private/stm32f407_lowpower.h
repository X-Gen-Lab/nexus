/**
 * \file            stm32f407_lowpower.h
 * \brief           Optional private TIM2 CC1 and SysTick lifecycle binding
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_LOWPOWER_H
#define NX_STM32F407_LOWPOWER_H
#include <stdbool.h>
/** \brief True while tick pause or alarm ownership prevents clock release. */
bool nx_stm32_lowpower_busy(void);
/** \brief Dispatch only the selected alarm; overflow remains timebase-owned. */
void nx_stm32_lowpower_alarm_irq(void);
/** \brief Selected SysTick wrapper restores full reload before kernel entry. */
void SysTick_Handler(void);
#endif /* NX_STM32F407_LOWPOWER_H */
