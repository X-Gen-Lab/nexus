Architecture
============

This document provides a detailed overview of the Nexus platform architecture.

Design Principles
-----------------

Nexus is built on the following design principles:

1. **Separation of Concerns**: Each layer has a specific responsibility
2. **Abstraction**: Hide implementation details behind clean interfaces
3. **Portability**: Minimize platform-specific code in applications
4. **Testability**: Design for unit testing and simulation
5. **Resource Efficiency**: Optimize for constrained embedded systems

Layer Architecture
------------------

.. code-block:: text

    ┌─────────────────────────────────────────────────────────────┐
    │                      Applications                            │
    ├─────────────────────────────────────────────────────────────┤
    │                      Middleware                              │
    │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │
    │  │  Shell  │ │   Log   │ │  Config │ │  Event  │           │
    │  └─────────┘ └─────────┘ └─────────┘ └─────────┘           │
    ├─────────────────────────────────────────────────────────────┤
    │                         OSAL                                 │
    │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │
    │  │  Task   │ │  Mutex  │ │  Queue  │ │  Timer  │           │
    │  └─────────┘ └─────────┘ └─────────┘ └─────────┘           │
    ├─────────────────────────────────────────────────────────────┤
    │                          HAL                                 │
    │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │
    │  │  GPIO   │ │  UART   │ │   SPI   │ │   I2C   │           │
    │  └─────────┘ └─────────┘ └─────────┘ └─────────┘           │
    ├─────────────────────────────────────────────────────────────┤
    │                    Platform / Hardware                       │
    │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │
    │  │ STM32F4 │ │ STM32H7 │ │  ESP32  │ │  nRF52  │           │
    │  └─────────┘ └─────────┘ └─────────┘ └─────────┘           │
    └─────────────────────────────────────────────────────────────┘

Application Layer
~~~~~~~~~~~~~~~~~

User applications built on top of Nexus platform. Applications should:

- Use only public APIs from lower layers
- Avoid direct hardware access
- Be portable across supported platforms

Middleware Layer
~~~~~~~~~~~~~~~~

Common services that provide higher-level functionality:

- **Log Framework**: Flexible logging with multiple backends
- **Shell**: Command-line interface for debugging
- **Config**: Configuration management and persistence
- **Event**: Event-driven programming support

See :doc:`log` for detailed Log Framework documentation.

OSAL Layer
~~~~~~~~~~

OS Abstraction Layer provides portable RTOS primitives:

- **Task**: Thread/task management
- **Mutex**: Mutual exclusion locks
- **Semaphore**: Binary and counting semaphores
- **Queue**: Message queues for inter-task communication
- **Timer**: Software timers

Supported backends:

- FreeRTOS
- Native (pthreads for PC simulation)
- Bare-metal (cooperative scheduling)

See :doc:`osal` for detailed OSAL documentation.

HAL Layer
~~~~~~~~~

Hardware Abstraction Layer provides unified peripheral APIs:

- **GPIO**: General-purpose I/O
- **UART**: Serial communication
- **SPI**: SPI bus master/slave
- **I2C**: I2C bus master/slave
- **Timer**: Hardware timers and PWM
- **ADC**: Analog-to-digital conversion

See :doc:`hal` for detailed HAL documentation.

Platform Layer
~~~~~~~~~~~~~~

MCU-specific implementations including:

- Startup code and system initialization
- Linker scripts
- Vendor SDK integration
- Clock and power management

Data Flow
---------

.. code-block:: text

    Application
        │
        ▼
    ┌─────────────────┐
    │   Middleware    │  ← Uses OSAL for threading, HAL for I/O
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │      OSAL       │  ← Uses HAL for timing
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │       HAL       │  ← Uses Platform for hardware access
    └────────┬────────┘
             │
             ▼
    ┌─────────────────┐
    │    Platform     │  ← Direct hardware access
    └─────────────────┘

Dependency Rules
----------------

Strict dependency rules ensure clean architecture:

1. **Upper layers depend on lower layers** (never reverse)
2. **Same-layer components are independent** (no horizontal dependencies)
3. **Platform layer is the only layer with hardware access**

.. code-block:: text

    ✅ Application → Middleware → OSAL → HAL → Platform
    ❌ HAL → Middleware (reverse dependency)
    ❌ GPIO → UART (horizontal dependency within HAL)

Memory Management
-----------------

Nexus supports multiple memory management strategies:

**Static Allocation**
    All memory allocated at compile time. Suitable for safety-critical systems.

**Dynamic Allocation**
    Runtime memory allocation using malloc/free. More flexible but requires
    careful management.

**Pool Allocation**
    Fixed-size memory pools for predictable allocation patterns.

Configuration example:

.. code-block:: c

    /* Static allocation mode */
    #define NEXUS_USE_STATIC_ALLOC  1
    #define NEXUS_MAX_TASKS         8
    #define NEXUS_MAX_MUTEXES       16

Error Handling
--------------

All Nexus APIs follow consistent error handling:

.. code-block:: c

    /* All functions return status codes */
    hal_status_t status = hal_gpio_init(port, pin, &config);
    if (status != HAL_OK) {
        /* Handle error */
        LOG_ERROR("GPIO init failed: %d", status);
    }

Common status codes:

+------------------------+----------------------------------+
| Status                 | Description                      |
+========================+==================================+
| ``XXX_OK``             | Operation successful             |
+------------------------+----------------------------------+
| ``XXX_ERROR``          | General error                    |
+------------------------+----------------------------------+
| ``XXX_ERROR_PARAM``    | Invalid parameter                |
+------------------------+----------------------------------+
| ``XXX_ERROR_STATE``    | Invalid state                    |
+------------------------+----------------------------------+
| ``XXX_ERROR_TIMEOUT``  | Operation timed out              |
+------------------------+----------------------------------+
| ``XXX_ERROR_NO_MEMORY``| Memory allocation failed         |
+------------------------+----------------------------------+

Build System
------------

Nexus uses CMake for cross-platform build configuration:

.. code-block:: bash

    # Select platform
    cmake -B build -DNEXUS_PLATFORM=stm32f4

    # Enable features
    cmake -B build \
        -DNEXUS_PLATFORM=stm32f4 \
        -DNEXUS_BUILD_TESTS=ON \
        -DNEXUS_ENABLE_LOG=ON

Key CMake options:

+---------------------------+----------------------------------+
| Option                    | Description                      |
+===========================+==================================+
| ``NEXUS_PLATFORM``        | Target platform (stm32f4, native)|
+---------------------------+----------------------------------+
| ``NEXUS_BUILD_TESTS``     | Build unit tests                 |
+---------------------------+----------------------------------+
| ``NEXUS_ENABLE_LOG``      | Enable log framework             |
+---------------------------+----------------------------------+
| ``NEXUS_ENABLE_COVERAGE`` | Enable code coverage             |
+---------------------------+----------------------------------+

Next Steps
----------

- :doc:`hal` - Learn about the Hardware Abstraction Layer
- :doc:`osal` - Learn about the OS Abstraction Layer
- :doc:`log` - Learn about the Log Framework
- :doc:`porting` - Port Nexus to a new platform
