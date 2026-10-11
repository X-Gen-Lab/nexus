/**
 * \file            tick.c
 * \brief           Strong external Tick setup selected by typed OS policy
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/tick.h"
#include "FreeRTOS.h"

/* The maintained ports expose a weak setup hook. Linking this selected object
 * makes the external source mandatory rather than silently using SysTick. */
void vPortSetupTimerInterrupt(void);

void vPortSetupTimerInterrupt(void) {
    nx_freertos_external_tick_setup((uint32_t)configTICK_RATE_HZ);
}
