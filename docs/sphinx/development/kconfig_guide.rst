Kconfig Writing Guide
=====================

This guide explains how to write Kconfig files for the Nexus platform,
following best practices and conventions.

Overview
--------

Kconfig is a configuration language used by the Linux kernel and many
embedded projects. It provides a hierarchical, type-safe way to define
build-time configuration options.

**Key Concepts:**

* **config**: Define a configuration symbol
* **menuconfig**: Define a configuration symbol that opens a menu
* **choice**: Define mutually exclusive options
* **menu**: Group related options
* **if/endif**: Conditional inclusion
* **depends on**: Specify dependencies
* **select**: Automatically enable dependencies
* **default**: Specify default values
* **range**: Specify valid value ranges
* **help**: Provide documentation

Basic Syntax
------------

Configuration Symbol
^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    config SYMBOL_NAME
        bool "Human-readable description"
        default y
        help
          Detailed help text explaining what this option does.
          Can span multiple lines.

**Types:**

* ``bool``: Boolean (y/n)
* ``int``: Integer
* ``hex``: Hexadecimal
* ``string``: String

**Example:**

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200
        range 9600 921600
        help
          Baud rate for UART1 in bits per second.
          Common values: 9600, 19200, 38400, 57600, 115200

Menu Configuration
^^^^^^^^^^^^^^^^^^

``menuconfig`` creates a configuration symbol that also opens a submenu:

.. code-block:: Kconfig

    menuconfig UART_ENABLE
        bool "Enable UART support"
        default y
        help
          Enable UART peripheral support.

    if UART_ENABLE

    config UART_MAX_INSTANCES
        int "Maximum UART instances"
        default 4
        range 1 8

    endif # UART_ENABLE

Choice Menu
^^^^^^^^^^^

``choice`` creates mutually exclusive options:

.. code-block:: Kconfig

    choice
        prompt "Target Platform"
        default PLATFORM_NATIVE
        help
          Select the target hardware platform.

    config PLATFORM_NATIVE
        bool "Native Platform (PC Simulation)"

    config PLATFORM_STM32
        bool "STM32 Platform"

    config PLATFORM_GD32
        bool "GD32 Platform"

    endchoice

Regular Menu
^^^^^^^^^^^^

``menu`` groups related options without creating a symbol:

.. code-block:: Kconfig

    menu "System Parameters"

    config TICK_RATE_HZ
        int "System tick rate (Hz)"
        default 1000
        range 100 10000

    config HEAP_SIZE
        int "Heap size (bytes)"
        default 32768
        range 4096 1048576

    endmenu

Dependencies
------------

depends on
^^^^^^^^^^

``depends on`` makes an option visible only when dependencies are met:

.. code-block:: Kconfig

    config UART1_DMA_ENABLE
        bool "Enable DMA for UART1"
        depends on UART1_ENABLE && DMA_ENABLE
        default y
        help
          Use DMA for UART1 transfers.

**Operators:**

* ``&&``: Logical AND
* ``||``: Logical OR
* ``!``: Logical NOT
* ``=``: Equality
* ``!=``: Inequality

**Example:**

.. code-block:: Kconfig

    config ADVANCED_FEATURE
        bool "Advanced feature"
        depends on PLATFORM_STM32 && (STM32F4 || STM32H7)
        depends on !MINIMAL_BUILD

select
^^^^^^

``select`` automatically enables dependencies:

.. code-block:: Kconfig

    config USB_DEVICE
        bool "USB Device support"
        select USB_CORE
        select DMA_ENABLE
        help
          Enable USB device functionality.

**Note:** Use ``select`` sparingly. Prefer ``depends on`` when possible.

Conditional Inclusion
^^^^^^^^^^^^^^^^^^^^^

``if/endif`` blocks conditionally include configuration options:

.. code-block:: Kconfig

    if PLATFORM_STM32

    config STM32_CHIP_NAME
        string "STM32 chip name"
        default "STM32F407xx"

    source "platforms/STM32/Kconfig_chip"

    endif # PLATFORM_STM32

Default Values
--------------

Simple Defaults
^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200

Conditional Defaults
^^^^^^^^^^^^^^^^^^^^

Defaults can depend on other options:

.. code-block:: Kconfig

    config HEAP_SIZE
        int "Heap size (bytes)"
        default 32768 if OSAL_FREERTOS
        default 16384 if OSAL_BAREMETAL
        default 65536

**Evaluation order:** First matching condition is used.

Expression Defaults
^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    config PLATFORM_NAME
        string
        default "native" if PLATFORM_NATIVE
        default "STM32" if PLATFORM_STM32
        default "GD32" if PLATFORM_GD32

Range Constraints
-----------------

Integer Ranges
^^^^^^^^^^^^^^

.. code-block:: Kconfig

    config BUFFER_SIZE
        int "Buffer size"
        default 256
        range 16 4096
        help
          Buffer size in bytes. Must be between 16 and 4096.

Conditional Ranges
^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    config STACK_SIZE
        int "Stack size (bytes)"
        default 2048
        range 512 8192 if PLATFORM_STM32
        range 1024 16384 if PLATFORM_NATIVE

Hexadecimal Values
^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    config RAM_START
        hex "RAM start address"
        default 0x20000000
        range 0x20000000 0x2FFFFFFF

File Inclusion
--------------

source Directive
^^^^^^^^^^^^^^^^

``source`` includes a file with absolute or relative path:

.. code-block:: Kconfig

    # Absolute path from project root
    source "platforms/STM32/Kconfig"

    # Relative path
    source "Kconfig_chip"

rsource Directive
^^^^^^^^^^^^^^^^^

``rsource`` includes a file with path relative to current file:

.. code-block:: Kconfig

    # In platforms/Kconfig
    rsource "native/Kconfig"
    rsource "STM32/Kconfig"
    rsource "GD32/Kconfig"

**Best Practice:** Use ``rsource`` for platform-specific includes.

Naming Conventions
------------------

Symbol Names
^^^^^^^^^^^^

**Platform symbols:**

.. code-block:: text

    PLATFORM_<NAME>                 # Platform selection
    <PLATFORM>_<COMPONENT>_<PARAM>  # Platform-specific

**Examples:**

.. code-block:: text

    PLATFORM_STM32
    STM32_UART_ENABLE
    STM32_CHIP_NAME

**Peripheral instances:**

.. code-block:: text

    INSTANCE_<PLATFORM>_<PERIPHERAL>_<N>  # Instance enable
    <PERIPHERAL><N>_<PARAMETER>           # Instance parameter

**Examples:**

.. code-block:: text

    INSTANCE_STM32_UART_1
    UART1_BAUDRATE
    UART1_MODE_DMA

**OSAL symbols:**

.. code-block:: text

    OSAL_<BACKEND>                  # Backend selection
    OSAL_<PARAMETER>                # OSAL parameter

**Examples:**

.. code-block:: text

    OSAL_FREERTOS
    OSAL_TICK_RATE_HZ
    OSAL_HEAP_SIZE

Prompt Text
^^^^^^^^^^^

* Use sentence case
* Be concise but descriptive
* Include units in parentheses

**Examples:**

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"  # Good

    config UART1_BAUDRATE
        int "Baud Rate For UART1"  # Bad: wrong case

    config TIMEOUT
        int "Timeout (ms)"  # Good: includes unit

Help Text
^^^^^^^^^

* First line: brief description
* Following lines: detailed explanation
* Include examples if helpful
* Document valid ranges and common values

**Example:**

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200
        help
          Baud rate for UART1 in bits per second.

          Common values:
          - 9600: Low-speed communication
          - 115200: Standard debug console
          - 921600: High-speed data transfer

          Note: Higher baud rates require more accurate
          clock configuration.

Configuration Patterns
----------------------

Platform Configuration
^^^^^^^^^^^^^^^^^^^^^^

**Pattern:**

.. code-block:: Kconfig

    # Platform selection (choice)
    choice
        prompt "Target Platform"
        default PLATFORM_NATIVE

    config PLATFORM_<NAME>
        bool "Platform description"

    endchoice

    # Platform-specific configuration
    if PLATFORM_<NAME>

    # Platform settings
    menu "Platform Settings"
        # ...
    endmenu

    # Peripheral configuration
    source "platforms/<name>/Kconfig_peripherals"

    endif

Peripheral Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

