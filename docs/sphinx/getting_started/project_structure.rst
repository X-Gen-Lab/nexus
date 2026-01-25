Project Structure
=================

Understanding the Nexus project structure will help you navigate the codebase and know where to find what you need.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

Nexus follows a layered architecture with clear separation of concerns. The directory structure reflects this design:

.. code-block:: text

   nexus/
   ├── hal/                    # Hardware Abstraction Layer
   ├── osal/                   # OS Abstraction Layer
   ├── framework/              # Framework components
   ├── platforms/              # Platform-specific code
   ├── applications/           # Example applications
   ├── tests/                  # Unit and integration tests
   ├── docs/                   # Documentation
   ├── scripts/                # Build and utility scripts
   ├── cmake/                  # CMake modules and toolchains
   ├── vendors/                # Vendor SDKs
   ├── ext/                    # External dependencies
   └── .github/                # CI/CD workflows

Directory Structure
-------------------

Core Layers
~~~~~~~~~~~

HAL - Hardware Abstraction Layer
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Location**: ``hal/``

Provides unified APIs for hardware peripherals.

.. code-block:: text

   hal/
   ├── include/hal/            # Public API headers
   │   ├── hal.h               # Main HAL header
   │   ├── gpio.h              # GPIO interface
   │   ├── uart.h              # UART interface
   │   ├── spi.h               # SPI interface
   │   ├── i2c.h               # I2C interface
   │   ├── timer.h             # Timer interface
   │   ├── adc.h               # ADC interface
   │   ├── pwm.h               # PWM interface
   │   ├── can.h               # CAN interface
   │   └── dma.h               # DMA interface
   ├── src/                    # Common implementations
   │   ├── hal_common.c        # Common HAL functions
   │   └── hal_utils.c         # Utility functions
   ├── docs/                   # HAL documentation
   │   ├── README.md           # Overview
   │   ├── USER_GUIDE.md       # User guide
   │   ├── DESIGN.md           # Design document
   │   ├── TEST_GUIDE.md       # Testing guide
   │   ├── PORTING_GUIDE.md    # Porting guide
   │   └── TROUBLESHOOTING.md  # Troubleshooting
   ├── Kconfig                 # HAL configuration
   └── CMakeLists.txt          # Build configuration

**Key Files**:

* ``hal.h`` - Main header, includes all HAL interfaces
* ``gpio.h`` - GPIO operations (read, write, toggle, configure)
* ``uart.h`` - UART communication (send, receive, configure)
* ``spi.h`` - SPI master/slave operations
* ``i2c.h`` - I2C master/slave operations

**Documentation**: See :doc:`../user_guide/hal` for detailed HAL documentation.

OSAL - OS Abstraction Layer
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Location**: ``osal/``

Provides portable RTOS primitives.

.. code-block:: text

   osal/
   ├── include/osal/           # Public API headers
   │   ├── osal.h              # Main OSAL header
   │   ├── task.h              # Task management
   │   ├── mutex.h             # Mutex operations
   │   ├── semaphore.h         # Semaphore operations
   │   ├── queue.h             # Message queues
   │   ├── timer.h             # Software timers
   │   ├── event.h             # Event flags
   │   └── memory.h            # Memory management
   ├── adapters/               # RTOS adapters
   │   ├── baremetal/          # Bare-metal implementation
   │   ├── freertos/           # FreeRTOS adapter
   │   ├── rtthread/           # RT-Thread adapter
   │   └── zephyr/             # Zephyr adapter
   ├── docs/                   # OSAL documentation
   │   ├── README.md           # Overview
   │   ├── USER_GUIDE.md       # User guide
   │   ├── DESIGN.md           # Design document
   │   ├── TEST_GUIDE.md       # Testing guide
   │   ├── PORTING_GUIDE.md    # Porting guide
   │   └── TROUBLESHOOTING.md  # Troubleshooting
   ├── Kconfig                 # OSAL configuration
   └── CMakeLists.txt          # Build configuration

