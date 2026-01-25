Your First Nexus Application
============================

This tutorial guides you through creating your first Nexus application from scratch. You'll learn how to set up a project, configure the build system, and create a simple LED blinky application.

Learning Objectives
-------------------

By the end of this tutorial, you will:

- Understand the basic structure of a Nexus application
- Know how to configure CMake for Nexus projects
- Be able to initialize the HAL subsystem
- Create and control GPIO devices
- Build and flash your application to hardware

Prerequisites
-------------

Before starting, ensure you have:

- Completed the :doc:`../getting_started/environment_setup` guide
- A supported development board (we'll use STM32F4 Discovery)
- ARM GCC toolchain installed
- OpenOCD or ST-Link tools for flashing

.. seealso::

   * :doc:`../user_guide/build_system` - Build system overview
   * :doc:`../user_guide/hal` - HAL API documentation
   * :doc:`../platform_guides/stm32f4` - STM32F4 platform guide
   * :doc:`gpio_control` - GPIO control tutorial
   * :doc:`../user_guide/kconfig_tutorial` - Configuration tutorial

Step 1: Create Project Structure
--------------------------------

First, create a new directory for your project:

.. code-block:: bash

    mkdir my_first_nexus_app
    cd my_first_nexus_app

Create the following directory structure:

.. code-block:: text

    my_first_nexus_app/
    ├── CMakeLists.txt
    ├── main.c
    └── nexus/           (git submodule or copy of Nexus)

Step 2: Add Nexus as Dependency
-------------------------------

You can add Nexus to your project in two ways:

**Option A: Git Submodule (Recommended)**

.. code-block:: bash

    git init
    git submodule add https://github.com/X-Gen-Lab/nexus.git nexus
    git submodule update --init --recursive

**Option B: Copy Nexus**

.. code-block:: bash

    cp -r /path/to/nexus ./nexus

Step 3: Create CMakeLists.txt
-----------------------------

Create ``CMakeLists.txt`` with the following content:

.. code-block:: CMake

    cmake_minimum_required(VERSION 3.16)

    # Project name and language
    project(my_first_nexus_app C ASM)

    # Set C standard
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)

    # Add Nexus as subdirectory
    add_subdirectory(nexus)

    # Create executable
    add_executable(${PROJECT_NAME}
        main.c
    )

    # Link against Nexus libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE
        nexus::hal
        nexus::platform
    )

    # Set output name
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "app"
    )

**Understanding the CMakeLists.txt:**

- ``add_subdirectory(nexus)`` - Includes Nexus build system
- ``nexus::hal`` - Links the Hardware Abstraction Layer
- ``nexus::platform`` - Links platform-specific code

Step 4: Write Your First Application
------------------------------------

Create ``main.c`` with a simple LED blinky:

.. code-block:: c

    /**
     * \file            main.c
     * \brief           My First Nexus Application
     * \author          Your Name
     */

    #include "hal/hal.h"

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    /** LED pin definitions for STM32F4 Discovery */
    #define LED_PORT    HAL_GPIO_PORT_D
    #define LED_PIN     12  /* Green LED */

    /** Blink delay in milliseconds */
    #define BLINK_DELAY_MS  500

    /*-----------------------------------------------------------------------*/
    /* Helper Functions                                                      */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Initialize LED GPIO
     * \return          HAL_OK on success, error code otherwise
     */
    static hal_status_t led_init(void) {
        /* Configure GPIO as output */
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };

        /* Initialize GPIO */
        return hal_gpio_init(LED_PORT, LED_PIN, &config);
    }

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    /**
     * \brief           Main entry point
     * \return          Should never return
     */
    int main(void) {
        /* Initialize HAL subsystem */
        if (hal_init() != HAL_OK) {
            /* Initialization failed - stay in error loop */
            while (1) {
                /* Error state */
            }
        }

        /* Initialize LED */
        if (led_init() != HAL_OK) {
            /* LED initialization failed */
            while (1) {
                /* Error state */
            }
        }

        /* Main loop: blink LED */
        while (1) {
            /* Toggle LED state */
            hal_gpio_toggle(LED_PORT, LED_PIN);

            /* Wait for blink period */
            hal_delay_ms(BLINK_DELAY_MS);
        }

        return 0;
    }

