GPIO Control Tutorial
=====================

This tutorial teaches you advanced GPIO control techniques using the Nexus HAL. You'll learn how to read inputs, control outputs, handle interrupts, and implement debouncing.

Learning Objectives
-------------------

By the end of this tutorial, you will:

- Configure GPIO pins as inputs and outputs
- Read button states with proper debouncing
- Use GPIO interrupts for event-driven programming
- Control multiple LEDs with patterns
- Understand GPIO electrical characteristics

Prerequisites
-------------

- Completed :doc:`first_application` tutorial
- STM32F4 Discovery board or compatible hardware
- Basic understanding of digital I/O concepts

Hardware Setup
--------------

For this tutorial, you'll use:

**Outputs (LEDs on STM32F4 Discovery):**

- PD12: Green LED
- PD13: Orange LED
- PD14: Red LED
- PD15: Blue LED

**Input (User Button):**

- PA0: User button (active high)

No additional wiring is required for the STM32F4 Discovery board.

Part 1: Reading GPIO Inputs
---------------------------

Let's start by reading the user button state.

GPIO Input Workflow
~~~~~~~~~~~~~~~~~~~

The following diagram shows the workflow for reading GPIO inputs and controlling outputs:

.. mermaid::
   :alt: GPIO input reading workflow showing initialization, polling, and output control

   flowchart TD
       START([Start]) --> INIT_HAL[Initialize HAL]
       INIT_HAL --> INIT_BTN[Configure Button GPIO as Input]
       INIT_BTN --> INIT_LED[Configure LED GPIO as Output]
       INIT_LED --> LOOP{Main Loop}

       LOOP --> READ[Read Button State]
       READ --> CHECK{Button Pressed?}

       CHECK -->|Yes| LED_ON[Turn LED On]
       CHECK -->|No| LED_OFF[Turn LED Off]

       LED_ON --> DELAY[Delay/Debounce]
       LED_OFF --> DELAY
       DELAY --> LOOP

       style START fill:#e1f5ff
       style INIT_HAL fill:#fff4e1
       style INIT_BTN fill:#ffe1f5
       style INIT_LED fill:#ffe1f5
       style READ fill:#e1ffe1
       style LED_ON fill:#ffe1e1
       style LED_OFF fill:#ffe1e1

Basic Button Reading
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "hal/hal.h"

    /* Button pin */
    #define BTN_PORT    HAL_GPIO_PORT_A
    #define BTN_PIN     0

    /* LED pin */
    #define LED_PORT    HAL_GPIO_PORT_D
    #define LED_PIN     12

    /**
     * \brief           Initialize button GPIO
     */
    static hal_status_t button_init(void) {
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_INPUT,
            .pull = HAL_GPIO_PULL_DOWN,  /* Pull-down for active-high button */
            .output_mode = HAL_GPIO_OUTPUT_PP,  /* Not used for input */
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };

        return hal_gpio_init(BTN_PORT, BTN_PIN, &config);
    }

    /**
     * \brief           Initialize LED GPIO
     */
    static hal_status_t led_init(void) {
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };

        return hal_gpio_init(LED_PORT, LED_PIN, &config);
    }

    int main(void) {
        hal_init();
        button_init();
        led_init();

        while (1) {
            /* Read button state */
            hal_gpio_level_t button_state = hal_gpio_read(BTN_PORT, BTN_PIN);

            /* Control LED based on button */
            if (button_state == HAL_GPIO_LEVEL_HIGH) {
                hal_gpio_write(LED_PORT, LED_PIN, HAL_GPIO_LEVEL_HIGH);
            } else {
                hal_gpio_write(LED_PORT, LED_PIN, HAL_GPIO_LEVEL_LOW);
            }
        }

        return 0;
    }

**Key Points:**

