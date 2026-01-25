Configuration Options Index
===========================

This page provides a comprehensive index of all Kconfig configuration options available in the Nexus Embedded Platform.

.. contents:: Table of Contents
   :local:
   :depth: 2

Platform Configuration
----------------------

Platform Selection
^^^^^^^^^^^^^^^^^^

.. index::
   single: PLATFORM
   single: PLATFORM_NATIVE
   single: PLATFORM_STM32F4
   single: PLATFORM_STM32H7
   single: PLATFORM_GD32
   single: PLATFORM_ESP32

* ``PLATFORM`` - Target platform selection
* ``PLATFORM_NATIVE`` - Native platform (Linux/Windows/macOS)
* ``PLATFORM_STM32F4`` - STM32F4 series MCUs
* ``PLATFORM_STM32H7`` - STM32H7 series MCUs
* ``PLATFORM_GD32`` - GigaDevice GD32 series MCUs
* ``PLATFORM_ESP32`` - Espressif ESP32 series

See :doc:`../user_guide/kconfig_platforms` for detailed platform configuration.

Chip Selection
^^^^^^^^^^^^^^

.. index::
   single: CHIP_STM32F407
   single: CHIP_STM32F429
   single: CHIP_STM32H743
   single: CHIP_STM32H750

STM32F4 Series:

* ``CHIP_STM32F407`` - STM32F407 MCU
* ``CHIP_STM32F429`` - STM32F429 MCU

STM32H7 Series:

* ``CHIP_STM32H743`` - STM32H743 MCU
* ``CHIP_STM32H750`` - STM32H750 MCU

OSAL Configuration
------------------

Backend Selection
^^^^^^^^^^^^^^^^^

.. index::
   single: OSAL_BACKEND
   single: OSAL_BAREMETAL
   single: OSAL_FREERTOS
   single: OSAL_RTTHREAD
   single: OSAL_ZEPHYR

* ``OSAL_BACKEND`` - OSAL backend selection
* ``OSAL_BAREMETAL`` - Bare-metal (no RTOS)
* ``OSAL_FREERTOS`` - FreeRTOS backend
* ``OSAL_RTTHREAD`` - RT-Thread backend
* ``OSAL_ZEPHYR`` - Zephyr RTOS backend

See :doc:`../user_guide/kconfig_osal` for OSAL backend configuration.

FreeRTOS Configuration
^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: FREERTOS_HEAP_SIZE
   single: FREERTOS_TOTAL_HEAP_SIZE
   single: FREERTOS_MINIMAL_STACK_SIZE
   single: FREERTOS_MAX_PRIORITIES

* ``FREERTOS_HEAP_SIZE`` - FreeRTOS heap size in bytes
* ``FREERTOS_TOTAL_HEAP_SIZE`` - Total heap size
* ``FREERTOS_MINIMAL_STACK_SIZE`` - Minimum stack size for tasks
* ``FREERTOS_MAX_PRIORITIES`` - Maximum number of task priorities

Peripheral Configuration
------------------------

GPIO Configuration
^^^^^^^^^^^^^^^^^^

.. index::
   single: GPIO_ENABLED
   single: GPIO_PORT_A_ENABLED
   single: GPIO_PORT_B_ENABLED
   single: GPIO_PORT_C_ENABLED

* ``GPIO_ENABLED`` - Enable GPIO support
* ``GPIO_PORT_A_ENABLED`` - Enable GPIO Port A
* ``GPIO_PORT_B_ENABLED`` - Enable GPIO Port B
* ``GPIO_PORT_C_ENABLED`` - Enable GPIO Port C
* ``GPIO_PORT_D_ENABLED`` - Enable GPIO Port D
* ``GPIO_PORT_E_ENABLED`` - Enable GPIO Port E

See :doc:`../user_guide/kconfig_peripherals` for peripheral configuration examples.

UART Configuration
^^^^^^^^^^^^^^^^^^

.. index::
   single: UART_ENABLED
   single: UART_INSTANCE_COUNT
   single: UART_DEFAULT_BAUDRATE
   single: UART_BUFFER_SIZE

* ``UART_ENABLED`` - Enable UART support
* ``UART_INSTANCE_COUNT`` - Number of UART instances
* ``UART_DEFAULT_BAUDRATE`` - Default baud rate
* ``UART_BUFFER_SIZE`` - UART buffer size

SPI Configuration
^^^^^^^^^^^^^^^^^

.. index::
   single: SPI_ENABLED
   single: SPI_INSTANCE_COUNT
   single: SPI_DEFAULT_SPEED
   single: SPI_DMA_ENABLED

* ``SPI_ENABLED`` - Enable SPI support
* ``SPI_INSTANCE_COUNT`` - Number of SPI instances
* ``SPI_DEFAULT_SPEED`` - Default SPI clock speed
* ``SPI_DMA_ENABLED`` - Enable DMA for SPI

I2C Configuration
^^^^^^^^^^^^^^^^^

.. index::
   single: I2C_ENABLED
   single: I2C_INSTANCE_COUNT
   single: I2C_DEFAULT_SPEED
   single: I2C_TIMEOUT

