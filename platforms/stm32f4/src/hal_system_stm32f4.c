/**
 * \file            hal_system_stm32f4.c
 * \brief           STM32F4 System HAL Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \par             Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 13.3, 13.8
 */

#include "compiler_abstraction.h"
#include "core_config.h"
#include "hal/hal_def.h"
#include "stm32f4xx.h"

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

/**
 * \brief           System clock frequency (168 MHz for STM32F407)
 */
#define SYSTEM_CLOCK_FREQ 168000000UL

/**
 * \brief           HSE crystal frequency (8 MHz typical)
 */
#define HSE_FREQ 8000000UL

/**
 * \brief           PLL configuration for 168 MHz
 *                  VCO = HSE * PLLN / PLLM = 8 * 336 / 8 = 336 MHz
 *                  SYSCLK = VCO / PLLP = 336 / 2 = 168 MHz
 *                  USB/SDIO = VCO / PLLQ = 336 / 7 = 48 MHz
 */
#define PLL_M 8
#define PLL_N 336
#define PLL_P 2 /* PLLP = 2 (value 0 in register) */
#define PLL_Q 7

/**
 * \brief           Flash latency for 168 MHz at 3.3V
 */
#define FLASH_LATENCY_168MHZ 5

/**
 * \brief           SysTick counter for delay functions
 */
static volatile uint32_t systick_counter = 0;

/**
 * \brief           System initialization flag
 */
static bool system_initialized = false;

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Configure system clock to 168 MHz using HSE and PLL
 * \note            HSE (8 MHz) -> PLL -> 168 MHz SYSCLK
 *                  AHB = 168 MHz, APB1 = 42 MHz, APB2 = 84 MHz
 * \return          HAL_OK on success, error code otherwise
 *
 * \par             Requirements: 9.1
 */
static hal_status_t system_clock_config(void) {
    uint32_t timeout;

    /* Enable HSE (High Speed External oscillator) */
    RCC->CR |= RCC_CR_HSEON;

    /* Wait for HSE to be ready with timeout */
    timeout = 0x5000;
    while (!(RCC->CR & RCC_CR_HSERDY)) {
        if (--timeout == 0) {
            return HAL_ERROR_TIMEOUT;
        }
    }

    /* Enable Power interface clock */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    /* Set voltage regulator scale 1 for maximum performance */
    PWR->CR |= PWR_CR_VOS;

    /* Configure Flash prefetch, instruction cache, data cache, and wait states
     */
    FLASH->ACR = FLASH_ACR_PRFTEN |     /* Prefetch enable */
                 FLASH_ACR_ICEN |       /* Instruction cache enable */
                 FLASH_ACR_DCEN |       /* Data cache enable */
                 FLASH_ACR_LATENCY_5WS; /* 5 wait states for 168 MHz */

    /* Configure bus prescalers:
     * HCLK (AHB)  = SYSCLK / 1 = 168 MHz
     * PCLK1 (APB1) = HCLK / 4 = 42 MHz (max 42 MHz)
     * PCLK2 (APB2) = HCLK / 2 = 84 MHz (max 84 MHz)
     */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;  /* AHB prescaler = 1 */
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV4; /* APB1 prescaler = 4 */
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2; /* APB2 prescaler = 2 */

    /* Configure PLL:
     * PLL source = HSE
     * VCO input = HSE / PLLM = 8 / 8 = 1 MHz (must be 1-2 MHz)
     * VCO output = VCO input * PLLN = 1 * 336 = 336 MHz (must be 192-432 MHz)
     * SYSCLK = VCO output / PLLP = 336 / 2 = 168 MHz
     * USB/SDIO = VCO output / PLLQ = 336 / 7 = 48 MHz
     */
    RCC->PLLCFGR = (PLL_M << RCC_PLLCFGR_PLLM_Pos) |
                   (PLL_N << RCC_PLLCFGR_PLLN_Pos) |
                   (((PLL_P >> 1) - 1) << RCC_PLLCFGR_PLLP_Pos) |
                   RCC_PLLCFGR_PLLSRC_HSE | (PLL_Q << RCC_PLLCFGR_PLLQ_Pos);

    /* Enable PLL */
    RCC->CR |= RCC_CR_PLLON;

    /* Wait for PLL to be ready with timeout */
    timeout = 0x5000;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {
        if (--timeout == 0) {
            return HAL_ERROR_TIMEOUT;
        }
    }

    /* Select PLL as system clock source */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    /* Wait for PLL to be used as system clock with timeout */
    timeout = 0x5000;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
        if (--timeout == 0) {
            return HAL_ERROR_TIMEOUT;
        }
    }

    /* Update SystemCoreClock variable */
    SystemCoreClock = SYSTEM_CLOCK_FREQ;

    return HAL_OK;
}

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

/**
 * \brief           Initialize HAL system
 * \note            Configures system clock, SysTick, and FPU
 * \return          HAL_OK on success
 *
 * \par             Requirements: 9.1, 9.2, 9.6
 */
hal_status_t hal_system_init(void) {
    hal_status_t status;

    /* Prevent double initialization */
    if (system_initialized) {
        return HAL_ERROR_ALREADY_INIT;
    }

#if CORE_HAS_FPU
    /* Enable FPU (Cortex-M4F) - must be done before any FP operations */
    /* Set CP10 and CP11 to full access */
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    HAL_DSB();
    HAL_ISB();
#endif

    /* Configure system clock to 168 MHz */
    status = system_clock_config();
    if (status != HAL_OK) {
        return status;
    }

    /* Configure SysTick for 1ms tick */
    if (SysTick_Config(SystemCoreClock / 1000) != 0) {
        return HAL_ERROR;
    }

    /* Set SysTick priority to lowest (15 for 4-bit priority) */
    NVIC_SetPriority(SysTick_IRQn, CORE_NVIC_PRIO_LOWEST);

    system_initialized = true;
    return HAL_OK;
}