**Understanding the Code:**

1. **HAL Initialization**: ``hal_init()`` initializes the HAL subsystem, including system clock, SysTick timer, and platform-specific hardware.

2. **GPIO Configuration**: The ``hal_gpio_config_t`` structure defines GPIO behavior:

   - ``direction``: Input or output mode
   - ``pull``: Pull-up, pull-down, or none
   - ``output_mode``: Push-pull or open-drain
   - ``speed``: GPIO speed (low, medium, high, very high)
   - ``init_level``: Initial output level (high or low)

3. **GPIO Operations**:

   - ``hal_gpio_init()``: Initialize GPIO with configuration
   - ``hal_gpio_toggle()``: Toggle GPIO output state
   - ``hal_delay_ms()``: Blocking delay in milliseconds

Step 5: Build for Native Platform (Testing)
-------------------------------------------

First, let's build for the native platform to test our code structure:

.. code-block:: bash

    # Configure for native platform
    CMake -B build-native -DNEXUS_PLATFORM=native

    # Build
    CMake --build build-native

    # Run (LED operations will be simulated)
    ./build-native/app

The native platform is useful for:

- Testing application logic without hardware
- Running unit tests
- Debugging on your development machine

Step 6: Build for STM32F4
-------------------------

Now let's build for the actual target hardware:

.. code-block:: bash

    # Configure for STM32F4 with ARM toolchain
    CMake -B build-stm32f4 \
        -DCMAKE_TOOLCHAIN_FILE=nexus/CMake/toolchains/arm-none-eabi.CMake \
        -DNEXUS_PLATFORM=stm32f4

    # Build
    CMake --build build-stm32f4

This creates ``build-stm32f4/app.elf`` and ``build-stm32f4/app.bin`` files.

Step 7: Flash to Hardware
-------------------------

**Using OpenOCD:**

.. code-block:: bash

    # Flash with OpenOCD
    openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build-stm32f4/app.elf verify reset exit"

**Using ST-Link Utility (Windows):**

1. Open ST-Link Utility
2. Connect to the board
3. Load ``build-stm32f4/app.bin`` at address ``0x08000000``
4. Click "Program & Verify"

**Using st-flash (Linux/macOS):**

.. code-block:: bash

    st-flash write build-stm32f4/app.bin 0x08000000

Step 8: Verify Operation
------------------------

After flashing:

1. The green LED (PD12) on the STM32F4 Discovery board should blink
2. The LED should toggle every 500ms
3. Press the reset button to restart the application

If the LED doesn't blink:

- Check that the board is powered
- Verify the correct LED pin is configured
- Check that the application was flashed successfully
- Try debugging with GDB (see :doc:`../development/testing`)

Understanding HAL Initialization
--------------------------------

The ``hal_init()`` function performs several important tasks:

1. **System Clock Configuration**: Sets up the main system clock (typically 168 MHz for STM32F4)
2. **SysTick Timer**: Configures the SysTick timer for ``hal_delay_ms()`` and ``hal_get_tick()``
3. **FPU Initialization**: Enables the Floating Point Unit (if available)
4. **Platform-Specific Setup**: Initializes platform-specific hardware

Always call ``hal_init()`` before using any HAL functions.

GPIO Configuration Options
--------------------------

The ``hal_gpio_config_t`` structure provides fine-grained control:

**Direction:**

- ``HAL_GPIO_DIR_INPUT``: Configure as input
- ``HAL_GPIO_DIR_OUTPUT``: Configure as output

**Pull Resistors:**

- ``HAL_GPIO_PULL_NONE``: No pull resistor
- ``HAL_GPIO_PULL_UP``: Enable pull-up resistor
- ``HAL_GPIO_PULL_DOWN``: Enable pull-down resistor

**Output Mode:**