**Pattern:**

.. code-block:: Kconfig

    # Peripheral enable (menuconfig)
    menuconfig <PLATFORM>_<PERIPHERAL>_ENABLE
        bool "Enable <peripheral> support"
        default y

    if <PLATFORM>_<PERIPHERAL>_ENABLE

    # Max instances
    config <PLATFORM>_<PERIPHERAL>_MAX_INSTANCES
        int "Maximum instances"
        default 4
        range 1 8

    # Instance configuration
    menuconfig INSTANCE_<PLATFORM>_<PERIPHERAL>_<N>
        bool "Enable <peripheral><N>"
        default y

    if INSTANCE_<PLATFORM>_<PERIPHERAL>_<N>

    # Instance parameters
    config <PERIPHERAL><N>_<PARAMETER>
        int/bool/string "Parameter description"
        default <value>

    endif

    endif

Transfer Mode Selection
^^^^^^^^^^^^^^^^^^^^^^^

**Pattern:**

.. code-block:: Kconfig

    choice
        prompt "Transfer mode"
        default <PERIPHERAL><N>_MODE_DMA

    config <PERIPHERAL><N>_MODE_POLLING
        bool "Polling"

    config <PERIPHERAL><N>_MODE_INTERRUPT
        bool "Interrupt"

    config <PERIPHERAL><N>_MODE_DMA
        bool "DMA"

    endchoice

    # Mode value for code
    config <PERIPHERAL><N>_MODE_VALUE
        int
        default 0 if <PERIPHERAL><N>_MODE_POLLING
        default 1 if <PERIPHERAL><N>_MODE_INTERRUPT
        default 2 if <PERIPHERAL><N>_MODE_DMA

Buffer Configuration
^^^^^^^^^^^^^^^^^^^^

**Pattern:**

.. code-block:: Kconfig

    config <PERIPHERAL><N>_TX_BUFFER_SIZE
        int "TX buffer size"
        default 256
        range 16 4096

    config <PERIPHERAL><N>_RX_BUFFER_SIZE
        int "RX buffer size"
        default 256
        range 16 4096

Validation
----------

Syntax Validation
^^^^^^^^^^^^^^^^^

Always validate Kconfig files:

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py Kconfig

**Common errors:**

* Unclosed blocks (if/endif, menu/endmenu, choice/endchoice)
* Missing source files
* Undefined symbols in dependencies
* Invalid range constraints

Dependency Validation
^^^^^^^^^^^^^^^^^^^^^

Check for:

* Circular dependencies
* Undefined symbols
* Conflicting dependencies

**Example:**

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py Kconfig

    # Output:
    # Errors:
    #   Circular dependency: A -> B -> C -> A
    #   Undefined symbol: INVALID_SYMBOL

Testing
-------

Test Configuration
^^^^^^^^^^^^^^^^^^

Create test configurations:

.. code-block:: bash

    # Test with minimal config
    echo "CONFIG_PLATFORM_NATIVE=y" > test.config
    python scripts/Kconfig/generate_config.py --config test.config

    # Test with full config
    cp platforms/STM32/defconfig_stm32f4 test.config
    python scripts/Kconfig/generate_config.py --config test.config

Verify Generated Headers
^^^^^^^^^^^^^^^^^^^^^^^^

Check generated headers:

.. code-block:: bash

    # Generate header
    python scripts/Kconfig/generate_config.py --config .config

    # Verify macros
    grep "NX_CONFIG_" nexus_config.h

Property-Based Testing
^^^^^^^^^^^^^^^^^^^^^^

Write property tests for configuration:

.. code-block:: python

    from hypothesis import given, strategies as st
    from validate_kconfig import KconfigValidator

    @given(st.integers(min_value=9600, max_value=921600))
    def test_baudrate_range(baudrate):
        """Test that all valid baudrates are accepted"""
        config = f"CONFIG_UART1_BAUDRATE={baudrate}"
        # Validate configuration
        assert validate_config(config)

Best Practices
--------------

Organization
^^^^^^^^^^^^

1. **Group related options** in menus
2. **Use menuconfig** for enable + settings pattern
3. **Keep files focused** on single component
4. **Use consistent naming** across platforms

Documentation
^^^^^^^^^^^^^