/**
 * \brief           Get system tick count (milliseconds)
 * \return          Current tick count
 *
 * \par             Requirements: 9.3
 */
uint32_t hal_get_tick(void) {
    return systick_counter;
}

/**
 * \brief           Delay in milliseconds
 * \param[in]       ms: Delay time in milliseconds
 *
 * \par             Requirements: 9.4
 */
void hal_delay_ms(uint32_t ms) {
    uint32_t start = systick_counter;

    /* Handle potential overflow by checking difference */
    while ((systick_counter - start) < ms) {
        /* Wait - could add WFI for power saving in future */
    }
}

/**
 * \brief           Delay in microseconds (approximate)
 * \param[in]       us: Delay time in microseconds
 * \note            This is a busy-wait implementation using cycle counting.
 *                  Accuracy depends on compiler optimization and cache state.
 *
 * \par             Requirements: 9.5
 */
void hal_delay_us(uint32_t us) {
    /* Calculate number of loop iterations needed
     * Each loop iteration takes approximately 4 cycles on Cortex-M4
     * cycles = (SystemCoreClock / 1000000) * us
     * iterations = cycles / 4
     */
    uint32_t cycles = (SystemCoreClock / 1000000UL) * us;
    uint32_t iterations = cycles / 4;

    /* Prevent optimization from removing the loop */
    while (iterations--) {
        HAL_NOP();
    }
}

#if CORE_HAS_FPU
/**
 * \brief           Enable FPU (Floating Point Unit)
 * \note            Enables CP10 and CP11 coprocessors for full access.
 *                  This is automatically called by hal_system_init() but
 *                  can be called explicitly if needed before hal_system_init().
 *
 * \par             Requirements: 9.6, 13.3, 13.8
 */
void hal_fpu_enable(void) {
    /* Set CP10 and CP11 to full access (bits 20-23) */
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));

    /* Ensure the write completes before any FP instructions */
    HAL_DSB();
    HAL_ISB();
}

/**
 * \brief           Check if FPU is enabled
 * \return          true if FPU is enabled, false otherwise
 */
bool hal_fpu_is_enabled(void) {
    /* Check if CP10 and CP11 have full access */
    return ((SCB->CPACR & ((3UL << 10 * 2) | (3UL << 11 * 2))) ==
            ((3UL << 10 * 2) | (3UL << 11 * 2)));
}
#endif /* CORE_HAS_FPU */

/**
 * \brief           System reset
 */
void hal_system_reset(void) {
    NVIC_SystemReset();
}

/**
 * \brief           Enter critical section (disable interrupts)
 * \return          Previous interrupt state (PRIMASK value)
 * \note            Uses compiler abstraction for multi-compiler support.
 *                  Nested critical sections are supported.
 *
 * \par             Requirements: 9.7
 */
uint32_t hal_enter_critical(void) {
    uint32_t primask = hal_get_primask();
    HAL_DISABLE_IRQ();
    return primask;
}

/**
 * \brief           Exit critical section (restore interrupts)
 * \param[in]       state: Previous interrupt state from hal_enter_critical()
 * \note            Uses compiler abstraction for multi-compiler support.
 *
 * \par             Requirements: 9.7
 */
void hal_exit_critical(uint32_t state) {
    hal_set_primask(state);
}

/*===========================================================================*/
/* Interrupt handlers                                                         */
/*===========================================================================*/

/**
 * \brief           SysTick interrupt handler
 */
void SysTick_Handler(void) {
    systick_counter++;
}

/*===========================================================================*/
/* HAL Init/Deinit (Main Entry Points)                                        */
/*===========================================================================*/

/**
 * \brief           Initialize HAL layer
 * \note            This is the main entry point for HAL initialization.
 *                  It initializes the system (clock, SysTick, FPU) and
 *                  prepares all subsystems for use.
 * \return          HAL_OK on success, error code otherwise
 *
 * \par             Requirements: 9.1, 9.2
 */
hal_status_t hal_init(void) {
    hal_status_t status;

    /* Initialize system (clock, SysTick, FPU) */
    status = hal_system_init();
    if (status != HAL_OK) {
        return status;
    }

    /* Note: Individual peripheral drivers (GPIO, UART, SPI, I2C, Timer, ADC)
     * are initialized on-demand when their respective hal_xxx_init() functions
     * are called. This allows for flexible resource management and reduces
     * startup time by only initializing peripherals that are actually used.
     */

    return HAL_OK;
}

/**
 * \brief           Deinitialize HAL layer
 * \note            This function deinitializes the HAL layer and releases
 *                  all resources. After calling this function, hal_init()
 *                  must be called again before using any HAL functions.
 * \return          HAL_OK on success, error code otherwise
 *
 * \par             Requirements: 9.1, 9.2
 */
hal_status_t hal_deinit(void) {
    /* Disable SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    /* Reset tick counter */
    systick_counter = 0;

    /* Mark system as not initialized */
    system_initialized = false;

    /* Note: Individual peripheral drivers should be deinitialized by calling
     * their respective hal_xxx_deinit() functions before calling hal_deinit().
     * This function only handles system-level deinitialization.
     */

    return HAL_OK;
}
