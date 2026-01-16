Quick Start
===========

This guide will help you create your first Nexus application - a simple LED blinky.

Prerequisites
-------------

Make sure you have completed the :doc:`installation` guide.

Create a New Project
--------------------

1. Create a project directory:

.. code-block:: bash

    mkdir my_blinky
    cd my_blinky

2. Create ``CMakeLists.txt``:

.. code-block:: cmake

    cmake_minimum_required(VERSION 3.16)
    project(my_blinky C)

    # Add Nexus as subdirectory
    add_subdirectory(nexus)

    # Create application
    add_executable(blinky main.c)
    target_link_libraries(blinky PRIVATE nexus_hal nexus_platform)

3. Create ``main.c``:

.. code-block:: c

    #include "hal/nx_hal.h"

    #define LED_PORT    0   /* Port A */
    #define LED_PIN     5   /* Pin 5 */

    int main(void)
    {
        /* Initialize HAL */
        nx_hal_init();

        /* Configure GPIO */
        nx_gpio_config_t cfg = {
            .mode  = NX_GPIO_MODE_OUTPUT_PP,
            .pull  = NX_GPIO_PULL_NONE,
            .speed = NX_GPIO_SPEED_LOW,
        };

        /* Get GPIO device */
        nx_gpio_t* led = nx_factory_gpio_with_config(LED_PORT, LED_PIN, &cfg);
        if (!led) {
            return -1;
        }

        /* Blink loop */
        while (1) {
            led->toggle(led);
            /* Platform-specific delay */
            for (volatile int i = 0; i < 1000000; i++);
        }

        /* Cleanup (never reached) */
        nx_factory_gpio_release(led);
        nx_hal_deinit();
        return 0;
    }

Build for Native (Testing)
--------------------------

.. code-block:: bash

    # Configure for native platform
    cmake -B build -DNEXUS_PLATFORM=native

    # Build
    cmake --build build

    # Run
    ./build/blinky

Build for STM32F4
-----------------

.. code-block:: bash

    # Configure for STM32F4
    cmake -B build-stm32f4 \
        -DCMAKE_TOOLCHAIN_FILE=nexus/cmake/toolchains/arm-none-eabi.cmake \
        -DNEXUS_PLATFORM=stm32f4

    # Build
    cmake --build build-stm32f4

    # Flash (using OpenOCD)
    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build-stm32f4/blinky.elf verify reset exit"

Understanding the Code
----------------------

**HAL Initialization:**

.. code-block:: c

    nx_hal_init();  /* Initialize HAL subsystem */

This initializes the HAL subsystem, including platform-specific hardware,
resource managers, and device registry.

**GPIO Configuration:**

.. code-block:: c

    nx_gpio_config_t cfg = {
        .mode  = NX_GPIO_MODE_OUTPUT_PP,  /* Push-pull output */
        .pull  = NX_GPIO_PULL_NONE,       /* No pull-up/down */
        .speed = NX_GPIO_SPEED_LOW,       /* Low speed */
    };

    nx_gpio_t* led = nx_factory_gpio_with_config(LED_PORT, LED_PIN, &cfg);

The factory function creates a GPIO device instance with the specified
configuration. It returns a pointer to the GPIO interface.

**GPIO Operations:**

.. code-block:: c

    led->toggle(led);       /* Toggle pin state */
    led->write(led, 1);     /* Set high */
    led->write(led, 0);     /* Set low */
    uint8_t state = led->read(led);  /* Read state */

The GPIO interface provides methods for basic I/O operations.

**Resource Cleanup:**

.. code-block:: c

    nx_factory_gpio_release(led);  /* Release GPIO device */
    nx_hal_deinit();               /* Deinitialize HAL */

Always release devices when done to free resources.

Adding UART Output
------------------

Extend your application with serial output:

.. code-block:: c

    #include "hal/nx_hal.h"
    #include <string.h>

    int main(void)
    {
        nx_hal_init();

        /* Configure UART */
        nx_uart_config_t uart_cfg = {
            .baudrate    = 115200,
            .word_length = 8,
            .stop_bits   = 1,
            .parity      = 0,
        };

        nx_uart_t* uart = nx_factory_uart_with_config(0, &uart_cfg);
        if (!uart) {
            return -1;
        }

        /* Get synchronous TX interface */
        nx_tx_sync_t* tx = uart->get_tx_sync(uart);

        /* Send message */
        const char* msg = "Hello, Nexus!\r\n";
        tx->send(tx, (uint8_t*)msg, strlen(msg), 1000);

        /* Cleanup */
        nx_factory_uart_release(uart);
        nx_hal_deinit();
        return 0;
    }

Next Steps
----------

- :doc:`../user_guide/hal` - Learn more about HAL interfaces
- :doc:`../user_guide/osal` - Add multi-tasking with OSAL
- :doc:`../user_guide/log` - Add logging to your application
- Browse example applications in ``applications/`` directory