- Use ``HAL_GPIO_DIR_INPUT`` for input pins
- Configure pull resistors based on button type (pull-down for active-high, pull-up for active-low)
- ``hal_gpio_read()`` returns ``HAL_GPIO_LEVEL_HIGH`` or ``HAL_GPIO_LEVEL_LOW``

Button Debouncing
~~~~~~~~~~~~~~~~~

Mechanical buttons can "bounce" when pressed, causing multiple transitions. Here's how to debounce:

.. code-block:: c

    #define DEBOUNCE_DELAY_MS  50

    /**
     * \brief           Read button with debouncing
     * \return          true if button is pressed (debounced)
     */
    static bool button_read_debounced(void) {
        static uint32_t last_change_time = 0;
        static bool last_stable_state = false;

        /* Read current state */
        bool current_state = (hal_gpio_read(BTN_PORT, BTN_PIN) == HAL_GPIO_LEVEL_HIGH);

        /* Check if state changed */
        if (current_state != last_stable_state) {
            uint32_t now = hal_get_tick();

            /* Check if enough time has passed since last change */
            if ((now - last_change_time) >= DEBOUNCE_DELAY_MS) {
                last_stable_state = current_state;
                last_change_time = now;
            }
        }

        return last_stable_state;
    }

    /* Usage in main loop */
    while (1) {
        if (button_read_debounced()) {
            hal_gpio_write(LED_PORT, LED_PIN, HAL_GPIO_LEVEL_HIGH);
        } else {
            hal_gpio_write(LED_PORT, LED_PIN, HAL_GPIO_LEVEL_LOW);
        }

        hal_delay_ms(10);  /* Small delay to reduce CPU usage */
    }

Edge Detection
~~~~~~~~~~~~~~

Detect button press and release events:

.. code-block:: c

    /**
     * \brief           Detect button edges
     * \param[out]      pressed: Set to true on press event
     * \param[out]      released: Set to true on release event
     */
    static void button_detect_edges(bool* pressed, bool* released) {
        static bool last_state = false;
        bool current_state = button_read_debounced();

        *pressed = false;
        *released = false;

        if (current_state && !last_state) {
            *pressed = true;  /* Rising edge */
        } else if (!current_state && last_state) {
            *released = true;  /* Falling edge */
        }

        last_state = current_state;
    }

    /* Usage */
    while (1) {
        bool pressed, released;
        button_detect_edges(&pressed, &released);

        if (pressed) {
            /* Button was just pressed */
            hal_gpio_toggle(LED_PORT, LED_PIN);
        }

        hal_delay_ms(10);
    }

Part 2: Advanced Output Control
-------------------------------

LED Patterns
~~~~~~~~~~~~

Create complex LED patterns:

.. code-block:: c

    /* LED array for easy access */
    typedef struct {
        hal_gpio_port_t port;
        hal_gpio_pin_t pin;
    } led_t;

    static const led_t leds[] = {
        {HAL_GPIO_PORT_D, 12},  /* Green */
        {HAL_GPIO_PORT_D, 13},  /* Orange */
        {HAL_GPIO_PORT_D, 14},  /* Red */
        {HAL_GPIO_PORT_D, 15}   /* Blue */
    };

    #define NUM_LEDS  (sizeof(leds) / sizeof(leds[0]))

    /**
     * \brief           Initialize all LEDs
     */
    static hal_status_t leds_init(void) {
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };

        for (size_t i = 0; i < NUM_LEDS; i++) {
            if (hal_gpio_init(leds[i].port, leds[i].pin, &config) != HAL_OK) {
                return HAL_ERR_FAIL;
            }
        }

        return HAL_OK;
    }

    /**
     * \brief           Set LED state
     */
    static void led_set(size_t index, bool on) {
        if (index < NUM_LEDS) {
            hal_gpio_write(leds[index].port, leds[index].pin,
                          on ? HAL_GPIO_LEVEL_HIGH : HAL_GPIO_LEVEL_LOW);
        }
    }

    /**
     * \brief           Turn off all LEDs
     */
    static void leds_all_off(void) {
        for (size_t i = 0; i < NUM_LEDS; i++) {
            led_set(i, false);
        }
    }

    /**
     * \brief           Knight Rider pattern
     */
    static void pattern_knight_rider(void) {
        /* Forward */
        for (size_t i = 0; i < NUM_LEDS; i++) {
            led_set(i, true);
            hal_delay_ms(100);
            led_set(i, false);
        }

        /* Backward */
        for (int i = NUM_LEDS - 1; i >= 0; i--) {
            led_set(i, true);
            hal_delay_ms(100);
            led_set(i, false);
        }
    }

    /**
     * \brief           Binary counter pattern
     */
    static void pattern_binary_counter(void) {
        for (uint8_t count = 0; count < 16; count++) {
            for (size_t i = 0; i < NUM_LEDS; i++) {
                led_set(i, (count & (1 << i)) != 0);
            }
            hal_delay_ms(500);
        }
    }

