/**
 * \file            main.c
 * \brief           STM32 Minimal Boot Blinky Example
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-28
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This example demonstrates the STM32 minimal boot system by:
 *                  - Calling stm32_platform_init() to initialize the platform
 *                  - Configuring a GPIO pin for LED output
 *                  - Blinking the LED in an infinite loop
 *                  - Verifying that the system boots correctly and runs
 *
 * \note            This example uses the STM32 HAL library directly.
 *                  Default LED is on GPIOA Pin 5 (common on STM32 Nucleo
 * boards). Adjust LED_GPIO_PORT and LED_GPIO_PIN for your hardware.
 */

/*---------------------------------------------------------------------------*/
/* Includes                                                                  */
/*---------------------------------------------------------------------------*/

#include "boot/stm32_boot.h"
#include "stm32f4xx_hal.h"

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define LED_GPIO_PORT GPIOG       /**< LED GPIO port */
#define LED_GPIO_PIN  GPIO_PIN_13 /**< LED GPIO pin */
#define LED_GPIO_CLK_ENABLE                                                    \
    __HAL_RCC_GPIOG_CLK_ENABLE /**< GPIO clock enable                          \
                                */
#define BLINK_DELAY_MS 500     /**< Blink delay in milliseconds */

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize LED GPIO
 * \details         Configures the LED pin as output push-pull
 */
static void led_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO clock */
    LED_GPIO_CLK_ENABLE();

    /* Configure GPIO pin */
    GPIO_InitStruct.Pin = LED_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

    /* Set initial state to OFF */
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
}

/**
 * \brief           Simple delay function
 * \param[in]       ms: Delay in milliseconds
 * \details         Uses HAL_Delay which relies on SysTick configured by
 * HAL_Init
 */
static void delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}

/*---------------------------------------------------------------------------*/
/* Main Entry Point                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main entry point
 * \details         Demonstrates STM32 platform initialization and LED blinking
 * \return          0 on success (never reached)
 */
int main(void) {
    /* Initialize STM32 platform (HAL, clock, NVIC) */
    if (stm32_platform_init() != 0) {
        /* Platform initialization failed - enter error loop */
        while (1) {
            /* Error: Platform initialization failed */
        }
    }

    /* Initialize LED GPIO */
    led_init();

    /* Main loop: blink LED */
    while (1) {
        /* Toggle LED */
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);

        /* Delay */
        delay_ms(BLINK_DELAY_MS);
    }

    /* Never reached */
    return 0;
}

/*---------------------------------------------------------------------------*/
/* HAL Callbacks                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           HAL MSP initialization callback
 * \details         This function is called by HAL_Init() to perform low-level
 *                  initialization. Users can override this function for custom
 *                  initialization.
 */
void HAL_MspInit(void) {
    /* User can add custom initialization here */
    /* Example: Configure additional peripherals, enable clocks, etc. */
}