**Key Files**:

* ``osal.h`` - Main header, includes all OSAL interfaces
* ``task.h`` - Task creation, deletion, delay, priority
* ``mutex.h`` - Mutual exclusion locks
* ``semaphore.h`` - Binary and counting semaphores
* ``queue.h`` - Inter-task message passing

**Documentation**: See :doc:`../user_guide/osal` for detailed OSAL documentation.

Framework Layer
^^^^^^^^^^^^^^^

**Location**: ``framework/``

High-level framework components.

.. code-block:: text

   framework/
   ├── config/                 # Configuration management
   │   ├── include/config/     # Public headers
   │   ├── src/                # Implementation
   │   ├── docs/               # Documentation
   │   ├── Kconfig             # Configuration
   │   └── CMakeLists.txt      # Build
   ├── log/                    # Logging framework
   │   ├── include/log/        # Public headers
   │   ├── src/                # Implementation
   │   ├── docs/               # Documentation
   │   ├── Kconfig             # Configuration
   │   └── CMakeLists.txt      # Build
   ├── shell/                  # Command shell
   │   ├── include/shell/      # Public headers
   │   ├── src/                # Implementation
   │   ├── docs/               # Documentation
   │   ├── Kconfig             # Configuration
   │   └── CMakeLists.txt      # Build
   └── init/                   # Initialization framework
       ├── include/init/       # Public headers
       ├── src/                # Implementation
       ├── docs/               # Documentation
       ├── Kconfig             # Configuration
       └── CMakeLists.txt      # Build

**Components**:

* **Config**: Key-value configuration storage with namespaces, JSON/binary import/export
* **Log**: Flexible logging with multiple backends, log levels, module filtering
* **Shell**: Interactive command-line interface with command registration, history, completion
* **Init**: Initialization framework with dependency management and priority levels

**Documentation**:

* :doc:`../user_guide/config` - Configuration system
* :doc:`../user_guide/log` - Logging framework
* :doc:`../user_guide/shell` - Shell system

Platform Layer
~~~~~~~~~~~~~~

**Location**: ``platforms/``

Platform-specific implementations.

.. code-block:: text

   platforms/
   ├── native/                 # Native platform (PC simulation)
   │   ├── include/            # Platform headers
   │   ├── src/                # Platform implementation
   │   │   ├── gpio_native.c   # GPIO simulation
   │   │   ├── uart_native.c   # UART simulation
   │   │   └── ...             # Other peripherals
   │   ├── defconfig           # Default configuration
   │   ├── defconfig.example   # Example configuration
   │   ├── Kconfig             # Platform configuration
   │   ├── README.md           # Platform documentation
   │   └── CMakeLists.txt      # Build configuration
   ├── stm32/                  # STM32 family
   │   ├── src/                # STM32 implementations
   │   │   ├── stm32f4/        # STM32F4 specific
   │   │   ├── stm32h7/        # STM32H7 specific
   │   │   └── common/         # Common STM32 code
   │   ├── defconfig_stm32f4   # STM32F4 default config
   │   ├── defconfig_stm32h7   # STM32H7 default config
   │   ├── Kconfig             # STM32 configuration
   │   ├── Kconfig_chip        # Chip selection
   │   └── Kconfig_peripherals # Peripheral configuration
   ├── gd32/                   # GD32 family
   │   ├── src/                # GD32 implementations
   │   ├── Kconfig             # GD32 configuration
   │   └── CMakeLists.txt      # Build configuration
   ├── esp32/                  # ESP32 family (planned)
   │   └── Kconfig             # ESP32 configuration
   ├── nrf52/                  # nRF52 family (planned)
   │   └── Kconfig             # nRF52 configuration
   ├── Kconfig                 # Platform selection
   └── CMakeLists.txt          # Platform build logic

**Platform Guides**:

* :doc:`../platform_guides/native` - Native platform
* :doc:`../platform_guides/stm32f4` - STM32F4 platform
* :doc:`../platform_guides/stm32h7` - STM32H7 platform
* :doc:`../platform_guides/gd32` - GD32 platform

Applications
~~~~~~~~~~~~

**Location**: ``applications/``

Example applications demonstrating Nexus features.

.. code-block:: text

   applications/
   ├── blinky/                 # LED blinky example
   │   ├── main.c              # Application code
   │   └── CMakeLists.txt      # Build configuration
   ├── shell_demo/             # Shell/CLI demo
   │   ├── main.c              # Application code
   │   └── CMakeLists.txt      # Build configuration
   ├── config_demo/            # Config manager demo
   │   ├── main.c              # Application code
   │   └── CMakeLists.txt      # Build configuration
   ├── freertos_demo/          # FreeRTOS demo
   │   ├── main.c              # Application code
   │   └── CMakeLists.txt      # Build configuration
   └── CMakeLists.txt          # Applications build logic

**Examples**:

* **blinky**: Simple LED blinking (GPIO, delays)
* **shell_demo**: Interactive CLI (UART, Shell, commands)
* **config_demo**: Configuration management (Config API, JSON/binary)
* **freertos_demo**: Multi-tasking (FreeRTOS, tasks, queues, mutexes)

See :doc:`examples_tour` for detailed example descriptions.

Tests
~~~~~

**Location**: ``tests/``

Comprehensive test suite with 1539+ tests.

.. code-block:: text

   tests/
   ├── hal/                    # HAL tests
   │   ├── test_gpio.cpp       # GPIO unit tests
   │   ├── test_uart.cpp       # UART unit tests
   │   ├── test_spi.cpp        # SPI unit tests
   │   ├── test_i2c.cpp        # I2C unit tests
   │   └── ...                 # Other peripheral tests
   ├── osal/                   # OSAL tests
   │   ├── test_task.cpp       # Task tests
   │   ├── test_mutex.cpp      # Mutex tests
   │   ├── test_queue.cpp      # Queue tests
   │   └── ...                 # Other OSAL tests
   ├── config/                 # Config framework tests
   │   ├── test_config.cpp     # Config API tests
   │   ├── test_namespace.cpp  # Namespace tests
   │   └── ...                 # Other config tests
   ├── log/                    # Log framework tests
   │   ├── test_log.cpp        # Log API tests
   │   ├── test_backend.cpp    # Backend tests
   │   └── ...                 # Other log tests
   ├── shell/                  # Shell framework tests
   │   ├── test_shell.cpp      # Shell API tests
   │   ├── test_command.cpp    # Command tests
   │   └── ...                 # Other shell tests
   ├── init/                   # Init framework tests
   │   └── test_init.cpp       # Init tests
   ├── integration/            # Integration tests
   │   └── test_integration.cpp
   └── CMakeLists.txt          # Test build configuration

**Test Statistics**:

* Total: 1539+ tests
* HAL: ~400 tests
* OSAL: ~200 tests
* Config: ~300 tests
* Log: ~130 tests
* Shell: ~400 tests
* Init: ~15 tests
* Integration: ~40 tests

**Documentation**: See :doc:`../development/testing` for testing guide.

Documentation
~~~~~~~~~~~~~

**Location**: ``docs/``

Project documentation.

.. code-block:: text

   docs/
   ├── sphinx/                 # Sphinx documentation
   │   ├── getting_started/    # Getting started guides
   │   ├── user_guide/         # User guides
   │   ├── tutorials/          # Step-by-step tutorials
   │   ├── platform_guides/    # Platform-specific guides
   │   ├── api/                # API reference
   │   ├── reference/          # Reference documentation
   │   ├── development/        # Development guides
   │   ├── _static/            # Static assets
   │   ├── _templates/         # Documentation templates
   │   ├── locale/             # Translations (Chinese)
   │   ├── conf.py             # Sphinx configuration
   │   ├── index.rst           # Documentation index
   │   └── requirements.txt    # Python dependencies
   └── api/                    # Doxygen API docs (generated)

