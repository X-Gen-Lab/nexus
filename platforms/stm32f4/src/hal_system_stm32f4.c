/**
 * \file            hal_system_stm32f4.c
 * \brief           STM32F4 System HAL Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/hal_def.h"
#include "stm32f4xx.h"

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

/**
 * \brief           SysTick counter for delay functions
 */
static volatile uint32_t systick_counter = 0;

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

/**
 * \brief           Initialize HAL system
 * \return          HAL_OK on success
 */
hal_status_t hal_system_init(void)
{
    /* Configure SysTick for 1ms tick */
    if (SysTick_Config(SystemCoreClock / 1000) != 0) {
        return HAL_ERR_FAIL;
    }

    /* Set SysTick priority */
    NVIC_SetPriority(SysTick_IRQn, 15);

    return HAL_OK;
}

/**
 * \brief           Get system tick count (milliseconds)
 * \return          Current tick count
 */
uint32_t hal_get_tick(void)
{
    return systick_counter;
}

/**
 * \brief           Delay in milliseconds
 * \param[in]       ms: Delay time in milliseconds
 */
void hal_delay_ms(uint32_t ms)
{
    uint32_t start = systick_counter;
    while ((systick_counter - start) < ms) {
        /* Wait */
    }
}

/**
 * \brief           Delay in microseconds (approximate)
 * \param[in]       us: Delay time in microseconds
 * \note            This is a busy-wait implementation
 */
void hal_delay_us(uint32_t us)
{
    uint32_t cycles = (SystemCoreClock / 1000000UL) * us / 4;
    while (cycles--) {
        __asm volatile("nop");
    }
}

/**
 * \brief           System reset
 */
void hal_system_reset(void)
{
    NVIC_SystemReset();
}

/**
 * \brief           Enter critical section (disable interrupts)
 * \return          Previous interrupt state
 */
uint32_t hal_enter_critical(void)
{
    uint32_t primask;
    __asm volatile(
        "mrs %0, primask\n"
        "cpsid i\n"
        : "=r"(primask)
    );
    return primask;
}

/**
 * \brief           Exit critical section (restore interrupts)
 * \param[in]       state: Previous interrupt state
 */
void hal_exit_critical(uint32_t state)
{
    __asm volatile(
        "msr primask, %0\n"
        :
        : "r"(state)
    );
}

/*===========================================================================*/
/* Interrupt handlers                                                         */
/*===========================================================================*/

/**
 * \brief           SysTick interrupt handler
 */
void SysTick_Handler(void)
{
    systick_counter++;
}