1. **Always provide help text**
2. **Document valid ranges**
3. **Include examples** for complex options
4. **Explain dependencies**

Dependencies
^^^^^^^^^^^^

1. **Prefer depends on** over select
2. **Avoid circular dependencies**
3. **Keep dependency chains short**
4. **Document why dependencies exist**

Defaults
^^^^^^^^

1. **Provide sensible defaults**
2. **Use conditional defaults** for platform-specific values
3. **Document default rationale** in help text

Validation
^^^^^^^^^^

1. **Use range constraints** for integers
2. **Validate before committing**
3. **Test with multiple configurations**
4. **Check generated headers**

Common Pitfalls
---------------

Circular Dependencies
^^^^^^^^^^^^^^^^^^^^^

**Bad:**

.. code-block:: Kconfig

    config A
        bool "Feature A"
        depends on B

    config B
        bool "Feature B"
        depends on A

**Good:**

.. code-block:: Kconfig

    config A
        bool "Feature A"

    config B
        bool "Feature B"
        depends on A

Overuse of select
^^^^^^^^^^^^^^^^^

**Bad:**

.. code-block:: Kconfig

    config FEATURE
        bool "Feature"
        select DEPENDENCY1
        select DEPENDENCY2
        select DEPENDENCY3

**Good:**

.. code-block:: Kconfig

    config FEATURE
        bool "Feature"
        depends on DEPENDENCY1 && DEPENDENCY2 && DEPENDENCY3

Missing Help Text
^^^^^^^^^^^^^^^^^

**Bad:**

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200

**Good:**

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200
        help
          Baud rate for UART1 in bits per second.
          Common values: 9600, 19200, 38400, 57600, 115200

Inconsistent Naming
^^^^^^^^^^^^^^^^^^^

**Bad:**

.. code-block:: Kconfig

    config UART1_BAUD
        int "UART1 baud rate"

    config UART2_BAUDRATE
        int "UART2 baud rate"

**Good:**

.. code-block:: Kconfig

    config UART1_BAUDRATE
        int "UART1 baud rate"

    config UART2_BAUDRATE
        int "UART2 baud rate"

Examples
--------

Complete Platform Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    # platforms/STM32/Kconfig
    if PLATFORM_STM32

    config STM32_PLATFORM_NAME
        string
        default "STM32 Platform"

    config STM32_PLATFORM_VERSION
        string
        default "1.0.0"

    # Chip selection
    source "platforms/STM32/Kconfig_chip"

    menu "STM32 Platform Settings"

    config STM32_ENABLE_LOGGING
        bool "Enable platform logging"
        default y

    config STM32_LOG_LEVEL
        int "Platform log level"
        default 3
        range 0 4
        depends on STM32_ENABLE_LOGGING

    endmenu

    # Peripheral configuration
    source "platforms/STM32/Kconfig_peripherals"

    endif # PLATFORM_STM32

Complete Peripheral Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: Kconfig

    # platforms/STM32/src/uart/Kconfig
    config STM32_UART_MAX_INSTANCES
        int "Maximum UART instances"
        default 6
        range 1 8

    menuconfig INSTANCE_STM32_UART_1
        bool "Enable UART1"
        default y

    if INSTANCE_STM32_UART_1

    config UART1_BAUDRATE
        int "UART1 baud rate"
        default 115200
        help
          Baud rate for UART1 in bits per second.

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

    config UART1_MODE_VALUE
        int
        default 0 if UART1_MODE_POLLING
        default 1 if UART1_MODE_INTERRUPT
        default 2 if UART1_MODE_DMA

    config UART1_TX_BUFFER_SIZE
        int "UART1 TX buffer size"
        default 256
        range 16 4096

    config UART1_RX_BUFFER_SIZE
        int "UART1 RX buffer size"
        default 256
        range 16 4096

    endif # INSTANCE_STM32_UART_1

See Also
--------

* :doc:`../user_guide/kconfig` - Kconfig user guide
* :doc:`../api/kconfig_tools` - Kconfig tools API
* :doc:`contributing` - Contributing guidelines
* Configuration Guide: ``docs/configuration_guide.md``
* Kconfig Writing Guide: ``docs/kconfig_writing_guide.md``