**Build Documentation**:

.. code-block:: bash

   # Build all documentation
   python scripts/tools/docs.py

   # Or build separately
   doxygen Doxyfile                                    # API docs
   cd docs/sphinx && sphinx-build -b html . _build/html  # User guides

Build System
~~~~~~~~~~~~

**Location**: ``cmake/``

CMake modules and toolchains.

.. code-block:: text

   cmake/
   ├── modules/                # CMake helper modules
   │   ├── FreeRTOS.cmake      # FreeRTOS integration
   │   └── NexusHelpers.cmake  # Utility functions
   ├── toolchains/             # Toolchain files
   │   └── arm-none-eabi.cmake # ARM cross-compilation
   ├── linker/                 # Linker scripts
   │   ├── nx_sections.ld      # GCC linker script
   │   └── nx_sections.sct     # ARM linker script
   ├── CTestConfig.cmake       # CTest configuration
   └── CTestScript.cmake       # CTest script

**Documentation**: See :doc:`../user_guide/build_system` for build system details.

Scripts
~~~~~~~

**Location**: ``scripts/``

Build and utility scripts.

.. code-block:: text

   scripts/
   ├── building/               # Build scripts
   │   ├── build.py            # Cross-platform build script
   │   ├── build.sh            # Linux/macOS build script
   │   └── build.bat           # Windows build script
   ├── test/                   # Test scripts
   │   ├── test.py             # Cross-platform test script
   │   ├── test.sh             # Linux/macOS test script
   │   └── test.bat            # Windows test script
   ├── tools/                  # Utility scripts
   │   ├── format.py           # Code formatting
   │   ├── clean.py            # Clean build artifacts
   │   └── docs.py             # Documentation generation
   ├── coverage/               # Coverage scripts
   │   ├── run_coverage_linux.sh
   │   └── run_coverage_windows.ps1
   └── kconfig/                # Kconfig scripts
       ├── generate_config.py  # Configuration generation
       └── validate_kconfig.py # Configuration validation

**Documentation**: See :doc:`../development/scripts` for script documentation.

External Dependencies
~~~~~~~~~~~~~~~~~~~~~

**Location**: ``ext/``

External libraries and dependencies.

.. code-block:: text

   ext/
   ├── freertos/               # FreeRTOS kernel (submodule)
   │   ├── Source/             # FreeRTOS source
   │   └── portable/           # Port files
   └── googletest/             # Google Test framework (submodule)
       ├── googletest/         # GTest
       └── googlemock/         # GMock

**Vendor SDKs**: ``vendors/``

.. code-block:: text

   vendors/
   ├── st/                     # STMicroelectronics
   │   ├── cmsis/              # CMSIS headers
   │   ├── stm32f4xx_hal/      # STM32F4 HAL
   │   └── stm32h7xx_hal/      # STM32H7 HAL
   ├── arm/                    # ARM CMSIS
   │   └── cmsis/              # CMSIS core
   ├── espressif/              # Espressif (planned)
   └── nordic/                 # Nordic Semiconductor (planned)

Configuration Files
-------------------

Root Configuration
~~~~~~~~~~~~~~~~~~

**CMakeLists.txt**

Root CMake configuration file.

**Kconfig**

Root Kconfig file for configuration system.

**nexus_config.h**

Generated configuration header (auto-generated from ``.config``).

**.config**

Kconfig configuration file (user-editable or generated from defconfig).

**defconfig**

