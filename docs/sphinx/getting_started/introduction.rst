Introduction
============

What is Nexus?
--------------

Nexus is a world-class embedded software development platform designed for
building reliable, secure, and portable embedded applications. It provides
a comprehensive set of abstraction layers and middleware components that
enable developers to write code once and deploy across multiple MCU families.

Key Features
------------

- **Unified APIs**: Consistent interfaces across different hardware platforms
- **Portability**: Write once, run on multiple MCU families (STM32, ESP32, nRF52)
- **Quality**: MISRA C compliant, thoroughly tested code with 90%+ coverage
- **Security**: Built-in security features for IoT applications
- **Real-time**: Support for FreeRTOS and bare-metal configurations
- **Ecosystem**: Rich middleware, cloud integration, and tooling

Why Nexus?
----------

**For Embedded Developers:**

- Reduce time-to-market with ready-to-use components
- Focus on application logic instead of low-level drivers
- Easy migration between MCU platforms

**For Teams:**

- Consistent coding standards across projects
- Comprehensive documentation and examples
- Active community and support

Architecture Overview
---------------------

Nexus follows a layered architecture that separates hardware-specific code
from application logic:

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

Layer Descriptions
~~~~~~~~~~~~~~~~~~

**Application Layer**
    User applications built on top of Nexus platform.

**Middleware Layer**
    Common services including logging, shell, configuration management,
    and event handling.

**OSAL (OS Abstraction Layer)**
    Portable interface for RTOS primitives: tasks, mutexes, semaphores,
    queues, and timers. Supports FreeRTOS and bare-metal backends.

**HAL (Hardware Abstraction Layer)**
    Unified API for hardware peripherals: GPIO, UART, SPI, I2C, Timer, ADC.
    Hides hardware differences from application code.

**Platform Layer**
    MCU-specific implementations including startup code, linker scripts,
    and vendor SDK integration.

Supported Platforms
-------------------

+------------+------------------+--------+------------------+
| Platform   | MCU              | Status | RTOS Support     |
+============+==================+========+==================+
| STM32F4    | STM32F407VGT6    | ✅     | FreeRTOS, Bare   |
+------------+------------------+--------+------------------+
| STM32H7    | STM32H743ZIT6    | 🚧     | FreeRTOS, Bare   |
+------------+------------------+--------+------------------+
| ESP32      | ESP32-WROOM-32   | 🚧     | FreeRTOS         |
+------------+------------------+--------+------------------+
| nRF52      | nRF52840         | 🚧     | FreeRTOS, Zephyr |
+------------+------------------+--------+------------------+
| Native     | x86/x64          | ✅     | pthreads         |
+------------+------------------+--------+------------------+

Legend: ✅ Supported, 🚧 In Progress

Project Structure
-----------------

::

    nexus/
    ├── hal/                    # Hardware Abstraction Layer
    │   ├── include/            # Public headers
    │   └── src/                # Common HAL code
    ├── osal/                   # OS Abstraction Layer
    │   ├── include/            # Public headers
    │   └── adapters/           # RTOS adapters (FreeRTOS, native)
    ├── framework/              # Middleware components
    │   └── log/                # Logging framework
    ├── platforms/              # Platform-specific code
    │   ├── stm32f4/            # STM32F4 platform
    │   └── native/             # Native (PC) platform
    ├── applications/           # Example applications
    ├── tests/                  # Unit tests
    ├── docs/                   # Documentation
    └── cmake/                  # CMake modules and toolchains

License
-------

Nexus is released under the MIT License. See the LICENSE file for details.

Getting Started
---------------

Ready to start? Follow these guides:

1. :doc:`installation` - Set up your development environment
2. :doc:`quickstart` - Build your first Nexus application
3. :doc:`../user_guide/architecture` - Deep dive into the architecture