- ``HAL_GPIO_OUTPUT_PP``: Push-pull output (can drive high and low)
- ``HAL_GPIO_OUTPUT_OD``: Open-drain output (can only pull low)

**Speed:**

- ``HAL_GPIO_SPEED_LOW``: Low speed (2 MHz)
- ``HAL_GPIO_SPEED_MEDIUM``: Medium speed (25 MHz)
- ``HAL_GPIO_SPEED_HIGH``: High speed (50 MHz)
- ``HAL_GPIO_SPEED_VERY_HIGH``: Very high speed (100 MHz)

For LED control, low speed is sufficient. Use higher speeds for high-frequency signals.

Common Issues and Solutions
---------------------------

**Issue: Build fails with "nexus not found"**

Solution: Ensure Nexus is properly added as a subdirectory and the path in ``add_subdirectory()`` is correct.

**Issue: Linker errors about undefined references**

Solution: Make sure you're linking against the correct Nexus libraries (``nexus::hal`` and ``nexus::platform``).

**Issue: Application doesn't run after flashing**

Solution:
- Verify the correct flash address (0x08000000 for STM32F4)
- Check that the toolchain file is specified correctly
- Ensure the platform is set to ``stm32f4``

**Issue: LED doesn't blink**

Solution:
- Verify the LED pin number matches your board
- Check that ``hal_init()`` succeeded
- Try a longer delay to make blinking more visible

Next Steps
----------

Congratulations! You've created your first Nexus application. Now you can:

1. :doc:`gpio_control` - Learn advanced GPIO techniques
2. :doc:`uart_communication` - Add serial communication
3. :doc:`task_creation` - Create multi-tasking applications
4. Explore the :doc:`../user_guide/hal` for more HAL features

Additional Exercises
--------------------

Try these exercises to reinforce your learning:

1. **Multiple LEDs**: Modify the code to blink all four LEDs on the STM32F4 Discovery board in sequence

2. **Variable Speed**: Add a button to change the blink speed when pressed

3. **Pattern**: Create a custom blink pattern (e.g., two quick blinks, pause, repeat)

4. **Error Handling**: Add more robust error handling with LED error indicators

Example: Blinking Multiple LEDs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    /* LED definitions */
    #define LED_GREEN_PORT   HAL_GPIO_PORT_D
    #define LED_GREEN_PIN    12
    #define LED_ORANGE_PORT  HAL_GPIO_PORT_D
    #define LED_ORANGE_PIN   13
    #define LED_RED_PORT     HAL_GPIO_PORT_D
    #define LED_RED_PIN      14
    #define LED_BLUE_PORT    HAL_GPIO_PORT_D
    #define LED_BLUE_PIN     15

    /* Initialize all LEDs */
    static hal_status_t leds_init(void) {
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };

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

    /* Main loop with sequence */
    while (1) {
        hal_gpio_toggle(LED_GREEN_PORT, LED_GREEN_PIN);
        hal_delay_ms(250);
        hal_gpio_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
        hal_delay_ms(250);
        hal_gpio_toggle(LED_RED_PORT, LED_RED_PIN);
        hal_delay_ms(250);
        hal_gpio_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
        hal_delay_ms(250);
    }

Best Practices
--------------

1. **Always Initialize HAL First**: Call ``hal_init()`` before using any HAL functions

2. **Check Return Values**: Always check return values from HAL functions for errors

3. **Use Appropriate GPIO Speed**: Don't use high-speed GPIO for slow signals (wastes power)

4. **Document Pin Assignments**: Clearly document which pins are used for what purpose

5. **Error Handling**: Implement proper error handling with visual indicators (LEDs)

6. **Code Organization**: Keep initialization code separate from main loop logic

7. **Use Meaningful Names**: Use descriptive names for constants and functions

Resources
---------

- :doc:`../user_guide/hal` - Complete HAL API documentation
- :doc:`../platform_guides/stm32f4` - STM32F4-specific information
- :doc:`../user_guide/build_system` - Advanced build configuration
- `STM32F4 Discovery User Manual <https://www.st.com/resource/en/user_manual/dm00039084.pdf>`_

