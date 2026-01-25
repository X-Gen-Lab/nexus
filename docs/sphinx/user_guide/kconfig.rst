Kconfig Configuration System
============================

Overview
--------

The Nexus platform uses a Kconfig-based configuration system for managing
build-time configuration options. This system provides a hierarchical,
type-safe, and maintainable way to configure platforms, chips, peripherals,
and RTOS backends.

**Key Features:**

* **Hierarchical Organization**: Platform → Chip → Peripheral → Instance
* **Type Safety**: Boolean, integer, string, and hexadecimal types with validation
* **Multiple Platforms**: Native, STM32, GD32, ESP32, NRF52
* **OSAL Backends**: Bare-metal, FreeRTOS, RT-Thread, Zephyr
* **Automatic Generation**: C header files generated from configuration
* **Validation Tools**: Syntax checking, dependency validation, range constraints
* **Migration Support**: Configuration migration between versions

Architecture
------------

Configuration Hierarchy
^^^^^^^^^^^^^^^^^^^^^^^

The configuration system follows a four-level hierarchy:

.. code-block:: text

    Kconfig (Root)
    ├── Platform Selection (Native, STM32, GD32, ESP32, NRF52)
    │   ├── Chip Family (STM32F4, STM32H7, STM32L4, ...)
    │   │   ├── Chip Variant (STM32F407, STM32F429, ...)
    │   │   └── Peripheral Configuration
    │   │       ├── UART (UART1, UART2, ...)
    │   │       ├── GPIO (GPIO_A, GPIO_B, ...)
    │   │       ├── SPI (SPI1, SPI2, ...)
    │   │       └── I2C (I2C1, I2C2, ...)
    │   └── Platform-Specific Settings
    ├── OSAL Configuration
    │   ├── Backend Selection (Bare-metal, FreeRTOS, RT-Thread, ...)
    │   ├── System Parameters (Tick rate, Heap size, Stack size)
    │   └── Linker Script Configuration
    └── HAL Configuration
        ├── Debug Settings
        ├── Statistics
        └── Timeout Configuration

File Organization
^^^^^^^^^^^^^^^^^

.. code-block:: text

    nexus/
    ├── Kconfig                              # Root configuration file
    ├── .config                              # Generated configuration
    │
    ├── platforms/
    │   ├── Kconfig                          # Platform selection
    │   ├── native/
    │   │   ├── Kconfig                      # Native platform config
    │   │   ├── defconfig                    # Default configuration
    │   │   └── src/
    │   │       ├── uart/Kconfig
    │   │       ├── gpio/Kconfig
    │   │       └── ...
    │   │
    │   └── STM32/
    │       ├── Kconfig                      # STM32 platform config
    │       ├── Kconfig_chip                 # Chip selection
    │       ├── Kconfig_peripherals          # Peripheral config
    │       ├── defconfig_stm32f4            # STM32F4 defaults
    │       └── defconfig_stm32h7            # STM32H7 defaults
    │
    ├── osal/
    │   └── Kconfig                          # OSAL configuration
    │
    ├── hal/
    │   ├── Kconfig                          # HAL configuration
    │   └── include/hal/
    │       └── nexus_config.h              # Generated header
    │
    └── scripts/Kconfig/
        ├── generate_config.py           # Config → C header
        ├── validate_kconfig.py              # Validation tool
        ├── kconfig_migrate.py               # Migration tool
        └── kconfig_diff.py                  # Comparison tool

Quick Start
-----------

Basic Configuration
^^^^^^^^^^^^^^^^^^^

**1. Select Platform:**

Edit ``.config`` or use menuconfig:

.. code-block:: Kconfig

    CONFIG_PLATFORM_STM32=y
    CONFIG_STM32F4=y
    CONFIG_STM32F407=y

**2. Configure Peripherals:**

