/**
 * \file            nx_gpio_example.c
 * \brief           Nexus HAL GPIO Usage Example
 * \author          Nexus Team
 *
 * This example demonstrates how to use the Nexus HAL GPIO interface:
 * - Getting GPIO pins using the factory
 * - Configuring GPIO modes (input/output)
 * - Reading and writing GPIO states
 * - Runtime mode switching
 * - External interrupt (EXTI) configuration
 * - Pull-up/pull-down configuration
 *
 * \note            This example is for demonstration purposes and may need
 *                  adaptation for your specific platform and use case.
 */

#include "hal/nx_hal.h"
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* Example 1: Basic GPIO Output                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 1: Basic GPIO output control
 * \details         Demonstrates configuring a GPIO pin as output and
 *                  controlling its state (LED blinking example)
 */
void example_gpio_output_basic(void) {
    printf("=== Example 1: Basic GPIO Output ===\n");

    /* Get GPIO pin (Port A, Pin 5 - typical LED pin on many boards) */
    nx_gpio_t* led_pin = nx_factory_gpio(0, 5);
    if (!led_pin) {
        printf("Error: Failed to get GPIO pin\n");
        return;
    }

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = led_pin->get_lifecycle(led_pin);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize GPIO\n");
        nx_factory_gpio_release(led_pin);
        return;
    }

    /* Configure as output push-pull */
    nx_gpio_config_t config = {
        .mode = NX_GPIO_MODE_OUTPUT_PP,
        .pull = NX_GPIO_PULL_NONE,
        .speed = NX_GPIO_SPEED_LOW,
        .af_index = 0,
    };

    if (led_pin->set_config(led_pin, &config) != NX_OK) {
        printf("Error: Failed to configure GPIO\n");
        goto cleanup;
    }

    printf("Blinking LED...\n");

    /* Blink LED 5 times */
    for (int i = 0; i < 5; i++) {
        /* Turn LED on */
        led_pin->write(led_pin, 1);
        printf("LED ON\n");

        /* Simulate delay (platform-specific delay function needed) */
        for (volatile int j = 0; j < 1000000; j++)
            ;

        /* Turn LED off */
        led_pin->write(led_pin, 0);
        printf("LED OFF\n");

        /* Simulate delay */
        for (volatile int j = 0; j < 1000000; j++)
            ;
    }

    /* Alternative: Use toggle */
    printf("Using toggle function...\n");
    for (int i = 0; i < 5; i++) {
        led_pin->toggle(led_pin);
        printf("LED toggled (state: %d)\n", led_pin->read(led_pin));
        for (volatile int j = 0; j < 1000000; j++)
            ;
    }

cleanup:
    lifecycle->deinit(lifecycle);
    nx_factory_gpio_release(led_pin);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 2: GPIO Input with Pull-up                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 2: GPIO input reading
 * \details         Demonstrates configuring a GPIO pin as input with pull-up
 *                  and reading button state
 */
void example_gpio_input(void) {
    printf("=== Example 2: GPIO Input ===\n");

    /* Get GPIO pin (Port C, Pin 13 - typical button pin) */
    nx_gpio_t* button_pin = nx_factory_gpio(2, 13);
    if (!button_pin) {
        printf("Error: Failed to get GPIO pin\n");
        return;
    }

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = button_pin->get_lifecycle(button_pin);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize GPIO\n");
        nx_factory_gpio_release(button_pin);
        return;
    }

    /* Configure as input with pull-up */
    nx_gpio_config_t config = {
        .mode = NX_GPIO_MODE_INPUT,
        .pull = NX_GPIO_PULL_UP,
        .speed = NX_GPIO_SPEED_LOW,
        .af_index = 0,
    };

    if (button_pin->set_config(button_pin, &config) != NX_OK) {
        printf("Error: Failed to configure GPIO\n");
        goto cleanup;
    }

    /* Read button state multiple times */
    printf("Reading button state (press button if available)...\n");
    for (int i = 0; i < 10; i++) {
        uint8_t state = button_pin->read(button_pin);
        printf("Button state: %d (%s)\n", state,
               state ? "Released" : "Pressed");

        /* Simulate delay */
        for (volatile int j = 0; j < 500000; j++)
            ;
    }

cleanup:
    lifecycle->deinit(lifecycle);
    nx_factory_gpio_release(button_pin);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 3: Runtime Mode Switching                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 3: Runtime mode switching
 * \details         Demonstrates switching GPIO mode at runtime between
 *                  input and output
 */
