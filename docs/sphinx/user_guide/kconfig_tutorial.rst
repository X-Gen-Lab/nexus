Kconfig Tutorial
================

This tutorial provides a comprehensive guide to using the Kconfig configuration system in the Nexus Embedded Platform, from basic concepts to advanced techniques.

.. contents:: Table of Contents
   :local:
   :depth: 3

Introduction
------------

What is Kconfig?
^^^^^^^^^^^^^^^^

Kconfig is a configuration language originally developed for the Linux kernel. It provides:

* **Hierarchical Configuration**: Organize options in menus and submenus
* **Type Safety**: Boolean, integer, string, and hexadecimal types with validation
* **Dependencies**: Express relationships between configuration options
* **Default Values**: Platform-specific and conditional defaults
* **Validation**: Range constraints, symbol dependencies, and consistency checks

.. seealso::

   * :doc:`kconfig` - Kconfig system overview
   * :doc:`kconfig_peripherals` - Peripheral configuration examples
   * :doc:`kconfig_platforms` - Platform-specific configuration
   * :doc:`kconfig_osal` - OSAL backend configuration
   * :doc:`kconfig_tools` - Kconfig tools documentation
   * :doc:`build_system` - Build system integration

Configuration Workflow
^^^^^^^^^^^^^^^^^^^^^^

The following diagram shows the complete Kconfig configuration workflow:

.. mermaid::
   :alt: Kconfig configuration workflow from Kconfig files to generated C header

   flowchart TD
       START([Start Configuration]) --> KCONFIG[Kconfig Files]
       KCONFIG --> DEFCONFIG{Use defconfig?}

       DEFCONFIG -->|Yes| LOADDEF[Load defconfig]
       DEFCONFIG -->|No| MENUCONFIG[Interactive menuconfig]

       LOADDEF --> MERGE[Merge with Kconfig]
       MENUCONFIG --> MERGE

       MERGE --> VALIDATE[Validate Configuration]
       VALIDATE --> VALID{Valid?}

       VALID -->|No| ERROR[Show Errors]
       ERROR --> MENUCONFIG

       VALID -->|Yes| SAVE[Save .config]
       SAVE --> GENERATE[Generate nexus_config.h]
       GENERATE --> HEADER[C Header File]
       HEADER --> BUILD[Build System Uses Config]
       BUILD --> DONE([Configuration Complete])

       style START fill:#e1f5ff
       style KCONFIG fill:#fff4e1
       style MENUCONFIG fill:#ffe1f5
       style VALIDATE fill:#e1ffe1
       style GENERATE fill:#f5e1ff
       style HEADER fill:#ffe1e1
       style DONE fill:#e1f5ff
       style ERROR fill:#ffcccc

Why Use Kconfig?
^^^^^^^^^^^^^^^^

**Benefits:**

* **Compile-Time Configuration**: All configuration resolved before compilation
* **Type Safety**: Prevents invalid configurations
* **Dependency Management**: Automatic handling of option dependencies
* **Multiple Platforms**: Single configuration system for all platforms
* **Validation**: Built-in validation prevents configuration errors
* **Documentation**: Self-documenting with help text

**Use Cases:**

* Platform selection (Native, STM32, GD32, ESP32)
* Peripheral configuration (UART, GPIO, SPI, I2C, Timer, ADC)
* OSAL backend selection (FreeRTOS, RT-Thread, bare-metal)
* Memory layout configuration
* Debug and optimization settings

Basic Concepts
--------------

Configuration Symbols
^^^^^^^^^^^^^^^^^^^^^

A configuration symbol is a named option that can be set to a value:

.. code-block:: Kconfig

    config UART_ENABLE
        bool "Enable UART support"
        default y
        help
          Enable UART peripheral support in the HAL.

**Symbol Types:**

* ``bool``: Boolean (y/n)
* ``tristate``: Three-state (y/m/n) - rarely used in embedded
* ``int``: Integer value
* ``hex``: Hexadecimal value
* ``string``: String value

Symbol Naming
^^^^^^^^^^^^^