.. code-block:: Kconfig

    CONFIG_STM32_UART_ENABLE=y
    CONFIG_INSTANCE_STM32_UART_1=y
    CONFIG_UART1_BAUDRATE=115200
    CONFIG_UART1_MODE_DMA=y

**3. Select OSAL Backend:**

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=32768

**4. Generate Configuration:**

.. code-block:: bash

    python scripts/Kconfig/generate_config.py --config .config

This generates ``nexus_config.h``:

.. code-block:: c

    #define NX_CONFIG_PLATFORM_STM32 1
    #define NX_CONFIG_STM32F407 1
    #define NX_CONFIG_UART1_BAUDRATE 115200
    #define NX_CONFIG_OSAL_FREERTOS 1

Using Default Configurations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Native Platform:**

.. code-block:: bash

    cp platforms/native/defconfig .config
    python scripts/Kconfig/generate_config.py

**STM32F4 Platform:**

.. code-block:: bash

    cp platforms/STM32/defconfig_stm32f4 .config
    python scripts/Kconfig/generate_config.py

**STM32H7 Platform:**

.. code-block:: bash

    cp platforms/STM32/defconfig_stm32h7 .config
    python scripts/Kconfig/generate_config.py

Configuration Tools
-------------------

Generate Configuration Header
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Convert ``.config`` to C header file:

.. code-block:: bash

    # From .config file
    python scripts/Kconfig/generate_config.py \
        --config .config \
        --output nexus_config.h

    # Generate default configuration
    python scripts/Kconfig/generate_config.py --default

Validate Configuration
^^^^^^^^^^^^^^^^^^^^^^

Check Kconfig syntax and dependencies:

.. code-block:: bash

    # Validate root Kconfig
    python scripts/Kconfig/validate_kconfig.py Kconfig

    # Validate specific file
    python scripts/Kconfig/validate_kconfig.py platforms/STM32/Kconfig

**Validation checks:**

* Block structure (if/endif, menu/endmenu, choice/endchoice)
* Source file existence
* Circular dependencies
* Undefined symbol references
* Range constraint violations
* Default value consistency

Compare Configurations
^^^^^^^^^^^^^^^^^^^^^^

Show differences between two configurations:

.. code-block:: bash

    # Text format (default)
    python scripts/Kconfig/kconfig_diff.py .config platforms/native/.config

    # JSON format
    python scripts/Kconfig/kconfig_diff.py \
        --format json \
        config1 config2 \
        --output diff.json

Migrate Configuration
^^^^^^^^^^^^^^^^^^^^^

Migrate configuration to new version:

.. code-block:: bash

    python scripts/Kconfig/kconfig_migrate.py \
        --input old.config \
        --output new.config \
        --version 2.0

Generate Documentation
^^^^^^^^^^^^^^^^^^^^^^

Generate configuration reference documentation:

.. code-block:: bash

    python scripts/Kconfig/generate_config_docs.py \
        Kconfig \
        --output docs/config_reference.md

Unified Configuration Tool
^^^^^^^^^^^^^^^^^^^^^^^^^^

Use ``nexus_config.py`` for all operations:

.. code-block:: bash

    # Generate configuration
    python scripts/nexus_config.py generate

    # Validate configuration
    python scripts/nexus_config.py validate Kconfig

    # Compare configurations
    python scripts/nexus_config.py diff old.config new.config

    # Migrate configuration
    python scripts/nexus_config.py migrate old.config --target-version 2.0

Platform Configuration
----------------------

Platform Selection
^^^^^^^^^^^^^^^^^^

Select one platform from available options:

.. code-block:: Kconfig

    choice
        prompt "Target Platform"
        default PLATFORM_NATIVE

    config PLATFORM_NATIVE
        bool "Native Platform (PC Simulation)"

    config PLATFORM_STM32
        bool "STM32 Platform"

    config PLATFORM_GD32
        bool "GD32 Platform"

    config PLATFORM_ESP32
        bool "ESP32 Platform"

    config PLATFORM_NRF52
        bool "NRF52 Platform"

    endchoice