* ``I2C_ENABLED`` - Enable I2C support
* ``I2C_INSTANCE_COUNT`` - Number of I2C instances
* ``I2C_DEFAULT_SPEED`` - Default I2C clock speed
* ``I2C_TIMEOUT`` - I2C operation timeout

Timer Configuration
^^^^^^^^^^^^^^^^^^^

.. index::
   single: TIMER_ENABLED
   single: TIMER_INSTANCE_COUNT
   single: TIMER_RESOLUTION

* ``TIMER_ENABLED`` - Enable Timer support
* ``TIMER_INSTANCE_COUNT`` - Number of timer instances
* ``TIMER_RESOLUTION`` - Timer resolution in microseconds

ADC Configuration
^^^^^^^^^^^^^^^^^

.. index::
   single: ADC_ENABLED
   single: ADC_INSTANCE_COUNT
   single: ADC_RESOLUTION
   single: ADC_SAMPLE_TIME

* ``ADC_ENABLED`` - Enable ADC support
* ``ADC_INSTANCE_COUNT`` - Number of ADC instances
* ``ADC_RESOLUTION`` - ADC resolution (bits)
* ``ADC_SAMPLE_TIME`` - ADC sample time

Framework Configuration
-----------------------

Config Manager
^^^^^^^^^^^^^^

.. index::
   single: CONFIG_ENABLED
   single: CONFIG_BACKEND_RAM
   single: CONFIG_BACKEND_FLASH
   single: CONFIG_MAX_KEYS

* ``CONFIG_ENABLED`` - Enable Config Manager
* ``CONFIG_BACKEND_RAM`` - RAM backend
* ``CONFIG_BACKEND_FLASH`` - Flash backend
* ``CONFIG_MAX_KEYS`` - Maximum number of configuration keys

See :doc:`../user_guide/config` for Config Manager documentation.

Log Framework
^^^^^^^^^^^^^

.. index::
   single: LOG_ENABLED
   single: LOG_LEVEL
   single: LOG_BACKEND_CONSOLE
   single: LOG_BACKEND_UART
   single: LOG_ASYNC_ENABLED

* ``LOG_ENABLED`` - Enable Log Framework
* ``LOG_LEVEL`` - Default log level
* ``LOG_BACKEND_CONSOLE`` - Console backend
* ``LOG_BACKEND_UART`` - UART backend
* ``LOG_ASYNC_ENABLED`` - Enable async logging

See :doc:`../user_guide/log` for Log Framework documentation.

Shell Framework
^^^^^^^^^^^^^^^

.. index::
   single: SHELL_ENABLED
   single: SHELL_MAX_COMMANDS
   single: SHELL_HISTORY_SIZE
   single: SHELL_PROMPT

* ``SHELL_ENABLED`` - Enable Shell Framework
* ``SHELL_MAX_COMMANDS`` - Maximum number of commands
* ``SHELL_HISTORY_SIZE`` - Command history size
* ``SHELL_PROMPT`` - Shell prompt string

See :doc:`../user_guide/shell` for Shell Framework documentation.

Init Framework
^^^^^^^^^^^^^^

.. index::
   single: INIT_ENABLED
   single: INIT_LEVEL_BOARD
   single: INIT_LEVEL_DRIVER
   single: INIT_LEVEL_APP

* ``INIT_ENABLED`` - Enable Init Framework
* ``INIT_LEVEL_BOARD`` - Board-level initialization
* ``INIT_LEVEL_DRIVER`` - Driver-level initialization
* ``INIT_LEVEL_APP`` - Application-level initialization

Build Configuration
-------------------

Build Options
^^^^^^^^^^^^^

.. index::
   single: BUILD_TYPE
   single: BUILD_TESTS
   single: BUILD_EXAMPLES
   single: BUILD_DOCS

* ``BUILD_TYPE`` - Build type (Debug/Release)
* ``BUILD_TESTS`` - Build unit tests
* ``BUILD_EXAMPLES`` - Build example applications
* ``BUILD_DOCS`` - Build documentation

Optimization
^^^^^^^^^^^^

.. index::
   single: OPTIMIZATION_LEVEL
   single: LTO_ENABLED
   single: SIZE_OPTIMIZATION

* ``OPTIMIZATION_LEVEL`` - Compiler optimization level
* ``LTO_ENABLED`` - Enable Link-Time Optimization
* ``SIZE_OPTIMIZATION`` - Optimize for size

Debugging
^^^^^^^^^

.. index::
   single: DEBUG_ENABLED
   single: ASSERT_ENABLED
   single: TRACE_ENABLED

* ``DEBUG_ENABLED`` - Enable debug features
* ``ASSERT_ENABLED`` - Enable assertions
* ``TRACE_ENABLED`` - Enable tracing

See Also
--------

* :doc:`../user_guide/kconfig` - Kconfig system overview
* :doc:`../user_guide/kconfig_tutorial` - Kconfig tutorial
* :doc:`../user_guide/kconfig_peripherals` - Peripheral configuration
* :doc:`../user_guide/kconfig_platforms` - Platform configuration
* :doc:`../user_guide/kconfig_osal` - OSAL configuration
* :doc:`../user_guide/kconfig_tools` - Kconfig tools
* :ref:`genindex` - General index
* :ref:`search` - Search documentation
