Porting Guide
=============

This guide explains how to port Nexus to a new MCU platform.

Overview
--------

Porting Nexus to a new platform involves implementing the Hardware Abstraction
Layer (HAL) for your target MCU. The OSAL and middleware layers are
platform-independent and require no modification.

Prerequisites
-------------

Before starting, ensure you have:

- MCU datasheet and reference manual
- Vendor SDK or register definitions
- ARM GCC toolchain (for ARM Cortex-M targets)
- Basic understanding of the target MCU architecture

Porting Steps
-------------

Step 1: Create Platform Directory
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a new directory under ``platforms/``::

    platforms/
    └── my_platform/
        ├── CMakeLists.txt
        ├── include/
        │   └── platform_config.h
        ├── src/
        │   ├── startup.c
        │   ├── system_init.c
        │   └── hal/
        │       ├── hal_gpio.c
        │       ├── hal_uart.c
        │       └── ...
        └── linker/
            └── my_platform.ld

Step 2: Implement HAL Drivers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Implement each HAL module for your platform. Start with GPIO as it's the
simplest:

**hal_gpio.c:**

.. code-block:: c

    /**
     * \file            hal_gpio.c
     * \brief           GPIO HAL implementation for my_platform
     */

    #include "hal/hal_gpio.h"
    #include "my_platform_registers.h"

    hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                               const hal_gpio_config_t* config)
    {
        if (config == NULL) {
            return HAL_ERROR_NULL_POINTER;
        }

        if (port >= HAL_GPIO_PORT_MAX || pin > 15) {
            return HAL_ERROR_INVALID_PARAM;
        }

        /* Platform-specific initialization */
        GPIO_TypeDef* gpio = get_gpio_base(port);

        /* Configure direction */
        if (config->direction == HAL_GPIO_DIR_OUTPUT) {
            gpio->MODER |= (1 << (pin * 2));
        } else {
            gpio->MODER &= ~(3 << (pin * 2));
        }

        /* Configure pull-up/pull-down */
        /* ... */

        return HAL_OK;
    }

    hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                                hal_gpio_level_t level)
    {
        GPIO_TypeDef* gpio = get_gpio_base(port);

        if (level == HAL_GPIO_LEVEL_HIGH) {
            gpio->BSRR = (1 << pin);
        } else {
            gpio->BSRR = (1 << (pin + 16));
        }

        return HAL_OK;
    }

    /* Implement remaining GPIO functions... */

Step 3: Create Startup Code
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Implement startup code for your MCU:

**startup.c:**

.. code-block:: c

    /**
     * \file            startup.c
     * \brief           Startup code for my_platform
     */

    #include <stdint.h>

    /* Stack pointer (defined in linker script) */
    extern uint32_t _estack;

    /* Entry point */
    extern int main(void);

    /* Weak interrupt handlers */
    void Default_Handler(void) { while(1); }
    void Reset_Handler(void);
    void NMI_Handler(void)      __attribute__((weak, alias("Default_Handler")));
    void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
    /* ... more handlers ... */

    /* Vector table */
    __attribute__((section(".isr_vector")))
    const void* vector_table[] = {
        &_estack,
        Reset_Handler,
        NMI_Handler,
        HardFault_Handler,
        /* ... more vectors ... */
    };

    void Reset_Handler(void)
    {
        /* Copy .data section */
        /* Zero .bss section */
        /* Call system init */
        /* Call main */
        main();
        while(1);
    }

Step 4: Create Linker Script
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a linker script for your MCU:

**my_platform.ld:**

.. code-block:: text

    /* Memory layout */
    MEMORY
    {
        FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
        RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K
    }

    /* Entry point */
    ENTRY(Reset_Handler)

    /* Sections */
    SECTIONS
    {
        .isr_vector :
        {
            . = ALIGN(4);
            KEEP(*(.isr_vector))
            . = ALIGN(4);
        } > FLASH

        .text :
        {
            . = ALIGN(4);
            *(.text)
            *(.text*)
            . = ALIGN(4);
        } > FLASH

        /* ... more sections ... */
    }

Step 5: Add CMake Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create CMakeLists.txt for your platform:

**CMakeLists.txt:**

.. code-block:: CMake

    # Platform: my_platform

    set(PLATFORM_SOURCES
        src/startup.c
        src/system_init.c
        src/hal/hal_gpio.c
        src/hal/hal_uart.c
        src/hal/hal_spi.c
        src/hal/hal_i2c.c
        src/hal/hal_timer.c
    )

    add_library(platform_my_platform STATIC ${PLATFORM_SOURCES})

    target_include_directories(platform_my_platform PUBLIC
        include
        ${CMAKE_SOURCE_DIR}/hal/include
    )

    target_link_libraries(platform_my_platform PUBLIC
        hal_interface
    )

    # Linker script
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/my_platform.ld)
    target_link_options(platform_my_platform PUBLIC
        -T${LINKER_SCRIPT}
    )

Step 6: Add Toolchain File
~~~~~~~~~~~~~~~~~~~~~~~~~~

If needed, create a toolchain file:

**CMake/toolchains/my_platform.CMake:**

.. code-block:: CMake

    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_SYSTEM_PROCESSOR arm)

    set(CMAKE_C_COMPILER arm-none-eabi-gcc)
    set(CMAKE_CXX_COMPILER arm-none-eabi-g++)

    set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard")
    set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard")

HAL Implementation Checklist
----------------------------

Implement these HAL modules (in recommended order):

+----------+----------+------------------------------------------+
| Module   | Priority | Notes                                    |
+==========+==========+==========================================+
| GPIO     | Required | Start here, simplest module              |
+----------+----------+------------------------------------------+
| System   | Required | Clock init, delay functions              |
+----------+----------+------------------------------------------+
| UART     | Required | Essential for debugging                  |
+----------+----------+------------------------------------------+
| Timer    | Required | Needed for OSAL timing                   |
+----------+----------+------------------------------------------+
| SPI      | Optional | If SPI peripherals are used              |
+----------+----------+------------------------------------------+
| I2C      | Optional | If I2C peripherals are used              |
+----------+----------+------------------------------------------+
| ADC      | Optional | If analog inputs are used                |
+----------+----------+------------------------------------------+

Testing Your Port
-----------------

1. **Build the blinky example:**

   .. code-block:: bash

       CMake -B build \
           -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/my_platform.CMake \
           -DNEXUS_PLATFORM=my_platform
       CMake --build build

2. **Flash and verify LED blinks**

3. **Run unit tests on native platform:**

   .. code-block:: bash

       CMake -B build-test -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
       CMake --build build-test
       ctest --test-dir build-test

4. **Test each HAL module individually**

Reference Implementation
------------------------

See ``platforms/stm32f4/`` for a complete reference implementation.

Key files to study:

- ``platforms/stm32f4/src/hal/hal_gpio.c`` - GPIO implementation
- ``platforms/stm32f4/src/startup.c`` - Startup code
- ``platforms/stm32f4/linker/stm32f407.ld`` - Linker script
- ``platforms/stm32f4/CMakeLists.txt`` - CMake configuration

Common Issues
-------------

**Linker errors about undefined symbols:**
    Ensure all HAL functions are implemented, even if as stubs.

**Hard fault on startup:**
    Check vector table alignment and stack pointer initialization.

**Interrupts not working:**
    Verify NVIC configuration and interrupt handler names.

**Timing issues:**
    Ensure system clock is configured correctly.

Getting Help
------------

If you encounter issues:

1. Check the reference implementation
2. Review MCU documentation
3. Open an issue on GitHub with details about your platform
