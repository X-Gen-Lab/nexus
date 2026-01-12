/**
 * \file            system_stm32f4xx.c
 * \brief           STM32F4xx System Initialization
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "stm32f4xx.h"

/**
 * \brief           System core clock frequency (default HSI = 16MHz)
 */
uint32_t SystemCoreClock = 16000000UL;

/**
 * \brief           AHB prescaler table
 */
static const uint8_t AHBPrescTable[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9
};

/**
 * \brief           APB prescaler table
 */
static const uint8_t APBPrescTable[8] = {
    0, 0, 0, 0, 1, 2, 3, 4
};

/**
 * \brief           System initialization
 * \note            Called from startup code before main()
 */
void SystemInit(void)
{
    /* FPU settings (Cortex-M4F) */
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    /* Enable CP10 and CP11 coprocessors */
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
#endif

    /* Reset RCC clock configuration to default state */
    /* Set HSION bit */
    RCC->CR |= (1UL << 0);

    /* Reset CFGR register */
    RCC->CFGR = 0x00000000UL;

    /* Reset HSEON, CSSON and PLLON bits */
    RCC->CR &= ~((1UL << 16) | (1UL << 19) | (1UL << 24));

    /* Reset PLLCFGR register */
    RCC->PLLCFGR = 0x24003010UL;

    /* Reset HSEBYP bit */
    RCC->CR &= ~(1UL << 18);

    /* Disable all interrupts */
    RCC->CIR = 0x00000000UL;

    /* Configure vector table location */
#ifdef VECT_TAB_SRAM
    SCB->VTOR = SRAM_BASE;
#else
    SCB->VTOR = FLASH_BASE;
#endif
}