PWM-like LED Dimming
~~~~~~~~~~~~~~~~~~~~

Create software PWM for LED dimming:

.. code-block:: c

    /**
     * \brief           Set LED brightness (0-100%)
     * \param[in]       index: LED index
     * \param[in]       brightness: Brightness percentage (0-100)
     */
    static void led_set_brightness(size_t index, uint8_t brightness) {
        if (index >= NUM_LEDS || brightness > 100) {
            return;
        }

        /* Software PWM: on_time / period = brightness / 100 */
        uint32_t period_us = 1000;  /* 1ms period = 1kHz */
        uint32_t on_time_us = (period_us * brightness) / 100;
        uint32_t off_time_us = period_us - on_time_us;

        if (on_time_us > 0) {
            hal_gpio_write(leds[index].port, leds[index].pin, HAL_GPIO_LEVEL_HIGH);
            /* Note: hal_delay_us() would be needed for microsecond delays */
            /* For now, use millisecond approximation */
            if (on_time_us >= 1000) {
                hal_delay_ms(on_time_us / 1000);
            }
        }

        if (off_time_us > 0) {
            hal_gpio_write(leds[index].port, leds[index].pin, HAL_GPIO_LEVEL_LOW);
            if (off_time_us >= 1000) {
                hal_delay_ms(off_time_us / 1000);
            }
        }
    }

    /**
     * \brief           Fade LED in and out
     */
    static void pattern_fade(size_t led_index) {
        /* Fade in */
        for (uint8_t brightness = 0; brightness <= 100; brightness += 5) {
            for (int i = 0; i < 10; i++) {  /* Repeat for smooth appearance */
                led_set_brightness(led_index, brightness);
            }
        }

        /* Fade out */
        for (uint8_t brightness = 100; brightness > 0; brightness -= 5) {
            for (int i = 0; i < 10; i++) {
                led_set_brightness(led_index, brightness);
            }
        }
    }

Part 3: GPIO Interrupts
-----------------------

GPIO interrupts allow event-driven programming without polling.

.. note::
   GPIO interrupt support depends on the platform. Check your platform guide for availability.