**Naming Conventions:**

.. code-block:: text

    PLATFORM_<NAME>                      # Platform selection
    <PLATFORM>_<PERIPHERAL>_ENABLE       # Peripheral enable
    INSTANCE_<PLATFORM>_<PERIPHERAL>_<N> # Instance enable
    <PERIPHERAL><N>_<PARAMETER>          # Instance parameter

**Examples:**

.. code-block:: Kconfig

    config PLATFORM_STM32                # Platform
    config STM32_UART_ENABLE             # Peripheral enable
    config INSTANCE_STM32_UART_1         # Instance enable
    config UART1_BAUDRATE                # Instance parameter

Dependencies
^^^^^^^^^^^^

Use ``depends on`` to make options conditional:

.. code-block:: Kconfig

    config UART1_DMA_ENABLE
        bool "Enable DMA for UART1"
        depends on INSTANCE_STM32_UART_1
        depends on STM32_DMA_ENABLE
        help
          Enable DMA transfers for UART1.

Multiple dependencies are AND'ed together. The option is only visible when all dependencies are satisfied.

Default Values
^^^^^^^^^^^^^^

Provide sensible defaults for all options:

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200
        help
          Baud rate for UART1 in bits per second.

**Conditional Defaults:**

.. code-block:: Kconfig

    config OSAL_HEAP_SIZE
        int "Heap size (bytes)"
        default 32768 if OSAL_FREERTOS
        default 16384 if OSAL_BAREMETAL
        default 65536
        range 4096 1048576

The first matching default is used.

Help Text
^^^^^^^^^

Always provide help text explaining the option:

.. code-block:: Kconfig

    config UART1_TX_BUFFER_SIZE
        int "UART1 TX buffer size"
        default 256
        range 16 4096
        help
          Size of the UART1 transmit buffer in bytes.

          Larger buffers can improve throughput but consume
          more RAM. Must be a power of 2 for optimal performance.

          Recommended values:
          - 128 bytes: Low-bandwidth applications
          - 256 bytes: General purpose (default)
          - 512+ bytes: High-bandwidth applications

Basic Configuration Workflow
----------------------------

Step 1: Select Platform
^^^^^^^^^^^^^^^^^^^^^^^

Choose your target platform:

.. code-block:: Kconfig

    # .config file
    CONFIG_PLATFORM_STM32=y

Or use a default configuration:

.. code-block:: bash

    # Copy platform defconfig
    cp platforms/STM32/defconfig_stm32f4 .config

Step 2: Select Chip
^^^^^^^^^^^^^^^^^^^

For STM32, select chip family and variant:

.. code-block:: Kconfig

    CONFIG_STM32F4=y
    CONFIG_STM32F407=y

Step 3: Configure Peripherals
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Enable and configure required peripherals:

.. code-block:: Kconfig

    # Enable UART
    CONFIG_STM32_UART_ENABLE=y
    CONFIG_INSTANCE_STM32_UART_1=y
    CONFIG_UART1_BAUDRATE=115200
    CONFIG_UART1_MODE_DMA=y

Step 4: Select OSAL Backend
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Choose your RTOS:

.. code-block:: Kconfig

    CONFIG_OSAL_FREERTOS=y
    CONFIG_OSAL_TICK_RATE_HZ=1000
    CONFIG_OSAL_HEAP_SIZE=32768

Step 5: Generate Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Generate the C header file:

.. code-block:: bash

    python scripts/Kconfig/generate_config.py --config .config

This creates ``nexus_config.h``:

.. code-block:: c

    #define NX_CONFIG_PLATFORM_STM32 1
    #define NX_CONFIG_STM32F407 1
    #define NX_CONFIG_UART1_BAUDRATE 115200
    #define NX_CONFIG_OSAL_FREERTOS 1

Step 6: Build Project
^^^^^^^^^^^^^^^^^^^^^

Build with the generated configuration:

.. code-block:: bash

    CMake -B build
    CMake --build build

Intermediate Concepts
---------------------