Default configuration for each platform (in ``platforms/*/defconfig``).

Build Configuration
~~~~~~~~~~~~~~~~~~~

**.clang-format**

Code formatting rules for clang-format.

**.clang-tidy**

Static analysis rules for clang-tidy.

**.editorconfig**

Editor configuration for consistent coding style.

Version Control
~~~~~~~~~~~~~~~

**.gitignore**

Git ignore rules.

**.gitmodules**

Git submodule configuration.

**.gitattributes**

Git attributes for line endings and diff.

CI/CD
~~~~~

**.github/workflows/**

GitHub Actions workflows:

* ``build.yml`` - Multi-platform build
* ``test.yml`` - Unit tests with coverage
* ``docs.yml`` - Documentation deployment

File Naming Conventions
-----------------------

Headers
~~~~~~~

* Public headers: ``include/<module>/<name>.h``
* Private headers: ``src/<name>_internal.h``
* Platform headers: ``platforms/<platform>/include/<name>.h``

Source Files
~~~~~~~~~~~~

* Implementation: ``src/<name>.c``
* Platform implementation: ``platforms/<platform>/src/<name>_<platform>.c``
* Tests: ``tests/<module>/test_<name>.cpp``

Documentation
~~~~~~~~~~~~~

* Markdown: ``README.md``, ``DESIGN.md``, ``USER_GUIDE.md``
* reStructuredText: ``*.rst`` (Sphinx documentation)
* Doxygen: In-source comments with ``\brief``, ``\param``, ``\return``

Code Organization
-----------------

Module Structure
~~~~~~~~~~~~~~~~

Each module follows a consistent structure:

.. code-block:: text

   <module>/
   ├── include/<module>/       # Public API headers
   │   └── <module>.h          # Main module header
   ├── src/                    # Implementation
   │   ├── <module>.c          # Main implementation
   │   └── <module>_internal.h # Private headers
   ├── docs/                   # Module documentation
   │   ├── README.md           # Overview
   │   ├── USER_GUIDE.md       # User guide
   │   └── DESIGN.md           # Design document
   ├── Kconfig                 # Module configuration
   └── CMakeLists.txt          # Build configuration

Header Organization
~~~~~~~~~~~~~~~~~~~

Headers are organized by visibility:

**Public Headers** (``include/``):

* Installed with the library
* Used by applications
* Stable API

**Private Headers** (``src/``):

* Internal implementation details
* Not installed
* Can change without breaking API

Include Paths
~~~~~~~~~~~~~

Use module-qualified includes:

.. code-block:: c

   /* Correct */
   #include "hal/gpio.h"
   #include "osal/task.h"
   #include "config/config.h"

   /* Incorrect */
   #include "gpio.h"
   #include "task.h"
   #include "config.h"

Navigation Tips
---------------

Finding Code
~~~~~~~~~~~~

**By Feature**:

* GPIO operations → ``hal/include/hal/gpio.h``
* Task management → ``osal/include/osal/task.h``
* Logging → ``framework/log/include/log/log.h``
* Configuration → ``framework/config/include/config/config.h``

**By Platform**:

* Native GPIO → ``platforms/native/src/gpio_native.c``
* STM32F4 GPIO → ``platforms/stm32/src/stm32f4/gpio_stm32f4.c``

**By Example**:

* LED blinking → ``applications/blinky/main.c``
* Shell commands → ``applications/shell_demo/main.c``

Finding Documentation
~~~~~~~~~~~~~~~~~~~~~

**User Guides**:

* Getting Started → ``docs/sphinx/getting_started/``
* User Guide → ``docs/sphinx/user_guide/``
* Tutorials → ``docs/sphinx/tutorials/``

**API Reference**:

* Doxygen → ``docs/api/`` (generated)
* Sphinx API → ``docs/sphinx/api/``

**Module Docs**:

* HAL → ``hal/docs/``
* OSAL → ``osal/docs/``
* Config → ``framework/config/docs/``

Next Steps
----------

Now that you understand the project structure:

1. :doc:`build_and_flash` - Learn build and deployment workflows
2. :doc:`first_application` - Create your own application
3. :doc:`core_concepts` - Understand Nexus architecture
4. :doc:`../user_guide/architecture` - Deep dive into architecture

See Also
--------

* :doc:`../user_guide/build_system` - Build system documentation
* :doc:`../development/coding_standards` - Coding standards
* :doc:`../development/contributing` - Contributing guide