Configuring GPIO Interrupts
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "hal/hal.h"

    static volatile bool button_pressed = false;

    /**
     * \brief           GPIO interrupt callback
     * \param[in]       port: GPIO port
     * \param[in]       pin: GPIO pin
     */
    static void gpio_interrupt_callback(hal_gpio_port_t port, hal_gpio_pin_t pin) {
        if (port == BTN_PORT && pin == BTN_PIN) {
            button_pressed = true;
        }
    }

    /**
     * \brief           Initialize button with interrupt
     */
    static hal_status_t button_init_interrupt(void) {
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_INPUT,
            .pull = HAL_GPIO_PULL_DOWN,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };

        /* Initialize GPIO */
        if (hal_gpio_init(BTN_PORT, BTN_PIN, &config) != HAL_OK) {
            return HAL_ERR_FAIL;
        }

        /* Configure interrupt on rising edge */
        hal_gpio_irq_config_t irq_config = {
            .trigger = HAL_GPIO_IRQ_TRIGGER_RISING,
            .callback = gpio_interrupt_callback
        };

        return hal_gpio_irq_init(BTN_PORT, BTN_PIN, &irq_config);
    }

    int main(void) {
        hal_init();
        button_init_interrupt();
        leds_init();

        /* Enable GPIO interrupts */
        hal_gpio_irq_enable(BTN_PORT, BTN_PIN);

        while (1) {
            /* Check flag set by interrupt */
            if (button_pressed) {
                button_pressed = false;  /* Clear flag */

                /* Toggle LED */
                hal_gpio_toggle(LED_PORT, LED_PIN);
            }

            /* CPU can sleep here to save power */
            hal_delay_ms(10);
        }

        return 0;
    }

**Interrupt Triggers:**

- ``HAL_GPIO_IRQ_TRIGGER_RISING``: Trigger on low-to-high transition
- ``HAL_GPIO_IRQ_TRIGGER_FALLING``: Trigger on high-to-low transition
- ``HAL_GPIO_IRQ_TRIGGER_BOTH``: Trigger on any edge

**Important Notes:**

- Keep interrupt handlers short and fast
- Use volatile for variables shared between interrupt and main code
- Avoid calling blocking functions in interrupt handlers
- Consider using a flag to signal the main loop

Part 4: Complete Example
------------------------

Here's a complete example combining all concepts:

.. code-block:: c

    /**
     * \file            gpio_demo.c
     * \brief           GPIO Control Demo
     */

    #include "hal/hal.h"
    #include <stdbool.h>

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define BTN_PORT    HAL_GPIO_PORT_A
    #define BTN_PIN     0

    #define DEBOUNCE_DELAY_MS  50

    typedef struct {
        hal_gpio_port_t port;
        hal_gpio_pin_t pin;
    } led_t;

    static const led_t leds[] = {
        {HAL_GPIO_PORT_D, 12},  /* Green */
        {HAL_GPIO_PORT_D, 13},  /* Orange */
        {HAL_GPIO_PORT_D, 14},  /* Red */
        {HAL_GPIO_PORT_D, 15}   /* Blue */
    };

    #define NUM_LEDS  (sizeof(leds) / sizeof(leds[0]))

    /*-----------------------------------------------------------------------*/
    /* State Machine                                                         */
    /*-----------------------------------------------------------------------*/

    typedef enum {
        MODE_OFF,
        MODE_SINGLE_BLINK,
        MODE_KNIGHT_RIDER,
        MODE_BINARY_COUNTER,
        MODE_MAX
    } led_mode_t;

    static led_mode_t current_mode = MODE_OFF;

    /*-----------------------------------------------------------------------*/
    /* Helper Functions                                                      */
    /*-----------------------------------------------------------------------*/

    static hal_status_t leds_init(void) {
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };

        for (size_t i = 0; i < NUM_LEDS; i++) {
            if (hal_gpio_init(leds[i].port, leds[i].pin, &config) != HAL_OK) {
                return HAL_ERR_FAIL;
            }
        }
        return HAL_OK;
    }

    static hal_status_t button_init(void) {
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_INPUT,
            .pull = HAL_GPIO_PULL_DOWN,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };
        return hal_gpio_init(BTN_PORT, BTN_PIN, &config);
    }

    static void led_set(size_t index, bool on) {
        if (index < NUM_LEDS) {
            hal_gpio_write(leds[index].port, leds[index].pin,
                          on ? HAL_GPIO_LEVEL_HIGH : HAL_GPIO_LEVEL_LOW);
        }
    }

    static void leds_all_off(void) {
        for (size_t i = 0; i < NUM_LEDS; i++) {
            led_set(i, false);
        }
    }

    static bool button_read_debounced(void) {
        static uint32_t last_change_time = 0;
        static bool last_stable_state = false;

        bool current_state = (hal_gpio_read(BTN_PORT, BTN_PIN) == HAL_GPIO_LEVEL_HIGH);

        if (current_state != last_stable_state) {
            uint32_t now = hal_get_tick();
            if ((now - last_change_time) >= DEBOUNCE_DELAY_MS) {
                last_stable_state = current_state;
                last_change_time = now;
            }
        }

        return last_stable_state;
    }

    static bool button_pressed_event(void) {
        static bool last_state = false;
        bool current_state = button_read_debounced();
        bool pressed = current_state && !last_state;
        last_state = current_state;
        return pressed;
    }

    /*-----------------------------------------------------------------------*/
    /* LED Patterns                                                          */
    /*-----------------------------------------------------------------------*/

    static void mode_single_blink(void) {
        led_set(0, true);
        hal_delay_ms(500);
        led_set(0, false);
        hal_delay_ms(500);
    }

    static void mode_knight_rider(void) {
        for (size_t i = 0; i < NUM_LEDS; i++) {
            led_set(i, true);
            hal_delay_ms(100);
            led_set(i, false);
        }
        for (int i = NUM_LEDS - 1; i >= 0; i--) {
            led_set(i, true);
            hal_delay_ms(100);
            led_set(i, false);
        }
    }

    static void mode_binary_counter(void) {
        static uint8_t count = 0;
        for (size_t i = 0; i < NUM_LEDS; i++) {
            led_set(i, (count & (1 << i)) != 0);
        }
        count++;
        hal_delay_ms(500);
    }

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        /* Initialize */
        hal_init();
        leds_init();
        button_init();

        /* Main loop */
        while (1) {
            /* Check for button press to change mode */
            if (button_pressed_event()) {
                current_mode = (current_mode + 1) % MODE_MAX;
                leds_all_off();
            }

            /* Execute current mode */
            switch (current_mode) {
                case MODE_OFF:
                    leds_all_off();
                    hal_delay_ms(100);
                    break;

                case MODE_SINGLE_BLINK:
                    mode_single_blink();
                    break;

                case MODE_KNIGHT_RIDER:
                    mode_knight_rider();
                    break;

                case MODE_BINARY_COUNTER:
                    mode_binary_counter();
                    break;

                default:
                    current_mode = MODE_OFF;
                    break;
            }
        }

        return 0;
    }