Menu Organization
^^^^^^^^^^^^^^^^^

Use ``menu`` and ``endmenu`` to group related options:

.. code-block:: Kconfig

    menu "UART Configuration"

    config UART_ENABLE
        bool "Enable UART support"
        default y

    if UART_ENABLE

    config UART_MAX_INSTANCES
        int "Maximum UART instances"
        default 4
        range 1 8

    endif # UART_ENABLE

    endmenu

Menuconfig
^^^^^^^^^^

Use ``menuconfig`` for options that have sub-options:

.. code-block:: Kconfig

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

    endif # INSTANCE_STM32_UART_1

This creates a menu entry that can be enabled/disabled and contains sub-options.

Choice Groups
^^^^^^^^^^^^^

Use ``choice`` for mutually exclusive options:

.. code-block:: Kconfig

    choice
        prompt "UART1 transfer mode"
        default UART1_MODE_DMA

    config UART1_MODE_POLLING
        bool "Polling"
        help
          Use polling mode for UART1 transfers.
          Simple but blocks CPU.

    config UART1_MODE_INTERRUPT
        bool "Interrupt"
        help
          Use interrupt mode for UART1 transfers.
          Non-blocking but requires ISR handling.

    config UART1_MODE_DMA
        bool "DMA"
        help
          Use DMA mode for UART1 transfers.
          Most efficient for large transfers.

    endchoice

Only one option in the choice group can be selected.

Range Constraints
^^^^^^^^^^^^^^^^^

Use ``range`` to constrain integer values:

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200
        range 9600 921600
        help
          Baud rate must be between 9600 and 921600 bps.

**Conditional Ranges:**

.. code-block:: Kconfig

    config TIMER_PRESCALER
        int "Timer prescaler"
        default 1
        range 1 65536 if STM32F4
        range 1 32768 if STM32L4
        help
          Timer prescaler value.
          Range depends on chip family.

Select Statements
^^^^^^^^^^^^^^^^^

Use ``select`` to automatically enable dependencies:

.. code-block:: Kconfig

    config UART1_DMA_ENABLE
        bool "Enable DMA for UART1"
        depends on INSTANCE_STM32_UART_1
        select STM32_DMA_ENABLE
        help
          Enable DMA transfers for UART1.
          Automatically enables DMA subsystem.

When ``UART1_DMA_ENABLE`` is set, ``STM32_DMA_ENABLE`` is automatically enabled.

**Warning**: Use ``select`` sparingly. It can create circular dependencies and override user choices.

Imply Statements
^^^^^^^^^^^^^^^^

Use ``imply`` to suggest but not force dependencies:

.. code-block:: Kconfig

    config UART_ENABLE
        bool "Enable UART support"
        imply DMA_ENABLE
        help
          Enable UART peripheral support.
          DMA is recommended but not required.

``imply`` sets the default but allows the user to override it.

Advanced Concepts
-----------------

Conditional Compilation
^^^^^^^^^^^^^^^^^^^^^^^

Use ``if`` blocks for conditional sections:

.. code-block:: Kconfig

    if PLATFORM_STM32

    source "platforms/STM32/Kconfig_chip"
    source "platforms/STM32/Kconfig_peripherals"

    menu "STM32 Platform Settings"
        # STM32-specific options
    endmenu

    endif # PLATFORM_STM32

Everything inside the ``if`` block is only visible when ``PLATFORM_STM32`` is enabled.

Visibility Conditions
^^^^^^^^^^^^^^^^^^^^^

Control option visibility with ``visible if``:

.. code-block:: Kconfig

    config ADVANCED_FEATURE
        bool "Enable advanced feature"
        visible if EXPERT_MODE
        help
          This option is only visible in expert mode.

Source Directives
^^^^^^^^^^^^^^^^^

Include other Kconfig files:

.. code-block:: Kconfig

    # Absolute path from root
    source "platforms/STM32/Kconfig"

    # Relative path (rsource)
    rsource "adapters/*/Kconfig"

``rsource`` supports wildcards for including multiple files.

