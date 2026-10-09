/* Idle F407 clock teardown. No HAL tick or scheduler dependency. */
#include "clock/stm32_clock.h"
#include "stm32f4xx_hal.h"
#include "nexus_config.h"
#include <stdbool.h>

static bool clock_wait_bits(volatile uint32_t* reg, uint32_t mask,
                            uint32_t expected) {
    /* This is a poll-count bound, not a millisecond deadline. It continues to
     * work after the platform timebase has stopped and with IRQs masked. */
    for (uint32_t n = 0; n < 1000000U; ++n) {
        if ((*reg & mask) == expected) {
            return true;
        }
    }
    return false;
}

nx_status_t nx_stm32f407_clock_release(void) {
    RCC->CR |= RCC_CR_HSION;
    if (!clock_wait_bits(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY)) {
        SystemCoreClockUpdate();
        return NX_ERR_TIMEOUT;
    }

    /* Keep the currently valid bus dividers until SYSCLK really is HSI. A
     * failed switch must never shut down the PLL currently feeding the CPU. */
    RCC->CFGR &= ~RCC_CFGR_SW;
    if (!clock_wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI)) {
        SystemCoreClockUpdate();
        return NX_ERR_TIMEOUT;
    }
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    SystemCoreClockUpdate();
    if ((RCC->CFGR & (RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) != 0U ||
        SystemCoreClock != NX_CONFIG_STM32_HSI_VALUE) {
        return NX_ERR_HARDWARE;
    }

    /* I2S has its own PLL. Both must have stopped before releasing HSE.
     * Preserve Flash latency / caches and voltage scaling: lowering their
     * settings is unnecessary for a safe idle state and restart. */
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_PLLI2SON);
    if (!clock_wait_bits(&RCC->CR,
                         RCC_CR_PLLON | RCC_CR_PLLRDY | RCC_CR_PLLI2SON |
                             RCC_CR_PLLI2SRDY,
                         0U)) {
        return NX_ERR_TIMEOUT;
    }
    RCC->CR &= ~(RCC_CR_CSSON | RCC_CR_HSEON);
    if (!clock_wait_bits(&RCC->CR, RCC_CR_HSEON | RCC_CR_HSERDY, 0U)) {
        return NX_ERR_TIMEOUT;
    }
    return NX_OK;
}
