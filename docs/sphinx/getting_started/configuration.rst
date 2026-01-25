Configuration
=============

This guide explains how to use Kconfig to configure your Nexus build.

.. contents:: Table of Contents
   :local:
   :depth: 2

What is Kconfig?
----------------

Kconfig is a configuration system originally developed for the Linux kernel. Nexus uses Kconfig for compile-time configuration.

**Benefits**:

* **Centralized Configuration**: All options in one place
* **Dependency Management**: Automatic handling of dependencies
* **Validation**: Type checking and range validation
* **Documentation**: Built-in help text
* **Flexibility**: Easy to customize builds

Configuration Files
-------------------

Key Files
~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - File
     - Description
   * - ``Kconfig``
     - Root configuration file
   * - ``.config``
     - User configuration (generated or edited)
   * - ``nexus_config.h``
     - Generated C header file
   * - ``defconfig``
     - Default configuration for platform

File Locations
~~~~~~~~~~~~~~

.. code-block:: text

   nexus/
   ├── Kconfig                          # Root Kconfig
   ├── .config                          # User configuration
   ├── nexus_config.h                   # Generated header
   ├── hal/Kconfig                      # HAL configuration
   ├── osal/Kconfig                     # OSAL configuration
   ├── framework/*/Kconfig              # Framework configs
   └── platforms/*/defconfig            # Platform defaults

Configuration Workflow
----------------------

Basic Workflow
~~~~~~~~~~~~~~

1. **Select Platform**: Choose target platform
2. **Load Defconfig**: Start with platform defaults
3. **Customize**: Modify configuration as needed
4. **Generate Header**: Create ``nexus_config.h``
5. **Build**: Compile with configuration

Step-by-Step
~~~~~~~~~~~~

**1. Copy Platform Defconfig**

.. code-block:: bash

   # For native platform
   cp platforms/native/defconfig .config

   # For STM32F4
   cp platforms/stm32/defconfig_stm32f4 .config

   # For STM32H7
   cp platforms/stm32/defconfig_stm32h7 .config

**2. Edit Configuration (Optional)**

Edit ``.config`` file directly or use menuconfig (Linux/macOS):

.. code-block:: bash

   # Install kconfiglib
   pip install kconfiglib

   # Run menuconfig
   python scripts/kconfig/generate_config.py --menuconfig

**3. Generate Header**

.. code-block:: bash

   # Generate nexus_config.h
   python scripts/kconfig/generate_config.py \
       --config .config \
       --output nexus_config.h

**4. Build**

.. code-block:: bash

   # CMake automatically generates header during configuration
   cmake -B build
   cmake --build build

Configuration Options
---------------------

Platform Selection
~~~~~~~~~~~~~~~~~~

Select target platform:

.. code-block:: kconfig

   choice PLATFORM
       prompt "Target Platform"
       default PLATFORM_NATIVE

   config PLATFORM_NATIVE
       bool "Native Platform (PC Simulation)"

   config PLATFORM_STM32F4
       bool "STM32F4 Series"

   config PLATFORM_STM32H7
       bool "STM32H7 Series"

   config PLATFORM_GD32
       bool "GD32 Series"

   endchoice

In ``.config``:

.. code-block:: kconfig

   CONFIG_PLATFORM_STM32F4=y

HAL Configuration
~~~~~~~~~~~~~~~~~

Enable HAL peripherals:

.. code-block:: kconfig

   config HAL_GPIO
       bool "Enable GPIO support"
       default y
       help
         Enable GPIO peripheral support

   config HAL_UART
       bool "Enable UART support"
       default y
       help
         Enable UART peripheral support

   config HAL_SPI
       bool "Enable SPI support"
       depends on HAL_GPIO
       help
         Enable SPI peripheral support

In ``.config``:

.. code-block:: kconfig

   CONFIG_HAL_GPIO=y
   CONFIG_HAL_UART=y
   CONFIG_HAL_SPI=y