void example_gpio_mode_switching(void) {
    printf("=== Example 3: Runtime Mode Switching ===\n");

    /* Get GPIO pin */
    nx_gpio_t* gpio = nx_factory_gpio(1, 0);
    if (!gpio) {
        printf("Error: Failed to get GPIO pin\n");
        return;
    }

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize GPIO\n");
        nx_factory_gpio_release(gpio);
        return;
    }

    /* Start as output */
    printf("Configuring as output...\n");
    if (gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_PP) == NX_OK) {
        gpio->write(gpio, 1);
        printf("Output mode: wrote 1, read back: %d\n", gpio->read(gpio));
    }

    /* Switch to input */
    printf("Switching to input mode...\n");
    if (gpio->set_mode(gpio, NX_GPIO_MODE_INPUT) == NX_OK) {
        printf("Input mode: current state: %d\n", gpio->read(gpio));
    }

    /* Switch back to output */
    printf("Switching back to output mode...\n");
    if (gpio->set_mode(gpio, NX_GPIO_MODE_OUTPUT_PP) == NX_OK) {
        gpio->write(gpio, 0);
        printf("Output mode: wrote 0, read back: %d\n", gpio->read(gpio));
    }

    /* Switch pull configuration */
    printf("Changing pull configuration...\n");
    gpio->set_mode(gpio, NX_GPIO_MODE_INPUT);
    gpio->set_pull(gpio, NX_GPIO_PULL_UP);
    printf("Pull-up enabled\n");

    gpio->set_pull(gpio, NX_GPIO_PULL_DOWN);
    printf("Pull-down enabled\n");

    gpio->set_pull(gpio, NX_GPIO_PULL_NONE);
    printf("Pull disabled\n");

    /* Cleanup */
    lifecycle->deinit(lifecycle);
    nx_factory_gpio_release(gpio);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 4: External Interrupt (EXTI)                                     */
/*---------------------------------------------------------------------------*/

/* Interrupt callback context */
static volatile int interrupt_count = 0;

/**
 * \brief           GPIO EXTI callback function
 * \param[in]       context: User context pointer
 */
static void gpio_exti_callback(void* context) {
    interrupt_count++;
    printf("  [IRQ] Interrupt triggered! Count: %d\n", interrupt_count);
}

/**
 * \brief           Example 4: External interrupt configuration
 * \details         Demonstrates configuring GPIO external interrupts
 */
void example_gpio_exti(void) {
    printf("=== Example 4: External Interrupt (EXTI) ===\n");

    /* Get GPIO pin */
    nx_gpio_t* gpio = nx_factory_gpio(0, 0);
    if (!gpio) {
        printf("Error: Failed to get GPIO pin\n");
        return;
    }

    /* Initialize GPIO */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize GPIO\n");
        nx_factory_gpio_release(gpio);
        return;
    }

    /* Configure as input */
    nx_gpio_config_t config = {
        .mode = NX_GPIO_MODE_INPUT,
        .pull = NX_GPIO_PULL_UP,
        .speed = NX_GPIO_SPEED_LOW,
        .af_index = 0,
    };
    gpio->set_config(gpio, &config);

    /* Configure EXTI on falling edge (button press) */
    printf("Configuring interrupt on falling edge...\n");
    interrupt_count = 0;

    if (gpio->set_exti(gpio, NX_GPIO_EXTI_FALLING, gpio_exti_callback, NULL) ==
        NX_OK) {
        printf("EXTI configured successfully\n");
        printf("Waiting for interrupts (simulate by toggling pin)...\n");

        /* Wait for interrupts (in real application, this would be event-driven)
         */
        for (volatile int i = 0; i < 10000000; i++) {
            /* In simulation, interrupts might not trigger */
        }

        printf("Total interrupts received: %d\n", interrupt_count);

        /* Clear EXTI */
        gpio->clear_exti(gpio);
        printf("EXTI cleared\n");
    }

    /* Configure EXTI on both edges */
    printf("\nConfiguring interrupt on both edges...\n");
    interrupt_count = 0;

    if (gpio->set_exti(gpio, NX_GPIO_EXTI_BOTH, gpio_exti_callback, NULL) ==
        NX_OK) {
        printf("EXTI configured for both edges\n");

        /* Wait for interrupts */
        for (volatile int i = 0; i < 10000000; i++) {
        }

        printf("Total interrupts received: %d\n", interrupt_count);

        /* Clear EXTI */
        gpio->clear_exti(gpio);
    }

    /* Cleanup */
    lifecycle->deinit(lifecycle);
    nx_factory_gpio_release(gpio);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 5: GPIO with Custom Configuration                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 5: Custom GPIO configuration
 * \details         Demonstrates using factory with custom configuration
 */
