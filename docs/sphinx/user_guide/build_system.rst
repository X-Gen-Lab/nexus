Build System
============

The Nexus Embedded Platform uses CMake as its build system, providing a flexible and cross-platform build infrastructure. This guide covers the CMake structure, build options, and cross-compilation setup.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The Nexus build system is designed to support:

* Multiple target platforms (Native, STM32F4, STM32H7, ESP32, nRF52)
* Multiple OSAL backends (Bare-metal, FreeRTOS, RT-Thread, Zephyr)
* Cross-compilation for ARM targets
* Automated configuration generation via Kconfig
* Unit testing with GoogleTest
* Code coverage analysis
* Documentation generation

.. seealso::

   * :doc:`kconfig` - Configuration system overview
   * :doc:`kconfig_tutorial` - Kconfig tutorial
   * :doc:`ide_integration` - IDE integration guides
   * :doc:`../development/scripts` - Build scripts documentation
   * :doc:`../platform_guides/index` - Platform-specific build instructions
   * :doc:`../tutorials/first_application` - Building your first application

Build Workflow
~~~~~~~~~~~~~~

The following diagram illustrates the complete build workflow from configuration to final binary:

.. mermaid::
   :alt: Build workflow showing the process from Kconfig configuration through CMake to final binary

   flowchart TD
       START([Start Build]) --> KCONFIG[Configure with Kconfig]
       KCONFIG --> GENCONFIG[Generate nexus_config.h]
       GENCONFIG --> CMAKE[Run CMake Configuration]
       CMAKE --> PLATFORM{Select Platform}

       PLATFORM -->|Native| NATIVE[Native Platform Build]
       PLATFORM -->|STM32F4| STM32F4[STM32F4 Platform Build]
       PLATFORM -->|STM32H7| STM32H7[STM32H7 Platform Build]
       PLATFORM -->|GD32| GD32[GD32 Platform Build]

       NATIVE --> COMPILE[Compile Source Files]
       STM32F4 --> COMPILE
       STM32H7 --> COMPILE
       GD32 --> COMPILE

       COMPILE --> LINK[Link Libraries]
       LINK --> BINARY[Generate Binary]
       BINARY --> TEST{Run Tests?}

       TEST -->|Yes| RUNTESTS[Execute Unit Tests]
       TEST -->|No| DONE([Build Complete])
       RUNTESTS --> COVERAGE{Coverage Enabled?}

       COVERAGE -->|Yes| GENCOV[Generate Coverage Report]
       COVERAGE -->|No| DONE
       GENCOV --> DONE

       style START fill:#e1f5ff
       style KCONFIG fill:#fff4e1
       style GENCONFIG fill:#fff4e1
       style CMAKE fill:#ffe1f5
       style COMPILE fill:#e1ffe1
       style LINK fill:#e1ffe1
       style BINARY fill:#e1ffe1
       style DONE fill:#e1f5ff

CMake Structure
---------------

Project Organization
~~~~~~~~~~~~~~~~~~~~

The project follows a hierarchical CMake structure:

.. code-block:: text

   nexus/
   ├── CMakeLists.txt              # Root CMake configuration
   ├── CMake/
   │   ├── modules/                # CMake helper modules
   │   │   ├── FreeRTOS.CMake      # FreeRTOS integration
   │   │   └── NexusHelpers.CMake  # Utility functions
   │   ├── toolchains/             # Toolchain files
   │   │   └── arm-none-eabi.CMake # ARM cross-compilation
   │   ├── linker/                 # Linker scripts
   │   │   ├── nx_sections.ld      # GCC linker script
   │   │   └── nx_sections.sct     # ARM linker script
   │   ├── CTestConfig.CMake       # CTest configuration
   │   └── CTestScript.CMake       # CTest script
   ├── hal/CMakeLists.txt          # HAL library
   ├── osal/CMakeLists.txt         # OSAL library
   ├── framework/CMakeLists.txt    # Framework libraries
   ├── platforms/CMakeLists.txt    # Platform-specific code
   ├── applications/CMakeLists.txt # Example applications
   └── tests/CMakeLists.txt        # Unit tests

Root CMakeLists.txt
~~~~~~~~~~~~~~~~~~~

The root ``CMakeLists.txt`` file:

1. Defines project metadata (name, version, languages)
2. Prevents in-source builds
3. Declares build options
4. Configures compiler flags
5. Integrates Kconfig configuration system
6. Includes subdirectories

Key sections:

.. code-block:: CMake

   # Project definition
   project(nexus
       VERSION 0.1.0
       DESCRIPTION "Nexus Embedded Software Development Platform"
       LANGUAGES C CXX ASM
   )

   # Options
   option(NEXUS_BUILD_TESTS "Build unit tests" ON)
   option(NEXUS_BUILD_EXAMPLES "Build example applications" ON)
   option(NEXUS_BUILD_DOCS "Build documentation" OFF)
   option(NEXUS_ENABLE_COVERAGE "Enable code coverage" OFF)

   # Platform selection
   set(NEXUS_PLATFORM "native" CACHE STRING "Target platform")

   # OSAL backend selection
   set(NEXUS_OSAL_BACKEND "baremetal" CACHE STRING "OSAL backend")

Build Options
-------------

The following CMake options control the build configuration:

Core Build Options
~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 15 15 40

   * - Option
     - Type
     - Default
     - Description
   * - ``NEXUS_BUILD_TESTS``
     - BOOL
     - ON
     - Build unit tests (requires GoogleTest)
   * - ``NEXUS_BUILD_EXAMPLES``
     - BOOL
     - ON
     - Build example applications
   * - ``NEXUS_BUILD_DOCS``
     - BOOL
     - OFF
     - Build documentation (requires Doxygen/Sphinx)
   * - ``NEXUS_ENABLE_COVERAGE``
     - BOOL
     - OFF
     - Enable code coverage instrumentation

Platform Selection
~~~~~~~~~~~~~~~~~~

The ``NEXUS_PLATFORM`` option selects the target platform. Type: STRING, Default: ``native``

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Platform
     - Description
   * - ``native``
     - Native platform for host testing (Windows/Linux/macOS)
   * - ``stm32f4``
     - STM32F4 series microcontrollers (Cortex-M4)
   * - ``stm32h7``
     - STM32H7 series microcontrollers (Cortex-M7)
   * - ``ESP32``
     - ESP32 series microcontrollers (Xtensa LX6)
   * - ``nrf52``
     - nRF52 series microcontrollers (Cortex-M4)

Set platform using:

.. code-block:: bash

   CMake -DNEXUS_PLATFORM=stm32f4 ..

OSAL Backend Selection
~~~~~~~~~~~~~~~~~~~~~~

The ``NEXUS_OSAL_BACKEND`` option selects the RTOS backend. Type: STRING, Default: ``baremetal``

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Backend
     - Description
   * - ``baremetal``
     - Bare-metal (no RTOS)
   * - ``FreeRTOS``
     - FreeRTOS integration
   * - ``rtthread``
     - RT-Thread integration
   * - ``zephyr``
     - Zephyr RTOS integration

Set OSAL backend using:

.. code-block:: bash

   CMake -DNEXUS_OSAL_BACKEND=FreeRTOS ..

Kconfig Options
~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Option
     - Description
   * - ``NEXUS_KCONFIG_FILE``
     - Path to root Kconfig file (default: ``Kconfig``)
   * - ``NEXUS_CONFIG_FILE``
     - Path to configuration file (default: ``.config``)
   * - ``NEXUS_CONFIG_HEADER``
     - Path to generated header (default: ``nexus_config.h``)

Build Types
~~~~~~~~~~~

CMake supports standard build types:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Build Type
     - Description
   * - ``Debug``
     - Debug build with symbols, no optimization (``-Og -g3``)
   * - ``Release``
     - Release build with optimization (``-O2``)
   * - ``RelWithDebInfo``
     - Release with debug info (``-O2 -g``)
   * - ``MinSizeRel``
     - Minimum size release (``-Os``)

Set build type using:

.. code-block:: bash

   CMake -DCMAKE_BUILD_TYPE=Release ..

Building the Project
--------------------

Basic Build
~~~~~~~~~~~

Native platform (host testing):

.. code-block:: bash

   # Create build directory
   mkdir build
   cd build

   # Configure
   CMake ..

   # Build
   CMake --build .

   # Run tests
   ctest --output-on-failure

With specific options:

.. code-block:: bash

   CMake -DNEXUS_BUILD_TESTS=ON \
         -DNEXUS_BUILD_EXAMPLES=ON \
         -DCMAKE_BUILD_TYPE=Debug \
         ..

Cross-Compilation
-----------------

ARM Cortex-M Targets
~~~~~~~~~~~~~~~~~~~~

Prerequisites
^^^^^^^^^^^^^

Install ARM GNU Toolchain:

* **Windows**: Download from `ARM Developer <https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm>`_
* **Linux**: ``sudo apt-get install gcc-arm-none-eabi``
* **macOS**: ``brew install gcc-arm-embedded``

Verify installation:

.. code-block:: bash

   arm-none-eabi-gcc --version

STM32F4 Build
^^^^^^^^^^^^^

.. code-block:: bash

   mkdir build-stm32f4
   cd build-stm32f4

   CMake -DNEXUS_PLATFORM=stm32f4 \
         -DCMAKE_BUILD_TYPE=Release \
         -DNEXUS_BUILD_TESTS=OFF \
         ..

   CMake --build .

STM32H7 Build
^^^^^^^^^^^^^

.. code-block:: bash

   mkdir build-stm32h7
   cd build-stm32h7

   CMake -DNEXUS_PLATFORM=stm32h7 \
         -DCMAKE_BUILD_TYPE=Release \
         -DNEXUS_BUILD_TESTS=OFF \
         ..

   CMake --build .

Toolchain Configuration
~~~~~~~~~~~~~~~~~~~~~~~

The ARM toolchain file (``CMake/toolchains/arm-none-eabi.CMake``) configures:

* Compiler: ``arm-none-eabi-gcc``
* Linker: ``arm-none-eabi-ld``
* Archiver: ``arm-none-eabi-ar``
* Objcopy: ``arm-none-eabi-objcopy``
* Size: ``arm-none-eabi-size``

Compiler flags for ARM:

.. code-block:: bash

   -mcpu=cortex-m4
   -mthumb
   -mfloat-abi=hard
   -mfpu=fpv4-sp-d16
   -ffunction-sections
   -fdata-sections

Linker flags:

.. code-block:: bash

   -Wl,--gc-sections
   -Wl,--print-memory-usage
   -specs=nano.specs
   -specs=nosys.specs

Custom Toolchain Path
^^^^^^^^^^^^^^^^^^^^^

If the toolchain is not in PATH:

.. code-block:: bash

   CMake -DNEXUS_PLATFORM=stm32f4 \
         -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake \
         -DTOOLCHAIN_PREFIX=/path/to/gcc-arm-none-eabi \
         ..

Configuration System
--------------------

Kconfig Integration
~~~~~~~~~~~~~~~~~~~

The build system integrates with Kconfig for configuration management:

1. **Configuration Generation**: During CMake configuration, the ``generate_config.py`` script generates ``nexus_config.h`` from ``.config``

2. **Dependency Tracking**: CMake tracks all Kconfig files and reconfigures when they change

3. **Default Configuration**: If no ``.config`` exists, a default configuration is generated

Configuration Workflow:

.. code-block:: bash

   # 1. Configure with menuconfig (if available)
   python scripts/Kconfig/generate_config.py --menuconfig

   # 2. Or use a defconfig
   cp platforms/STM32/defconfig_stm32f4 .config

   # 3. Generate header
   python scripts/Kconfig/generate_config.py --config .config --output nexus_config.h

   # 4. Build
   CMake --build build

Generated Header
~~~~~~~~~~~~~~~~

The ``nexus_config.h`` header contains:

.. code-block:: c

   /* Generated configuration header */
   #ifndef NEXUS_CONFIG_H
   #define NEXUS_CONFIG_H

   /* Platform configuration */
   #define CONFIG_PLATFORM_STM32F4 1
   #define CONFIG_CHIP_STM32F407 1

   /* Peripheral configuration */
   #define CONFIG_HAL_GPIO 1
   #define CONFIG_HAL_UART 1
   #define CONFIG_HAL_UART_COUNT 3

   /* OSAL configuration */
   #define CONFIG_OSAL_FREERTOS 1
   #define CONFIG_FREERTOS_HEAP_SIZE 32768

   #endif /* NEXUS_CONFIG_H */

Build Directory Structure
-------------------------

After building, the directory structure:

.. code-block:: text

   build/
   ├── CMakeCache.txt              # CMake cache
   ├── CMakeFiles/                 # CMake internal files
   ├── compile_commands.json       # Compilation database
   ├── hal/
   │   └── libnexus_hal.a          # HAL library
   ├── osal/
   │   └── libnexus_osal.a         # OSAL library
   ├── framework/
   │   ├── config/
   │   │   └── libnexus_config.a   # Config library
   │   ├── log/
   │   │   └── libnexus_log.a      # Log library
   │   └── shell/
   │       └── libnexus_shell.a    # Shell library
   ├── platforms/
   │   └── native/
   │       └── libnexus_platform.a # Platform library
   ├── applications/
   │   ├── blinky/
   │   │   └── blinky.exe          # Blinky application
   │   └── shell_demo/
   │       └── shell_demo.exe      # Shell demo
   └── tests/
       └── nexus_tests.exe         # Unit tests

Output Artifacts
~~~~~~~~~~~~~~~~

For embedded targets:

* ``.elf`` - Executable with debug symbols
* ``.bin`` - Raw binary for flashing
* ``.hex`` - Intel HEX format
* ``.map`` - Memory map file

Advanced Build Features
-----------------------

Parallel Builds
~~~~~~~~~~~~~~~

Use multiple cores for faster builds:

.. code-block:: bash

   # Unix/Linux/macOS
   CMake --build . -- -j$(nproc)

   # Windows
   CMake --build . -- /m

Verbose Build
~~~~~~~~~~~~~

Show full compiler commands:

.. code-block:: bash

   CMake --build . --verbose

   # Or
   make VERBOSE=1

Clean Build
~~~~~~~~~~~

.. code-block:: bash

   # Clean build artifacts
   CMake --build . --target clean

   # Or delete build directory
   rm -rf build

Install
~~~~~~~

Install built artifacts:

.. code-block:: bash

   CMake --build . --target install

   # With custom prefix
   CMake -DCMAKE_INSTALL_PREFIX=/opt/nexus ..
   CMake --build . --target install

Code Coverage
~~~~~~~~~~~~~

Enable coverage and generate reports:

.. code-block:: bash

   # Configure with coverage
   CMake -DNEXUS_ENABLE_COVERAGE=ON \
         -DCMAKE_BUILD_TYPE=Debug \
         ..

   # Build and run tests
   CMake --build .
   ctest

   # Generate coverage report (Linux/macOS)
   lcov --capture --directory . --output-file coverage.info
   lcov --remove coverage.info '/usr/*' '*/ext/*' '*/tests/*' --output-file coverage.info
   genhtml coverage.info --output-directory coverage_html

   # Or use scripts
   bash scripts/coverage/run_coverage_linux.sh

IDE Integration
---------------

See :doc:`ide_integration` for detailed IDE setup guides.

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**CMake version too old**

.. code-block:: text

   CMake Error: CMake 3.16 or higher is required.

Solution: Upgrade CMake to version 3.16 or higher.

**Toolchain not found**

.. code-block:: text

   CMake Error: Could not find toolchain file

Solution: Install ARM toolchain and ensure it's in PATH, or specify ``TOOLCHAIN_PREFIX``.

**Configuration generation failed**

.. code-block:: text

   Failed to generate HAL config from .config file

Solution: Validate ``.config`` file or regenerate from defconfig:

.. code-block:: bash

   python scripts/Kconfig/validate_kconfig.py --config .config
   # Or use defconfig
   cp platforms/STM32/defconfig_stm32f4 .config

**Missing dependencies**

.. code-block:: text

   Could NOT find Python3 (missing: Python3_EXECUTABLE)

Solution: Install Python 3.7 or higher.

**Build fails on Windows**

Solution: Use Visual Studio Developer Command Prompt or install MinGW-w64.

Getting Help
~~~~~~~~~~~~

* Check :doc:`../development/contributing` for contribution guidelines
* Visit `GitHub Issues <https://github.com/X-Gen-Lab/nexus/issues>`_
* Join `GitHub Discussions <https://github.com/X-Gen-Lab/nexus/discussions>`_

See Also
--------

* :doc:`kconfig` - Kconfig configuration system
* :doc:`../development/testing` - Testing guide
* :doc:`../getting_started/environment_setup` - Installation guide
* :doc:`ide_integration` - IDE integration guides
