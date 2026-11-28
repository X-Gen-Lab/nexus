/**
 * \file            connectivity_clock.h
 * \brief           Shared exact PLL2 and OTG FS clock receipt checks.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_CONNECTIVITY_USB_CLOCK_H
#define NEXUS_STM32F1_CONNECTIVITY_USB_CLOCK_H
#include "stm32f1xx.h"
#include "system/platform.h"

#define DWC_CLOCK_STATUS                                                       \
    (RCC_CR_PLLON | RCC_CR_PLLRDY | RCC_CR_PLL2ON | RCC_CR_PLL2RDY |           \
     RCC_CR_HSEON | RCC_CR_HSERDY)
#define DWC_CLOCK_FIELDS                                                       \
    (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |      \
     RCC_CFGR_PLLMULL | RCC_CFGR_OTGFSPRE)
#define DWC_CLOCK_VALUE (RCC_CFGR_SWS_PLL | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9)
#define DWC_PREDIV_FIELDS                                                      \
    (RCC_CFGR2_PREDIV1 | RCC_CFGR2_PREDIV2 | RCC_CFGR2_PLL2MUL |               \
     RCC_CFGR2_PREDIV1SRC)
#define DWC_PREDIV_VALUE                                                       \
    (RCC_CFGR2_PREDIV1_DIV5 | RCC_CFGR2_PREDIV2_DIV5 | RCC_CFGR2_PLL2MUL8 |    \
     RCC_CFGR2_PREDIV1SRC_PLL2)
static inline bool nx_stm32f1_connectivity_usb_clock_live(void) {
    return nx_stm32f1_system_live() &&
           (RCC->CR & (DWC_CLOCK_STATUS | RCC_CR_HSEBYP)) == DWC_CLOCK_STATUS &&
           (RCC->CFGR & DWC_CLOCK_FIELDS) == DWC_CLOCK_VALUE &&
           (RCC->CFGR2 & DWC_PREDIV_FIELDS) == DWC_PREDIV_VALUE;
}

#endif