STM32 Platform
^^^^^^^^^^^^^^

**Chip Family Selection:**

.. code-block:: Kconfig

    choice
        prompt "STM32 Chip Family"
        default STM32F4

    config STM32F4
        bool "STM32F4 Series"

    config STM32H7
        bool "STM32H7 Series"

    config STM32L4
        bool "STM32L4 Series"

    endchoice

**Chip Variant:**

.. code-block:: Kconfig

    if STM32F4

    choice
        prompt "STM32F4 Chip Variant"
        default STM32F407

    config STM32F407
        bool "STM32F407"

    config STM32F429
        bool "STM32F429"

    config STM32F446
        bool "STM32F446"

    endchoice

    endif

**Example Configuration:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_STM32=y
    CONFIG_STM32F4=y
    CONFIG_STM32F407=y
    CONFIG_STM32_CHIP_NAME="STM32F407xx"

Peripheral Configuration
------------------------

UART Configuration
^^^^^^^^^^^^^^^^^^

**Enable UART:**

.. code-block:: Kconfig

    menuconfig STM32_UART_ENABLE
        bool "Enable UART support"
        default y

**Configure UART Instance:**

.. code-block:: Kconfig

    if STM32_UART_ENABLE

    menuconfig INSTANCE_STM32_UART_1
        bool "Enable UART1"
        default y

    if INSTANCE_STM32_UART_1

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200

    config UART1_DATA_BITS
        int "UART1 data bits"
        default 8
        range 7 9

    choice
        prompt "UART1 transfer mode"
        default UART1_MODE_DMA

    config UART1_MODE_POLLING
        bool "Polling"

    config UART1_MODE_INTERRUPT
        bool "Interrupt"

    config UART1_MODE_DMA
        bool "DMA"

    endchoice

    config UART1_TX_BUFFER_SIZE
        int "UART1 TX buffer size"
        default 256
        range 16 4096

    config UART1_RX_BUFFER_SIZE
        int "UART1 RX buffer size"
        default 256
        range 16 4096

    endif # INSTANCE_STM32_UART_1

    endif # STM32_UART_ENABLE

**Example Configuration:**

.. code-block:: Kconfig

    CONFIG_STM32_UART_ENABLE=y
    CONFIG_INSTANCE_STM32_UART_1=y
    CONFIG_UART1_BAUDRATE=115200
    CONFIG_UART1_DATA_BITS=8
    CONFIG_UART1_MODE_DMA=y
    CONFIG_UART1_TX_BUFFER_SIZE=256
    CONFIG_UART1_RX_BUFFER_SIZE=256

GPIO Configuration
^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    menuconfig NATIVE_GPIO_ENABLE
        bool "Enable GPIO support"
        default y

    if NATIVE_GPIO_ENABLE

    menuconfig INSTANCE_NATIVE_GPIO_A
        bool "Enable GPIO Port A"
        default y

    if INSTANCE_NATIVE_GPIO_A

    config GPIO_A_PIN_COUNT
        int "Number of pins in Port A"
        default 16
        range 1 32

    config GPIO_A_DEFAULT_MODE
        int "Default pin mode"
        default 0
        help
          0 = Input, 1 = Output, 2 = Alternate Function

    endif # INSTANCE_NATIVE_GPIO_A

    endif # NATIVE_GPIO_ENABLE

OSAL Configuration
------------------

Backend Selection
^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    choice
        prompt "OSAL Backend"
        default OSAL_BAREMETAL

    config OSAL_BAREMETAL
        bool "Bare-metal (No OS)"

    config OSAL_FREERTOS
        bool "FreeRTOS"

    config OSAL_RTTHREAD
        bool "RT-Thread"

    config OSAL_ZEPHYR
        bool "Zephyr RTOS"

    config OSAL_LINUX
        bool "Linux"

    config OSAL_NATIVE
        bool "Native (PC Simulation)"

    endchoice