void example_gpio_custom_config(void) {
    printf("=== Example 5: Custom GPIO Configuration ===\n");

    /* Create custom configuration */
    nx_gpio_config_t custom_config = {
        .mode = NX_GPIO_MODE_OUTPUT_OD, /* Open-drain output */
        .pull = NX_GPIO_PULL_UP,
        .speed = NX_GPIO_SPEED_HIGH,
        .af_index = 0,
    };

    /* Get GPIO with custom configuration */
    nx_gpio_t* gpio = nx_factory_gpio_with_config(1, 5, &custom_config);
    if (!gpio) {
        printf("Error: Failed to get GPIO pin\n");
        return;
    }

    /* Initialize */
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize GPIO\n");
        nx_factory_gpio_release(gpio);
        return;
    }

    /* Verify configuration */
    nx_gpio_config_t current_config;
    if (gpio->get_config(gpio, &current_config) == NX_OK) {
        printf("Current configuration:\n");
        printf("  Mode: %d (OUTPUT_OD=%d)\n", current_config.mode,
               NX_GPIO_MODE_OUTPUT_OD);
        printf("  Pull: %d (PULL_UP=%d)\n", current_config.pull,
               NX_GPIO_PULL_UP);
        printf("  Speed: %d (HIGH=%d)\n", current_config.speed,
               NX_GPIO_SPEED_HIGH);
    }

    /* Use the GPIO */
    gpio->write(gpio, 1);
    printf("GPIO state: %d\n", gpio->read(gpio));

    /* Cleanup */
    lifecycle->deinit(lifecycle);
    nx_factory_gpio_release(gpio);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 6: Multiple GPIO Pins                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 6: Managing multiple GPIO pins
 * \details         Demonstrates working with multiple GPIO pins simultaneously
 */
void example_gpio_multiple_pins(void) {
    printf("=== Example 6: Multiple GPIO Pins ===\n");

    /* Get multiple GPIO pins */
    nx_gpio_t* leds[3];
    leds[0] = nx_factory_gpio(0, 5); /* LED 1 */
    leds[1] = nx_factory_gpio(0, 6); /* LED 2 */
    leds[2] = nx_factory_gpio(0, 7); /* LED 3 */

    /* Initialize all pins */
    for (int i = 0; i < 3; i++) {
        if (!leds[i]) {
            printf("Error: Failed to get LED %d\n", i);
            continue;
        }

        nx_lifecycle_t* lifecycle = leds[i]->get_lifecycle(leds[i]);
        if (lifecycle->init(lifecycle) != NX_OK) {
            printf("Error: Failed to initialize LED %d\n", i);
            continue;
        }

        /* Configure as output */
        leds[i]->set_mode(leds[i], NX_GPIO_MODE_OUTPUT_PP);
    }

    /* Create LED patterns */
    printf("Running LED patterns...\n");

    /* Pattern 1: Sequential */
    printf("Pattern 1: Sequential\n");
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < 3; i++) {
            if (leds[i]) {
                leds[i]->write(leds[i], 1);
                for (volatile int j = 0; j < 500000; j++)
                    ;
                leds[i]->write(leds[i], 0);
            }
        }
    }

    /* Pattern 2: All on/off */
    printf("Pattern 2: All on/off\n");
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < 3; i++) {
            if (leds[i])
                leds[i]->write(leds[i], 1);
        }
        for (volatile int j = 0; j < 1000000; j++)
            ;

        for (int i = 0; i < 3; i++) {
            if (leds[i])
                leds[i]->write(leds[i], 0);
        }
        for (volatile int j = 0; j < 1000000; j++)
            ;
    }

    /* Cleanup all pins */
    for (int i = 0; i < 3; i++) {
        if (leds[i]) {
            nx_lifecycle_t* lifecycle = leds[i]->get_lifecycle(leds[i]);
            lifecycle->deinit(lifecycle);
            nx_factory_gpio_release(leds[i]);
        }
    }

    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Main Function                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main function - runs all GPIO examples
 */
int main(void) {
    printf("Nexus HAL GPIO Examples\n");
    printf("=======================\n\n");

    /* Initialize HAL */
    nx_hal_init();

    /* Run examples */
    example_gpio_output_basic();
    example_gpio_input();
    example_gpio_mode_switching();
    example_gpio_exti();
    example_gpio_custom_config();
    example_gpio_multiple_pins();

    /* Cleanup HAL */
    nx_hal_deinit();

    printf("All examples completed\n");
    return 0;
}