GPIO Configuration
~~~~~~~~~~~~~~~~~~

Configure specific GPIO pins:

.. code-block:: kconfig

   config HAL_GPIO_A_5
       bool "Enable GPIO A5"
       depends on HAL_GPIO
       default n

   if HAL_GPIO_A_5

   choice HAL_GPIO_A_5_MODE
       prompt "GPIO A5 Mode"
       default HAL_GPIO_A_5_MODE_OUTPUT_PP

   config HAL_GPIO_A_5_MODE_INPUT
       bool "Input"

   config HAL_GPIO_A_5_MODE_OUTPUT_PP
       bool "Output Push-Pull"

   config HAL_GPIO_A_5_MODE_OUTPUT_OD
       bool "Output Open-Drain"

   endchoice

   endif # HAL_GPIO_A_5

In ``.config``:

.. code-block:: kconfig

   CONFIG_HAL_GPIO_A_5=y
   CONFIG_HAL_GPIO_A_5_MODE_OUTPUT_PP=y

UART Configuration
~~~~~~~~~~~~~~~~~~

Configure UART instances:

.. code-block:: kconfig

   config HAL_UART_1
       bool "Enable UART1"
       depends on HAL_UART
       default y

   if HAL_UART_1

   config HAL_UART_1_BAUDRATE
       int "UART1 Baud Rate"
       default 115200
       range 9600 921600

   config HAL_UART_1_TX_BUFFER_SIZE
       int "UART1 TX Buffer Size"
       default 256
       range 16 4096

   config HAL_UART_1_RX_BUFFER_SIZE
       int "UART1 RX Buffer Size"
       default 256
       range 16 4096

   endif # HAL_UART_1

In ``.config``:

.. code-block:: kconfig

   CONFIG_HAL_UART_1=y
   CONFIG_HAL_UART_1_BAUDRATE=115200
   CONFIG_HAL_UART_1_TX_BUFFER_SIZE=256
   CONFIG_HAL_UART_1_RX_BUFFER_SIZE=256

OSAL Configuration
~~~~~~~~~~~~~~~~~~

Select OSAL backend:

.. code-block:: kconfig

   choice OSAL_BACKEND
       prompt "OSAL Backend"
       default OSAL_BAREMETAL

   config OSAL_BAREMETAL
       bool "Bare Metal"

   config OSAL_FREERTOS
       bool "FreeRTOS"

   config OSAL_RTTHREAD
       bool "RT-Thread"

   config OSAL_ZEPHYR
       bool "Zephyr"

   endchoice

In ``.config``:

.. code-block:: kconfig

   CONFIG_OSAL_FREERTOS=y

FreeRTOS Configuration
~~~~~~~~~~~~~~~~~~~~~~

Configure FreeRTOS parameters:

.. code-block:: kconfig

   if OSAL_FREERTOS

   config FREERTOS_HEAP_SIZE
       int "FreeRTOS Heap Size (bytes)"
       default 32768
       range 4096 262144

   config FREERTOS_MAX_PRIORITIES
       int "Maximum Task Priorities"
       default 5
       range 1 32

   config FREERTOS_TICK_RATE_HZ
       int "Tick Rate (Hz)"
       default 1000
       range 100 10000

   endif # OSAL_FREERTOS

In ``.config``:

.. code-block:: kconfig

   CONFIG_FREERTOS_HEAP_SIZE=32768
   CONFIG_FREERTOS_MAX_PRIORITIES=5
   CONFIG_FREERTOS_TICK_RATE_HZ=1000

Framework Configuration
~~~~~~~~~~~~~~~~~~~~~~~

Enable framework components:

.. code-block:: kconfig

   config FRAMEWORK_LOG
       bool "Enable Logging Framework"
       default y

   config FRAMEWORK_SHELL
       bool "Enable Shell Framework"
       depends on HAL_UART
       default y

   config FRAMEWORK_CONFIG
       bool "Enable Configuration Framework"
       default y