**How It Works:**

1. Press the button to cycle through modes
2. Mode 0: All LEDs off
3. Mode 1: Single LED blinks
4. Mode 2: Knight Rider pattern
5. Mode 3: Binary counter (0-15)

Best Practices
--------------

1. **Always Initialize**: Call ``hal_init()`` before using GPIO functions

2. **Check Return Values**: Always check return values from HAL functions

3. **Use Appropriate Pull Resistors**: Configure pull-up/down based on your circuit

4. **Debounce Inputs**: Always debounce mechanical switches

5. **Keep Interrupts Short**: Minimize work in interrupt handlers

6. **Use Appropriate Speed**: Don't use high-speed GPIO for slow signals (wastes power)

7. **Document Pin Assignments**: Clearly document which pins are used for what

Common Pitfalls
---------------

**Forgetting Pull Resistors:**

Floating inputs can cause erratic behavior. Always configure pull resistors for inputs.

**No Debouncing:**

Mechanical switches bounce. Always implement debouncing for reliable operation.

**Blocking in Interrupts:**

Never call ``hal_delay_ms()`` or other blocking functions in interrupt handlers.

**Wrong Trigger Type:**

Choose the correct interrupt trigger (rising, falling, or both) for your application.

Next Steps
----------

- :doc:`uart_communication` - Add serial communication
- :doc:`task_creation` - Use OSAL for multi-tasking
- :doc:`../user_guide/hal` - Explore more HAL features
- :doc:`../platform_guides/stm32f4` - Platform-specific GPIO details