Environment Variables
^^^^^^^^^^^^^^^^^^^^^

Access environment variables:

.. code-block:: Kconfig

    config CUSTOM_PLATFORM
        string "Custom platform name"
        default "$(PLATFORM_NAME)"
        help
          Platform name from environment variable.

Derived Symbols
^^^^^^^^^^^^^^^

Create symbols derived from other symbols:

.. code-block:: Kconfig

    config UART_COUNT
        int
        default 1 if INSTANCE_STM32_UART_1
        default 2 if INSTANCE_STM32_UART_2
        default 3 if INSTANCE_STM32_UART_3
        default 0

This symbol has no prompt and is automatically calculated.

Platform-Specific Configuration
-------------------------------

Native Platform
^^^^^^^^^^^^^^^

**Quick Start:**

.. code-block:: bash

    cp platforms/native/defconfig .config
    python scripts/Kconfig/generate_config.py

**Key Options:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_NATIVE=y
    CONFIG_NATIVE_ENABLE_LOGGING=y
    CONFIG_NATIVE_LOG_LEVEL=3
    CONFIG_NATIVE_DMA_CHANNELS=8
    CONFIG_NATIVE_ISR_SLOTS=64

**Peripheral Configuration:**

.. code-block:: Kconfig

    # UART
    CONFIG_NATIVE_UART_ENABLE=y
    CONFIG_INSTANCE_NATIVE_UART_1=y
    CONFIG_UART1_BAUDRATE=115200

    # GPIO
    CONFIG_NATIVE_GPIO_ENABLE=y
    CONFIG_INSTANCE_NATIVE_GPIO_A=y
    CONFIG_GPIO_A_PIN_COUNT=16

STM32F4 Platform
^^^^^^^^^^^^^^^^

**Quick Start:**

.. code-block:: bash

    cp platforms/STM32/defconfig_stm32f4 .config
    python scripts/Kconfig/generate_config.py

**Key Options:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_STM32=y
    CONFIG_STM32F4=y
    CONFIG_STM32F407=y
    CONFIG_STM32_CHIP_NAME="STM32F407xx"

**Memory Configuration:**

.. code-block:: Kconfig

    CONFIG_LINKER_RAM_START=0x20000000
    CONFIG_LINKER_RAM_SIZE=0x00020000
    CONFIG_LINKER_FLASH_START=0x08000000
    CONFIG_LINKER_FLASH_SIZE=0x00100000

STM32H7 Platform
^^^^^^^^^^^^^^^^

**Quick Start:**

.. code-block:: bash

    cp platforms/STM32/defconfig_stm32h7 .config
    python scripts/Kconfig/generate_config.py

**Key Options:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_STM32=y
    CONFIG_STM32H7=y
    CONFIG_STM32H743=y
    CONFIG_STM32_CHIP_NAME="STM32H743xx"

**Memory Configuration:**

.. code-block:: Kconfig

    CONFIG_LINKER_RAM_START=0x24000000
    CONFIG_LINKER_RAM_SIZE=0x00080000
    CONFIG_LINKER_FLASH_START=0x08000000
    CONFIG_LINKER_FLASH_SIZE=0x00200000

GD32 Platform
^^^^^^^^^^^^^

**Key Options:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_GD32=y
    CONFIG_GD32F30X=y
    CONFIG_GD32F303=y

ESP32 Platform
^^^^^^^^^^^^^^

**Key Options:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_ESP32=y
    CONFIG_ESP32_WROOM_32=y

Configuration Workflow
----------------------

Interactive Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^

While Nexus doesn't currently provide a menuconfig UI, you can edit ``.config`` directly:

.. code-block:: bash

    # Edit configuration
    nano .config

    # Validate changes
    python scripts/Kconfig/validate_kconfig.py Kconfig

    # Generate header
    python scripts/Kconfig/generate_config.py

Automated Configuration
^^^^^^^^^^^^^^^^^^^^^^^

Use scripts for automated configuration:

.. code-block:: python

    #!/usr/bin/env python3
    # configure.py

    import subprocess

    def configure_platform(platform, chip=None):
        config_lines = [
            f"CONFIG_PLATFORM_{platform.upper()}=y"
        ]

        if chip:
            config_lines.append(f"CONFIG_{chip.upper()}=y")

        with open('.config', 'w') as f:
            f.write('\n'.join(config_lines))

        # Generate header
        subprocess.run([
            'python', 'scripts/Kconfig/generate_config.py'
        ])

    # Usage
    configure_platform('STM32', 'stm32f407')

Configuration Templates
^^^^^^^^^^^^^^^^^^^^^^^

Create reusable configuration templates:

.. code-block:: bash

    # templates/uart_debug.config
    CONFIG_STM32_UART_ENABLE=y
    CONFIG_INSTANCE_STM32_UART_1=y
    CONFIG_UART1_BAUDRATE=115200
    CONFIG_UART1_MODE_INTERRUPT=y
    CONFIG_HAL_DEBUG_ENABLE=y
    CONFIG_HAL_DEBUG_LEVEL=4

Merge with base configuration:

.. code-block:: bash

    cat platforms/STM32/defconfig_stm32f4 templates/uart_debug.config > .config
    python scripts/Kconfig/generate_config.py

Configuration Validation
------------------------

Syntax Validation
^^^^^^^^^^^^^^^^^

Validate Kconfig syntax:

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py Kconfig

**Checks:**

* Block structure (if/endif, menu/endmenu, choice/endchoice)
* Source file existence
* Symbol definitions
* Syntax errors

Dependency Validation
^^^^^^^^^^^^^^^^^^^^^

Check for dependency issues:

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py --check-deps Kconfig

**Checks:**

* Circular dependencies
* Undefined symbol references
* Unsatisfiable dependencies

Value Validation
^^^^^^^^^^^^^^^^

Validate configuration values:

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py --check-values .config

**Checks:**

* Range constraint violations
* Invalid type values
* Missing required symbols

Configuration Comparison
------------------------

Compare Two Configurations
^^^^^^^^^^^^^^^^^^^^^^^^^^

Show differences between configurations:

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py old.config new.config

**Output:**

.. code-block:: text

    Added symbols:
      CONFIG_NEW_FEATURE=y

    Removed symbols:
      CONFIG_OLD_FEATURE=y

    Changed symbols:
      CONFIG_UART1_BAUDRATE: 9600 -> 115200

JSON Output
^^^^^^^^^^^

Generate machine-readable diff:

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py \
        --format json \
        old.config new.config \
        --output diff.json

Configuration Migration
-----------------------

Version Migration
^^^^^^^^^^^^^^^^^

Migrate configuration to new version:

.. code-block:: bash

    python scripts/Kconfig/kconfig_migrate.py \
        --input old.config \
        --output new.config \
        --version 2.0

**Migration Rules:**

* Renamed symbols are automatically updated
* Deprecated symbols are removed
* New required symbols are added with defaults
* Migration log is generated

Symbol Renaming
^^^^^^^^^^^^^^^

Handle renamed symbols:

.. code-block:: python

    # migration_rules.py
    SYMBOL_RENAMES = {
        'OLD_UART_ENABLE': 'STM32_UART_ENABLE',
        'OLD_BAUDRATE': 'UART1_BAUDRATE',
    }

Best Practices
--------------

Naming Conventions
^^^^^^^^^^^^^^^^^^

**Follow consistent naming:**

.. code-block:: text

    ✓ CONFIG_PLATFORM_STM32
    ✓ CONFIG_STM32_UART_ENABLE
    ✓ CONFIG_INSTANCE_STM32_UART_1
    ✓ CONFIG_UART1_BAUDRATE

    ✗ CONFIG_STM32_PLATFORM
    ✗ CONFIG_ENABLE_UART_STM32
    ✗ CONFIG_STM32_UART1_INSTANCE
    ✗ CONFIG_BAUDRATE_UART1

Organization
^^^^^^^^^^^^

**Group related options:**