In ``.config``:

.. code-block:: kconfig

   CONFIG_FRAMEWORK_LOG=y
   CONFIG_FRAMEWORK_SHELL=y
   CONFIG_FRAMEWORK_CONFIG=y

Using Configuration in Code
----------------------------

Include Header
~~~~~~~~~~~~~~

.. code-block:: c

   #include "nexus_config.h"

Check Configuration
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #ifdef CONFIG_HAL_GPIO
   /* GPIO code */
   hal_gpio_init(port, pin, &config);
   #endif

   #ifdef CONFIG_HAL_UART
   /* UART code */
   hal_uart_init(uart_id, &uart_config);
   #endif

Use Configuration Values
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   #ifdef CONFIG_HAL_UART_1_BAUDRATE
   uart_config.baudrate = CONFIG_HAL_UART_1_BAUDRATE;
   #else
   uart_config.baudrate = 115200;  /* Default */
   #endif

Conditional Compilation
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   void init_peripherals(void) {
   #ifdef CONFIG_HAL_GPIO
       gpio_init();
   #endif

   #ifdef CONFIG_HAL_UART
       uart_init();
   #endif

   #ifdef CONFIG_HAL_SPI
       spi_init();
   #endif

   #ifdef CONFIG_HAL_I2C
       i2c_init();
   #endif
   }

Configuration Examples
----------------------

Minimal Configuration
~~~~~~~~~~~~~~~~~~~~~

Bare minimum for LED blinky:

.. code-block:: kconfig

   # Platform
   CONFIG_PLATFORM_STM32F4=y

   # HAL
   CONFIG_HAL_GPIO=y
   CONFIG_HAL_GPIO_D_12=y
   CONFIG_HAL_GPIO_D_12_MODE_OUTPUT_PP=y

   # OSAL
   CONFIG_OSAL_BAREMETAL=y

Full-Featured Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Complete configuration with all features:

.. code-block:: kconfig

   # Platform
   CONFIG_PLATFORM_STM32F4=y

   # HAL
   CONFIG_HAL_GPIO=y
   CONFIG_HAL_UART=y
   CONFIG_HAL_SPI=y
   CONFIG_HAL_I2C=y
   CONFIG_HAL_ADC=y
   CONFIG_HAL_TIMER=y

   # UART1
   CONFIG_HAL_UART_1=y
   CONFIG_HAL_UART_1_BAUDRATE=115200

   # OSAL
   CONFIG_OSAL_FREERTOS=y
   CONFIG_FREERTOS_HEAP_SIZE=32768

   # Framework
   CONFIG_FRAMEWORK_LOG=y
   CONFIG_FRAMEWORK_SHELL=y
   CONFIG_FRAMEWORK_CONFIG=y

Low-Power Configuration
~~~~~~~~~~~~~~~~~~~~~~~

Optimized for low power consumption:

.. code-block:: kconfig

   # Platform
   CONFIG_PLATFORM_STM32F4=y

   # HAL (minimal)
   CONFIG_HAL_GPIO=y
   CONFIG_HAL_UART=y

   # OSAL
   CONFIG_OSAL_BAREMETAL=y

   # Framework (minimal)
   CONFIG_FRAMEWORK_LOG=n
   CONFIG_FRAMEWORK_SHELL=n

   # Power management
   CONFIG_POWER_MANAGEMENT=y
   CONFIG_LOW_POWER_MODE=y

Advanced Topics
---------------

Dependencies
~~~~~~~~~~~~

Express dependencies between options:

.. code-block:: kconfig

   config HAL_SPI
       bool "Enable SPI support"
       depends on HAL_GPIO
       help
         SPI requires GPIO for chip select pins

   config FRAMEWORK_SHELL
       bool "Enable Shell Framework"
       depends on HAL_UART
       select FRAMEWORK_LOG
       help
         Shell requires UART and automatically enables logging

Selections
~~~~~~~~~~

Automatically enable dependencies:

