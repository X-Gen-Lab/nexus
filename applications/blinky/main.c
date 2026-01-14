/**
 * \file            main.c
 * \brief           Blinky Example Application
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \note            This example blinks an LED on STM32F4 Discovery board.
 *                  LED: PD12 (Green), PD13 (Orange), PD14 (Red), PD15 (Blue)
 */

#include "hal/hal.h"

/**
 * \brief           LED pin definitions (STM32F4 Discovery)
 */
#define LED_GREEN_PORT  HAL_GPIO_PORT_D
#define LED_GREEN_PIN   12
#define LED_ORANGE_PORT HAL_GPIO_PORT_D
#define LED_ORANGE_PIN  13
#define LED_RED_PORT    HAL_GPIO_PORT_D
#define LED_RED_PIN     14
#define LED_BLUE_PORT   HAL_GPIO_PORT_D
#define LED_BLUE_PIN    15

/**
 * \brief           Blink delay in milliseconds
 */
#define BLINK_DELAY_MS 500

/**
 * \brief           Initialize LEDs
 * \return          HAL_OK on success
 */
static hal_status_t led_init(void) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    /* Initialize all LEDs */
    if (hal_gpio_init(LED_GREEN_PORT, LED_GREEN_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }
    if (hal_gpio_init(LED_ORANGE_PORT, LED_ORANGE_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }
    if (hal_gpio_init(LED_RED_PORT, LED_RED_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }
    if (hal_gpio_init(LED_BLUE_PORT, LED_BLUE_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }

    return HAL_OK;
}

/**
 * \brief           Main entry point
 * \return          Should never return
 */
int main(void) {
    /* Initialize HAL system */
    hal_system_init();

    /* Initialize LEDs */
    if (led_init() != HAL_OK) {
        /* Error: stay in infinite loop */
        while (1) {
            /* Error state */
        }
    }

    /* Main loop: blink LEDs in sequence */
    while (1) {
        /* Green LED */
        hal_gpio_toggle(LED_GREEN_PORT, LED_GREEN_PIN);
        hal_delay_ms(BLINK_DELAY_MS);

        /* Orange LED */
        hal_gpio_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
        hal_delay_ms(BLINK_DELAY_MS);

        /* Red LED */
        hal_gpio_toggle(LED_RED_PORT, LED_RED_PIN);
        hal_delay_ms(BLINK_DELAY_MS);

        /* Blue LED */
        hal_gpio_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
        hal_delay_ms(BLINK_DELAY_MS);
    }

    return 0;
}