.. code-block:: Kconfig

    menu "UART Configuration"

    menuconfig STM32_UART_ENABLE
        bool "Enable UART support"

    if STM32_UART_ENABLE

    menu "UART Instances"
        # Instance configurations
    endmenu

    menu "UART Global Settings"
        # Global settings
    endmenu

    endif # STM32_UART_ENABLE

    endmenu

Documentation
^^^^^^^^^^^^^

**Provide comprehensive help:**

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200
        range 9600 921600
        help
          Baud rate for UART1 in bits per second.

          Common values:
          - 9600: Low-speed serial
          - 19200: Moderate speed
          - 38400: Higher speed
          - 57600: High speed
          - 115200: Very high speed (default)
          - 230400, 460800, 921600: Ultra high speed

          Note: Higher baud rates require more accurate
          clock configuration and may not work reliably
          on all platforms.

Defaults
^^^^^^^^

**Provide sensible defaults:**

.. code-block:: Kconfig

    config UART1_TX_BUFFER_SIZE
        int "UART1 TX buffer size"
        default 256 if UART1_MODE_DMA
        default 128 if UART1_MODE_INTERRUPT
        default 64 if UART1_MODE_POLLING
        default 256
        range 16 4096

Dependencies
^^^^^^^^^^^^

**Use depends on for hard requirements:**

.. code-block:: Kconfig

    config UART1_DMA_ENABLE
        bool "Enable DMA for UART1"
        depends on INSTANCE_STM32_UART_1
        depends on STM32_DMA_ENABLE

**Use select sparingly:**

.. code-block:: Kconfig

    # Good: select for internal dependencies
    config UART_DRIVER
        bool
        select CLOCK_ENABLE
        select GPIO_ENABLE

    # Bad: select for user-visible options
    config UART_ENABLE
        bool "Enable UART"
        select DMA_ENABLE  # Don't force DMA on users

Troubleshooting
---------------

Common Issues
^^^^^^^^^^^^^

**Issue: Symbol not defined**

.. code-block:: text

    Error: Symbol 'UART_ENABLE' is not defined

**Solution:**

.. code-block:: bash

    # Find where symbol should be defined
    grep -r "config UART_ENABLE" .

    # Check if Kconfig file is sourced
    grep -r "source.*uart.*Kconfig" .

**Issue: Circular dependency**

.. code-block:: text

    Error: Circular dependency detected: A -> B -> A

**Solution:**

.. code-block:: Kconfig

    # Bad: circular dependency
    config A
        bool "Option A"
        depends on B

    config B
        bool "Option B"
        depends on A

    # Good: remove circular dependency
    config A
        bool "Option A"

    config B
        bool "Option B"
        depends on A

**Issue: Range violation**

.. code-block:: text

    Error: Value 1000000 is outside range [9600, 921600]

**Solution:**

.. code-block:: Kconfig

    # Fix the value in .config
    CONFIG_UART1_BAUDRATE=115200

**Issue: Configuration not applied**

.. code-block:: bash

    # Regenerate configuration header
    python scripts/Kconfig/generate_config.py --config .config

    # Clean and rebuild
    rm -rf build
    CMake -B build
    CMake --build build

Debugging Configuration
^^^^^^^^^^^^^^^^^^^^^^^

**Enable verbose output:**

.. code-block:: bash

    python scripts/Kconfig/generate_config.py --verbose

**Check generated header:**

.. code-block:: bash

    cat nexus_config.h | grep UART

**Validate dependencies:**

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py --check-deps Kconfig

Next Steps
----------

* :doc:`kconfig_peripherals` - Detailed peripheral configuration examples
* :doc:`kconfig_platforms` - Platform-specific configuration guides
* :doc:`kconfig_tools` - Configuration tools reference
* :doc:`../development/kconfig_guide` - Writing Kconfig files

See Also
--------

* `Kconfig Language <https://www.kernel.org/doc/html/latest/kbuild/Kconfig-language.html>`_
* :doc:`build_system` - Build system integration
* :doc:`porting` - Platform porting guide