.. code-block:: kconfig

   config FRAMEWORK_SHELL
       bool "Enable Shell Framework"
       select FRAMEWORK_LOG
       help
         Shell automatically enables logging framework

Ranges
~~~~~~

Validate numeric values:

.. code-block:: kconfig

   config HAL_UART_BAUDRATE
       int "UART Baud Rate"
       default 115200
       range 9600 921600
       help
         Valid baud rates: 9600 to 921600

Choices
~~~~~~~

Mutually exclusive options:

.. code-block:: kconfig

   choice HAL_GPIO_MODE
       prompt "GPIO Mode"
       default HAL_GPIO_MODE_OUTPUT_PP

   config HAL_GPIO_MODE_INPUT
       bool "Input"

   config HAL_GPIO_MODE_OUTPUT_PP
       bool "Output Push-Pull"

   config HAL_GPIO_MODE_OUTPUT_OD
       bool "Output Open-Drain"

   endchoice

Menuconfig (Linux/macOS)
-------------------------

Installation
~~~~~~~~~~~~

.. code-block:: bash

   pip install kconfiglib

Running Menuconfig
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Run menuconfig
   python scripts/kconfig/generate_config.py --menuconfig

Navigation
~~~~~~~~~~

* **Arrow keys**: Navigate menu
* **Enter**: Select/enter submenu
* **Space**: Toggle option
* **Y**: Enable option
* **N**: Disable option
* **?**: Show help
* **/** : Search
* **Q**: Quit and save
* **Esc**: Go back

Searching
~~~~~~~~~

Press ``/`` to search for options:

.. code-block:: text

   Search: GPIO

   Results:
   1. CONFIG_HAL_GPIO - Enable GPIO support
   2. CONFIG_HAL_GPIO_A_5 - Enable GPIO A5
   3. CONFIG_HAL_GPIO_D_12 - Enable GPIO D12

Troubleshooting
---------------

Configuration Not Applied
~~~~~~~~~~~~~~~~~~~~~~~~~

**Issue**: Changes to ``.config`` not reflected in build

**Solution**: Regenerate header and rebuild:

.. code-block:: bash

   python scripts/kconfig/generate_config.py --config .config --output nexus_config.h
   cmake --build build

Invalid Configuration
~~~~~~~~~~~~~~~~~~~~~

**Issue**: Configuration validation fails

**Solution**: Check dependencies and ranges:

.. code-block:: bash

   python scripts/kconfig/validate_kconfig.py --config .config

Missing Configuration
~~~~~~~~~~~~~~~~~~~~~

**Issue**: ``nexus_config.h`` not found

**Solution**: Generate configuration header:

.. code-block:: bash

   python scripts/kconfig/generate_config.py --default --output nexus_config.h

Conflicting Options
~~~~~~~~~~~~~~~~~~~

**Issue**: Mutually exclusive options both enabled

**Solution**: Review dependencies and choices in Kconfig files

Best Practices
--------------

1. **Start with Defconfig**: Use platform defaults as starting point
2. **Document Changes**: Comment your ``.config`` file
3. **Version Control**: Commit ``.config`` for reproducible builds
4. **Validate Configuration**: Run validation before building
5. **Use Menuconfig**: For complex configurations
6. **Test Configurations**: Build and test after changes

Next Steps
----------

Now that you understand configuration:

1. :doc:`examples_tour` - Explore example configurations
2. :doc:`../user_guide/kconfig` - Detailed Kconfig documentation
3. :doc:`../user_guide/kconfig_tutorial` - Kconfig tutorial
4. :doc:`../platform_guides/index` - Platform-specific configurations

See Also
--------

* :doc:`../user_guide/kconfig` - Kconfig system overview
* :doc:`../user_guide/kconfig_tutorial` - Step-by-step tutorial
* :doc:`../user_guide/kconfig_peripherals` - Peripheral configuration
* :doc:`../user_guide/build_system` - Build system documentation