System Parameters
^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    menu "System Parameters"

    config OSAL_TICK_RATE_HZ
        int "System tick rate (Hz)"
        default 1000 if OSAL_FREERTOS
        default 1000 if OSAL_RTTHREAD
        default 100 if OSAL_ZEPHYR
        default 1000
        range 100 10000

    config OSAL_HEAP_SIZE
        int "Heap size (bytes)"
        default 32768 if OSAL_FREERTOS
        default 32768 if OSAL_RTTHREAD
        default 16384 if OSAL_BAREMETAL
        default 65536
        range 4096 1048576

    config OSAL_MAIN_STACK_SIZE
        int "Main stack size (bytes)"
        default 2048 if OSAL_FREERTOS
        default 2048 if OSAL_RTTHREAD
        default 4096 if OSAL_BAREMETAL
        default 4096
        range 512 65536

    endmenu

Linker Script Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    menu "Linker Script Configuration"
        depends on !PLATFORM_NATIVE

    config LINKER_RAM_START
        hex "RAM start address"
        default 0x20000000 if STM32F4
        default 0x24000000 if STM32H7
        default 0x20000000

    config LINKER_RAM_SIZE
        hex "RAM size"
        default 0x00020000 if STM32F407
        default 0x00080000 if STM32H743
        default 0x00020000

    config LINKER_FLASH_START
        hex "Flash start address"
        default 0x08000000 if PLATFORM_STM32
        default 0x08000000

    config LINKER_FLASH_SIZE
        hex "Flash size"
        default 0x00100000 if STM32F407
        default 0x00200000 if STM32H743
        default 0x00100000

    endmenu

HAL Configuration
-----------------

Debug Configuration
^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    menu "Debug Configuration"

    config HAL_DEBUG_ENABLE
        bool "Enable HAL debug output"
        default n

    config HAL_DEBUG_LEVEL
        int "HAL debug level"
        default 2
        range 0 4
        depends on HAL_DEBUG_ENABLE
        help
          Debug level: 0=None, 1=Error, 2=Warning, 3=Info, 4=Verbose

    config HAL_ASSERT_ENABLE
        bool "Enable HAL assertions"
        default y

    endmenu

Statistics Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    menu "Statistics Configuration"

    config HAL_STATISTICS_ENABLE
        bool "Enable HAL statistics"
        default y

    config HAL_STATISTICS_BUFFER_SIZE
        int "Statistics buffer size"
        default 256
        range 64 4096
        depends on HAL_STATISTICS_ENABLE

    endmenu

Using Configuration in Code
---------------------------

Include Generated Header
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    #include "nexus_config.h"

Conditional Compilation
^^^^^^^^^^^^^^^^^^^^^^^

**Platform-specific code:**

.. code-block:: c

    #ifdef NX_CONFIG_PLATFORM_STM32
        /* STM32 specific implementation */
    #elif defined(NX_CONFIG_PLATFORM_GD32)
        /* GD32 specific implementation */
    #endif

**Peripheral enable:**

.. code-block:: c

    #ifdef NX_CONFIG_INSTANCE_STM32_UART_1
        /* Initialize UART1 */
        uart_init(UART1, NX_CONFIG_UART1_BAUDRATE);
    #endif

Using Configuration Values
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    /* Use configuration values */
    static uint8_t tx_buffer[NX_CONFIG_UART1_TX_BUFFER_SIZE];
    static uint8_t rx_buffer[NX_CONFIG_UART1_RX_BUFFER_SIZE];

    /* OSAL backend selection */
    #if defined(NX_CONFIG_OSAL_FREERTOS)
        #include "FreeRTOS.h"
        #include "task.h"
    #elif defined(NX_CONFIG_OSAL_RTTHREAD)
        #include <rtthread.h>
    #elif defined(NX_CONFIG_OSAL_BAREMETAL)
        /* Bare-metal implementation */
    #endif

Build System Integration
------------------------

