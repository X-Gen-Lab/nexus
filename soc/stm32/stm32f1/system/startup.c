/**
 * \file            startup.c
 * \brief           F1 CPU reset entry before C storage is initialized.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_config.h"
#include "stm32f1xx.h"

#ifndef NEXUS_CLOCK_SOURCE_HSI
#error "STM32F1 startup requires the selected HSI clock plan"
#endif
_Static_assert(NEXUS_CORE_HZ == 8000000U && NEXUS_CLOCK_INPUT_HZ == 8000000U,
               "STM32F1 startup requires HSI8");
_Static_assert(NEXUS_CPU_EXTERNAL_IRQ_COUNT == 43U &&
                   NEXUS_IRQ_PRIORITY_BITS == __NVIC_PRIO_BITS,
               "STM32F103xB assembly and selected vector differ");

extern const uint32_t g_pfnVectors[];

void SystemInit(void) {
    /* Use the linker-selected vector table, including an image offset.
     * The startup assembler has yet to populate .data or clear .bss. */
    SCB->VTOR = (uint32_t)(uintptr_t)g_pfnVectors;
    NVIC_SetPriorityGrouping(3U);
    __DSB();
    __ISB();
}
