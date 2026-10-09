API Reference
=============

This section provides comprehensive API documentation for all Nexus modules.
The API documentation is automatically generated from source code using Doxygen
and integrated via Breathe.

Core Layers
-----------

Hardware Abstraction Layer (HAL)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The HAL provides a unified interface for hardware peripherals across different platforms.

:doc:`hal`
   Complete HAL API reference including GPIO, UART, SPI, I2C, Timer, ADC, and more.

OS Abstraction Layer (OSAL)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The OSAL provides a portable RTOS interface supporting multiple backends.

:doc:`osal`
   Complete OSAL API reference including tasks, mutexes, semaphores, queues, and timers.

Framework Modules
-----------------

Init Framework
~~~~~~~~~~~~~~

Automatic initialization system using linker sections.

:doc:`init`
   Init Framework API reference with initialization levels and export macros.

Log Framework
~~~~~~~~~~~~~

Flexible logging system with multiple backends and async support.

:doc:`log`
   Log Framework API reference including log levels, backends, and configuration.

Shell Framework
~~~~~~~~~~~~~~~

Interactive command-line interface with autocomplete and history.

:doc:`shell`
   Shell Framework API reference including command registration and line editing.

Config Manager
~~~~~~~~~~~~~~

Configuration management system with multiple storage backends.

:doc:`config`
   Config Manager API reference including data types and backends.

Development Tools
-----------------

Kconfig Tools
~~~~~~~~~~~~~

Tools for managing Kconfig-based configuration.

:doc:`kconfig_tools`
   Kconfig tools API reference for configuration generation and validation.

Module Organization
-------------------

The Nexus API is organized into the following categories:

**Core Layers:**

- **HAL** - Hardware abstraction for peripherals
- **OSAL** - Operating system abstraction

**Framework Modules:**

- **Init** - Automatic initialization system
- **Log** - Logging framework
- **Shell** - Command-line interface
- **Config** - Configuration management

**Development Tools:**

- **Kconfig Tools** - Configuration management tools

API Documentation Standards
---------------------------

All API functions follow these documentation standards:

- **Function signatures** with parameter types and return values
- **Parameter descriptions** with direction (in/out/in-out)
- **Return value descriptions** including error codes
- **Usage examples** for common patterns
- **Thread-safety information** for concurrent usage
- **Cross-references** to related functions

.. toctree::
   :maxdepth: 1
   :hidden:

   hal
   osal
   init
   log
   shell
   config
   kconfig_tools
