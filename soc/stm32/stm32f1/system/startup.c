/**
 * \file            startup.c
 * \brief           F1 CPU reset entry before C storage is initialized.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "system/clock_profile.h"

#if defined(NX_STM32F1_PROFILE_CORE_HZ)
_Static_assert(NEXUS_CORE_HZ == NX_STM32F1_PROFILE_CORE_HZ &&
                   NEXUS_CLOCK_INPUT_HZ == NX_STM32F1_PROFILE_INPUT_HZ,
               "STM32F1 startup requires the reviewed external clock plan");
#elif defined(NEXUS_CLOCK_SOURCE_HSI)
_Static_assert(NEXUS_CORE_HZ == 8000000U && NEXUS_CLOCK_INPUT_HZ == 8000000U,
               "STM32F1 HSI startup requires the reviewed HSI8 plan");
#else
#error "STM32F1 startup requires a reviewed clock plan"
#endif

#if defined(STM32F100xB)
#define DEVICE_LAST_IRQ TIM7_IRQn
#elif defined(STM32F100xE)
#define DEVICE_LAST_IRQ DMA2_Channel5_IRQn
#elif defined(STM32F101x6) || defined(STM32F101xB)
#define DEVICE_LAST_IRQ RTC_Alarm_IRQn
#elif defined(STM32F102x6) || defined(STM32F102xB) || defined(STM32F103x6) ||  \
    defined(STM32F103xB)
#define DEVICE_LAST_IRQ USBWakeUp_IRQn
#elif defined(STM32F101xE) || defined(STM32F101xG) || defined(STM32F103xE) ||  \
    defined(STM32F103xG)
#define DEVICE_LAST_IRQ DMA2_Channel4_5_IRQn
#elif defined(STM32F105xC) || defined(STM32F107xC)
#define DEVICE_LAST_IRQ OTG_FS_IRQn
#else
#error "STM32F1 startup requires a reviewed device vector"
#endif
_Static_assert(NEXUS_CPU_EXTERNAL_IRQ_COUNT == (unsigned)DEVICE_LAST_IRQ + 1U &&
                   NEXUS_IRQ_PRIORITY_BITS == __NVIC_PRIO_BITS,
               "STM32F1 CMSIS and selected vector differ");
_Static_assert(NEXUS_ARCH_MPU_VERSION == (__MPU_PRESENT ? 7U : 0U) &&
                   NEXUS_CPU_MPU_REGIONS == (__MPU_PRESENT ? 8U : 0U),
               "STM32F1 CMSIS and selected MPU differ");

extern const uint32_t g_pfnVectors[];

void SystemInit(void) {
    /* Use the linker-selected vector table, including an image offset.
     * The startup assembler has yet to populate .data or clear .bss. */
    SCB->VTOR = (uint32_t)(uintptr_t)g_pfnVectors;
    NVIC_SetPriorityGrouping(3U);
    __DSB();
    __ISB();
}
