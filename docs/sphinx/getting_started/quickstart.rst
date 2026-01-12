Quick Start
===========

This guide will help you create your first Nexus application - a simple LED blinky.

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
    target_link_libraries(blinky PRIVATE platform_stm32f4 hal_interface)

3. Create ``main.c``:

.. code-block:: c

    #include "hal/hal.h"

    #define LED_PORT    HAL_GPIO_PORT_D
    #define LED_PIN     12

    int main(void)
    {
        hal_gpio_config_t config = {
            .direction   = HAL_GPIO_DIR_OUTPUT,
            .pull        = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed       = HAL_GPIO_SPEED_LOW,
            .init_level  = HAL_GPIO_LEVEL_LOW
        };

        hal_system_init();
        hal_gpio_init(LED_PORT, LED_PIN, &config);

        while (1) {
            hal_gpio_toggle(LED_PORT, LED_PIN);
            hal_delay_ms(500);
        }

        return 0;
    }

Build and Flash
---------------

.. code-block:: bash

    # Configure
    cmake -B build \
        -DCMAKE_TOOLCHAIN_FILE=nexus/cmake/toolchains/arm-none-eabi.cmake \
        -DNEXUS_PLATFORM=stm32f4

    # Build
    cmake --build build

    # Flash (using OpenOCD)
    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/blinky.elf verify reset exit"

Understanding the Code
----------------------

**HAL Initialization:**

.. code-block:: c

    hal_system_init();  // Initialize system (SysTick, clocks)

**GPIO Configuration:**

.. code-block:: c

    hal_gpio_config_t config = {
        .direction   = HAL_GPIO_DIR_OUTPUT,  // Output mode
        .pull        = HAL_GPIO_PULL_NONE,   // No pull-up/down
        .output_mode = HAL_GPIO_OUTPUT_PP,   // Push-pull
        .speed       = HAL_GPIO_SPEED_LOW,   // Low speed
        .init_level  = HAL_GPIO_LEVEL_LOW    // Start low
    };
    hal_gpio_init(LED_PORT, LED_PIN, &config);

**Main Loop:**

.. code-block:: c

    while (1) {
        hal_gpio_toggle(LED_PORT, LED_PIN);  // Toggle LED
        hal_delay_ms(500);                   // Wait 500ms
    }

Next Steps
----------

- Add UART for serial output
- Use OSAL for multi-tasking
- Explore middleware components
