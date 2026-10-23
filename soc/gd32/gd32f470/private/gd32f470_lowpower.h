/**
 * \file            gd32f470_lowpower.h
 * \brief           Optional private TIMER1 CH0 and SysTick lifecycle binding
 * \author          Nexus Team
 */
#ifndef NX_GD32F470_LOWPOWER_H
#define NX_GD32F470_LOWPOWER_H
#include <stdbool.h>
/** \brief True while tick pause or alarm ownership prevents clock release. */
bool nx_gd32_lowpower_busy(void);
/** \brief Dispatch only the selected alarm; overflow remains timebase-owned. */
void nx_gd32_lowpower_alarm_irq(void);
/** \brief Selected SysTick wrapper restores full reload before kernel entry. */
void SysTick_Handler(void);
#endif /* NX_GD32F470_LOWPOWER_H */