CMake Integration
^^^^^^^^^^^^^^^^^

The configuration system is integrated into CMake:

.. code-block:: CMake

    # CMakeLists.txt
    function(nexus_generate_config)
        set(GENERATOR_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/Kconfig/generate_config.py")
        set(CONFIG_FILE "${CMAKE_SOURCE_DIR}/.config")
        set(OUTPUT_FILE "${CMAKE_SOURCE_DIR}/nexus_config.h")

        # Generate configuration header
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT}
                    --config ${CONFIG_FILE}
                    --output ${OUTPUT_FILE}
            RESULT_VARIABLE RESULT
        )

        if(NOT RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to generate HAL config")
        endif()
    endfunction()

    # Call during configuration
    nexus_generate_config()

Automatic Regeneration
^^^^^^^^^^^^^^^^^^^^^^

Configuration headers are automatically regenerated when:

* ``.config`` file changes
* Any ``Kconfig`` file changes
* CMake configuration is run

Best Practices
--------------

Naming Conventions
^^^^^^^^^^^^^^^^^^

**Platform symbols:**

.. code-block:: text

    PLATFORM_<NAME>                 # Platform selection
    <PLATFORM>_<COMPONENT>_<PARAM>  # Platform-specific config

**Peripheral instances:**

.. code-block:: text

    INSTANCE_<PLATFORM>_<PERIPHERAL>_<N>  # Instance enable
    <PERIPHERAL><N>_<PARAMETER>           # Instance parameter

**Examples:**

.. code-block:: text

    PLATFORM_STM32
    STM32_UART_ENABLE
    INSTANCE_STM32_UART_1
    UART1_BAUDRATE
    UART1_MODE_DMA

Configuration Organization
^^^^^^^^^^^^^^^^^^^^^^^^^^

1. **Use choice for mutually exclusive options**
2. **Use menuconfig for grouped settings**
3. **Provide sensible defaults**
4. **Add help text for all options**
5. **Use range constraints for integers**
6. **Use depends on for conditional options**

Validation
^^^^^^^^^^

Always validate configuration before building:

.. code-block:: bash

    # Validate syntax
    python scripts/Kconfig/validate_kconfig.py Kconfig

    # Check for errors
    if [ $? -ne 0 ]; then
        echo "Configuration validation failed"
        exit 1
    fi

    # Generate header
    python scripts/Kconfig/generate_config.py

Documentation
^^^^^^^^^^^^^

Document all configuration options:

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200
        help
          Baud rate for UART1 in bits per second.
          Common values: 9600, 19200, 38400, 57600, 115200

          Note: Higher baud rates may require more accurate
          clock configuration.

Troubleshooting
---------------

Common Issues
^^^^^^^^^^^^^

**Issue: Configuration not applied**

.. code-block:: bash

    # Regenerate configuration header
    python scripts/Kconfig/generate_config.py --config .config

    # Clean and rebuild
    rm -rf build
    CMake -B build
    CMake --build build

**Issue: Validation errors**

.. code-block:: bash

    # Check validation output
    python scripts/Kconfig/validate_kconfig.py Kconfig

    # Fix reported errors in Kconfig files

**Issue: Missing symbols**

.. code-block:: bash

    # Check if symbol is defined
    grep -r "config SYMBOL_NAME" .

    # Verify .config has the symbol
    grep "SYMBOL_NAME" .config

**Issue: Default values not working**

.. code-block:: bash

    # Use platform-specific defconfig
    cp platforms/STM32/defconfig_stm32f4 .config

    # Or generate default configuration
    python scripts/Kconfig/generate_config.py --default

See Also
--------

* :doc:`../development/contributing` - Contributing guidelines
* :doc:`porting` - Platform porting guide
* Configuration Guide: ``docs/configuration_guide.md``
* Kconfig Writing Guide: ``docs/kconfig_writing_guide.md``
* Configuration Reference: ``docs/config_reference.md``

