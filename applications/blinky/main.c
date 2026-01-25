/**
 * \file            main.c
 * \brief           Blinky Example Application
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-25
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This example demonstrates basic GPIO output control by
 *                  blinking LEDs in sequence. It shows how to:
 *                  - Initialize the Nexus HAL
 *                  - Get GPIO devices using the factory interface
 *                  - Toggle GPIO outputs
 *                  - Use OSAL delay functions
 *
 * \note            GPIO pins are configured via Kconfig at compile-time.
 *                  Default configuration uses GPIOA pins 0-2 and GPIOB pin 0.
 */

#include "hal/nx_hal.h"
#include "osal/osal.h"

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define BLINK_DELAY_MS 500 /**< Blink delay in milliseconds */

/*---------------------------------------------------------------------------*/
/* Main Entry Point                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main entry point
 * \details         Initializes HAL and OSAL, then blinks LEDs in sequence
 */
int main(void) {
    nx_status_t status;

    /* Initialize OSAL (must be first) */
    if (osal_init() != OSAL_OK) {
        while (1) {
            /* OSAL initialization failed */
        }
    }

    /* Initialize HAL */
    status = nx_hal_init();
    if (status != NX_OK) {
        while (1) {
            /* HAL initialization failed */
        }
    }

    /* Get GPIO devices (configured via Kconfig) */
    nx_gpio_write_t* led0 = nx_factory_gpio_write('A', 0);
    nx_gpio_write_t* led1 = nx_factory_gpio_write('A', 1);
    nx_gpio_write_t* led2 = nx_factory_gpio_write('A', 2);
    nx_gpio_write_t* led3 = nx_factory_gpio_write('B', 0);

    /* Check if all LEDs are available */
    if (!led0 || !led1 || !led2 || !led3) {
        while (1) {
            /* GPIO device not available - check Kconfig */
        }
    }

    /* Main loop: blink LEDs in sequence */
    while (1) {
        /* LED 0 */
        led0->toggle(led0);
        osal_task_delay(BLINK_DELAY_MS);

        /* LED 1 */
        led1->toggle(led1);
        osal_task_delay(BLINK_DELAY_MS);

        /* LED 2 */
        led2->toggle(led2);
        osal_task_delay(BLINK_DELAY_MS);

        /* LED 3 */
        led3->toggle(led3);
        osal_task_delay(BLINK_DELAY_MS);
    }

    /* Cleanup (never reached) */
    nx_factory_gpio_release((nx_gpio_t*)led0);
    nx_factory_gpio_release((nx_gpio_t*)led1);
    nx_factory_gpio_release((nx_gpio_t*)led2);
    nx_factory_gpio_release((nx_gpio_t*)led3);
    nx_hal_deinit();

    return 0;
}
