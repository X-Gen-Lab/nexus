Build System
============

The Nexus Embedded Platform uses CMake with CMake Presets as its build system, providing a modern, flexible, and cross-platform build infrastructure. This guide covers the CMake structure, preset-based workflow, build options, testing, and cross-compilation setup.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The Nexus build system is designed to support:

* **CMake Presets**: Pre-configured build configurations for all platforms
* **Multiple target platforms**: Native, STM32F4, STM32H7, GD32, ESP32, nRF52
* **Multiple OSAL backends**: Bare-metal, FreeRTOS, RT-Thread, Zephyr
* **Cross-compilation**: ARM Cortex-M targets with toolchain management
* **Automated configuration**: Kconfig-based configuration generation
* **Comprehensive testing**: GoogleTest with CTest integration
* **Code coverage**: Built-in coverage analysis support
* **CI/CD ready**: GitHub Actions workflow included

.. seealso::

   * :doc:`kconfig` - Configuration system overview
   * :doc:`kconfig_tutorial` - Kconfig tutorial
   * :doc:`ide_integration` - IDE integration guides
   * :doc:`../development/scripts` - Build scripts documentation
   * :doc:`../platform_guides/index` - Platform-specific build instructions
   * :doc:`../tutorials/first_application` - Building your first application

Key Features
~~~~~~~~~~~~

✨ **Modern CMake Presets**
   Pre-configured build settings for all platforms and compilers, ensuring consistent builds across teams.

🚀 **Fast Parallel Builds**
   Multi-core compilation support with automatic job detection.

🧪 **Integrated Testing**
   CTest integration with 1600+ unit tests and property-based testing.

📊 **Coverage Analysis**
   Built-in code coverage support for quality assurance.

🔧 **IDE Integration**
   Native support for VS Code, Visual Studio, and CLion.

🤖 **CI/CD Ready**
   GitHub Actions workflow with multi-platform testing.

Build Workflow
~~~~~~~~~~~~~~

The following diagram illustrates the complete build workflow from configuration to final binary:

.. mermaid::
   :alt: Build workflow showing the process from Kconfig configuration through CMake to final binary

   flowchart TD
       START([Start Build]) --> PRESET{Choose Method}

       PRESET -->|CMake Presets| SELECTPRESET[Select Preset]
       PRESET -->|Build Script| BUILDSCRIPT[Use build.py]
       PRESET -->|Manual CMake| MANUAL[Manual Configuration]

       SELECTPRESET --> KCONFIG[Configure with Kconfig]
       BUILDSCRIPT --> KCONFIG
       MANUAL --> KCONFIG

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

Quick Start
-----------

The fastest way to build Nexus is using CMake Presets:

.. code-block:: bash

   # List available presets
   cmake --list-presets

   # Configure with a preset
   cmake --preset windows-msvc-debug

   # Build
   cmake --build --preset windows-msvc-debug

   # Test
   ctest --preset windows-msvc-debug

Or use the Python build script for even simpler workflow:

.. code-block:: bash

   # Auto-detect platform and build
   python scripts/building/build.py

   # Build with specific preset and run tests
   python scripts/building/build.py -p windows-msvc-debug -t

   # Parallel build with 8 jobs
   python scripts/building/build.py -p linux-gcc-release -j8

CMake Presets
-------------

What are CMake Presets?
~~~~~~~~~~~~~~~~~~~~~~~

CMake Presets (introduced in CMake 3.19) provide a standard way to specify common build configurations. They eliminate the need to remember complex CMake command-line arguments and ensure consistent builds across different environments.

**Benefits:**

* 📋 **Consistent Configuration**: Same settings for all developers
* 🚀 **Quick Setup**: One command to configure, build, and test
* 🔄 **Easy Switching**: Switch between configurations instantly
* 🤝 **Team Collaboration**: Share configurations via version control
* 🤖 **CI/CD Integration**: Perfect for automated builds

Available Presets
~~~~~~~~~~~~~~~~~

List all available presets:

.. code-block:: bash

   # List configure presets
   cmake --list-presets

   # List build presets
   cmake --build --list-presets

   # List test presets
   ctest --list-presets

Windows Presets
^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 20 20 30

   * - Preset Name
     - Compiler
     - Generator
     - Description
   * - ``windows-msvc-debug``
     - MSVC
     - VS 2022
     - Debug build with Visual Studio
   * - ``windows-msvc-release``
     - MSVC
     - VS 2022
     - Release build with Visual Studio
   * - ``windows-gcc-debug``
     - GCC
     - Ninja
     - Debug build with MinGW GCC
   * - ``windows-gcc-release``
     - GCC
     - Ninja
     - Release build with MinGW GCC
   * - ``windows-clang-debug``
     - Clang
     - Ninja
     - Debug build with Clang
   * - ``windows-clang-release``
     - Clang
     - Ninja
     - Release build with Clang

Linux Presets
^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 20 20 30

   * - Preset Name
     - Compiler
     - Generator
     - Description
   * - ``linux-gcc-debug``
     - GCC
     - Ninja
     - Debug build with GCC
   * - ``linux-gcc-release``
     - GCC
     - Ninja
     - Release build with GCC
   * - ``linux-gcc-coverage``
     - GCC
     - Ninja
     - Debug build with coverage
   * - ``linux-clang-debug``
     - Clang
     - Ninja
     - Debug build with Clang
   * - ``linux-clang-release``
     - Clang
     - Ninja
     - Release build with Clang

macOS Presets
^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 20 20 30

   * - Preset Name
     - Compiler
     - Generator
     - Description
   * - ``macos-clang-debug``
     - Clang
     - Ninja
     - Debug build with AppleClang
   * - ``macos-clang-release``
     - Clang
     - Ninja
     - Release build with AppleClang

Cross-Compilation Presets
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Preset Name
     - Target
     - Description
   * - ``cross-arm-debug``
     - STM32F4
     - ARM Cortex-M4 Debug build
   * - ``cross-arm-release``
     - STM32F4
     - ARM Cortex-M4 Release build

Using Presets
~~~~~~~~~~~~~

Basic Workflow
^^^^^^^^^^^^^^

.. code-block:: bash

   # 1. Configure with preset
   cmake --preset windows-msvc-debug

   # 2. Build with preset
   cmake --build --preset windows-msvc-debug

   # 3. Test with preset
   ctest --preset windows-msvc-debug

All-in-One Command
^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   # Configure, build, and test in one go
   cmake --preset linux-gcc-debug && \
   cmake --build --preset linux-gcc-debug && \
   ctest --preset linux-gcc-debug

Parallel Builds
^^^^^^^^^^^^^^^

.. code-block:: bash

   # Build with 8 parallel jobs
   cmake --build --preset windows-msvc-debug -j8

   # Auto-detect CPU count
   cmake --build --preset linux-gcc-release -j$(nproc)

Preset Configuration
~~~~~~~~~~~~~~~~~~~~

Presets are defined in ``CMakePresets.json`` at the project root:

.. code-block:: json

   {
       "version": 6,
       "cmakeMinimumRequired": {
           "major": 3,
           "minor": 21,
           "patch": 0
       },
       "configurePresets": [
           {
               "name": "windows-msvc-debug",
               "displayName": "Windows MSVC Debug",
               "description": "Windows build with MSVC compiler (Debug)",
               "generator": "Visual Studio 17 2022",
               "binaryDir": "${sourceDir}/build/${presetName}",
               "cacheVariables": {
                   "CMAKE_BUILD_TYPE": "Debug",
                   "NEXUS_BUILD_TESTS": "ON",
                   "NEXUS_PLATFORM": "native"
               }
           }
       ],
       "buildPresets": [
           {
               "name": "windows-msvc-debug",
               "configurePreset": "windows-msvc-debug",
               "configuration": "Debug"
           }
       ],
       "testPresets": [
           {
               "name": "windows-msvc-debug",
               "configurePreset": "windows-msvc-debug",
               "configuration": "Debug",
               "output": {
                   "outputOnFailure": true
               }
           }
       ]
   }

Build Script
------------

The ``scripts/building/build.py`` script provides a convenient wrapper around CMake presets with additional features.

Features
~~~~~~~~

* 🎯 **Auto-detection**: Automatically selects appropriate preset for your platform
* 📋 **Preset listing**: View all available presets
* 🧹 **Clean builds**: Option to clean before building
* 🧪 **Integrated testing**: Run tests after building
* ⚡ **Parallel builds**: Automatic CPU detection for optimal performance

Usage
~~~~~

.. code-block:: bash

   # Show help
   python scripts/building/build.py --help

   # List available presets
   python scripts/building/build.py --list

   # Build with auto-detected preset
   python scripts/building/build.py

   # Build with specific preset
   python scripts/building/build.py --preset windows-msvc-debug

   # Build and test
   python scripts/building/build.py -p linux-gcc-debug --test

   # Clean build with 8 jobs
   python scripts/building/build.py -p windows-msvc-release --clean -j8

Command-Line Options
~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``-p, --preset <name>``
     - CMake preset name (auto-detected if not specified)
   * - ``-l, --list``
     - List all available CMake presets
   * - ``-c, --clean``
     - Clean build directory before building
   * - ``-j, --jobs <n>``
     - Number of parallel jobs (default: auto-detect)
   * - ``-t, --test``
     - Run tests after building
   * - ``-h, --help``
     - Show help message

Examples
~~~~~~~~

.. code-block:: bash

   # Quick build for current platform
   python scripts/building/build.py

   # Development workflow (build + test)
   python scripts/building/build.py -p windows-msvc-debug -t

   # Release build with maximum parallelism
   python scripts/building/build.py -p linux-gcc-release -j$(nproc)

   # Clean release build
   python scripts/building/build.py -p macos-clang-release -c

   # CI/CD workflow
   python scripts/building/build.py -p linux-gcc-coverage -t

Testing
-------

The Nexus project includes comprehensive testing with 1600+ unit tests covering all components.

Running Tests
~~~~~~~~~~~~~

Using CTest Presets (Recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The most elegant way to run tests:

.. code-block:: bash

   # Run all tests with preset
   ctest --preset windows-msvc-debug

   # Run tests in parallel (8 jobs)
   ctest --preset linux-gcc-debug -j8

   # Show output only on failure
   ctest --preset windows-msvc-debug --output-on-failure

   # Quiet mode (summary only)
   ctest --preset linux-gcc-release --quiet

   # Run specific test
   ctest --preset windows-msvc-debug -R "GPIOTest.*"

Using Build Script
^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   # Build and test in one command
   python scripts/building/build.py -p windows-msvc-debug --test

   # Or use short form
   python scripts/building/build.py -p linux-gcc-debug -t

Manual CTest
^^^^^^^^^^^^

.. code-block:: bash

   # Run all tests
   cd build/windows-msvc-debug
   ctest -C Debug

   # Run with verbose output
   ctest -C Debug --verbose

   # Run specific test suite
   ctest -C Debug -R "OsalTest"

Test Output
~~~~~~~~~~~

CTest provides clear, formatted output:

.. code-block:: text

   Test project D:/code/nexus/nexus/build/windows-msvc-debug
         Start   1: CRCTest.CRC32_EmptyData
    1/1631 Test   #1: CRCTest.CRC32_EmptyData ..................   Passed    0.01 sec
         Start   2: CRCTest.CRC32_SingleByte
    2/1631 Test   #2: CRCTest.CRC32_SingleByte .................   Passed    0.01 sec
         Start   3: CRCTest.CRC32_KnownValue
    3/1631 Test   #3: CRCTest.CRC32_KnownValue .................   Passed    0.01 sec

   ...

   100% tests passed, 0 tests failed out of 1631

   Total Test time (real) = 45.23 sec

Test Categories
~~~~~~~~~~~~~~~

The test suite includes:

**Unit Tests** (1400+ tests)
   Test individual components in isolation:

   * HAL drivers (GPIO, UART, SPI, I2C, Timer, ADC, DAC, etc.)
   * OSAL primitives (Tasks, Mutexes, Semaphores, Queues, Events)
   * Framework components (Log, Shell, Config, Init)

**Property-Based Tests** (200+ tests)
   Verify system properties using randomized inputs:

   * Lifecycle consistency
   * State machine correctness
   * Concurrency safety
   * Resource management

**Integration Tests** (30+ tests)
   Test component interactions:

   * HAL + OSAL integration
   * Framework integration
   * Platform-specific features

Test Organization
~~~~~~~~~~~~~~~~~

Tests are organized by component:

.. code-block:: text

   tests/
   ├── hal/                    # HAL tests
   │   ├── gpio_tests.cpp
   │   ├── uart_tests.cpp
   │   └── ...
   ├── osal/                   # OSAL tests
   │   ├── task_tests.cpp
   │   ├── mutex_tests.cpp
   │   └── ...
   ├── framework/              # Framework tests
   │   ├── log_tests.cpp
   │   ├── shell_tests.cpp
   │   └── ...
   └── integration/            # Integration tests
       └── integration_tests.cpp

Filtering Tests
~~~~~~~~~~~~~~~

Run specific test subsets:

.. code-block:: bash

   # Run only GPIO tests
   ctest --preset windows-msvc-debug -R "GPIO"

   # Run all HAL tests
   ctest --preset linux-gcc-debug -R "hal_"

   # Exclude property tests
   ctest --preset windows-msvc-debug -E "Property"

   # Run tests matching pattern
   ctest --preset linux-gcc-debug -R "Mutex.*Lock"

Test Timeout
~~~~~~~~~~~~

Configure test timeout:

.. code-block:: bash

   # Set 5-minute timeout
   ctest --preset windows-msvc-debug --timeout 300

   # No timeout
   ctest --preset linux-gcc-debug --timeout 0

Test Verbosity
~~~~~~~~~~~~~~

Control output verbosity:

.. code-block:: bash

   # Minimal output
   ctest --preset windows-msvc-debug --quiet

   # Normal output (default)
   ctest --preset linux-gcc-debug

   # Verbose output
   ctest --preset windows-msvc-debug --verbose

   # Extra verbose (debug)
   ctest --preset linux-gcc-debug --extra-verbose

Test Results
~~~~~~~~~~~~

Export test results:

.. code-block:: bash

   # JUnit XML format
   ctest --preset windows-msvc-debug --output-junit results.xml

   # JSON format
   ctest --preset linux-gcc-debug --output-json results.json

Debugging Failed Tests
~~~~~~~~~~~~~~~~~~~~~~

When tests fail:

.. code-block:: bash

   # Show output only for failed tests
   ctest --preset windows-msvc-debug --output-on-failure

   # Rerun only failed tests
   ctest --preset windows-msvc-debug --rerun-failed

   # Stop on first failure
   ctest --preset linux-gcc-debug --stop-on-failure

CMake Structure
---------------

Project Organization
~~~~~~~~~~~~~~~~~~~~

The project follows a hierarchical CMake structure:

.. code-block:: text

   nexus/
   ├── CMakeLists.txt              # Root CMake configuration
   ├── CMakePresets.json           # CMake presets definition
   ├── cmake/
   │   ├── modules/                # CMake helper modules
   │   │   ├── NexusPlatform.cmake # Platform detection & config
   │   │   ├── NexusHelpers.cmake  # Utility functions
   │   │   └── FreeRTOS.cmake      # FreeRTOS integration
   │   ├── toolchains/             # Toolchain files
   │   │   └── arm-none-eabi.cmake # ARM cross-compilation
   │   ├── linker/                 # Linker scripts
   │   │   ├── nx_sections.ld      # GCC linker script
   │   │   └── nx_sections.sct     # ARM linker script
   │   ├── CTestConfig.cmake       # CTest configuration
   │   └── CTestScript.cmake       # CTest automation script
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
4. Includes NexusPlatform module for platform detection
5. Configures compiler flags based on detected compiler
6. Integrates Kconfig configuration system
7. Includes subdirectories

Key sections:

.. code-block:: cmake

   # Project definition
   cmake_minimum_required(VERSION 3.21)
   project(nexus
       VERSION 0.1.0
       DESCRIPTION "Nexus Embedded Software Development Platform"
       LANGUAGES C CXX ASM
   )

   # Include platform detection module
   include(cmake/modules/NexusPlatform.cmake)

   # Options
   option(NEXUS_BUILD_TESTS "Build unit tests" ON)
   option(NEXUS_BUILD_EXAMPLES "Build example applications" ON)
   option(NEXUS_BUILD_DOCS "Build documentation" OFF)
   option(NEXUS_ENABLE_COVERAGE "Enable code coverage" OFF)

   # Platform selection
   set(NEXUS_PLATFORM "native" CACHE STRING "Target platform")

   # OSAL backend selection
   set(NEXUS_OSAL_BACKEND "baremetal" CACHE STRING "OSAL backend")

   # Configure compiler flags
   nexus_configure_compiler_flags()

   # Add subdirectories
   add_subdirectory(hal)
   add_subdirectory(osal)
   add_subdirectory(framework)
   add_subdirectory(platforms)
   add_subdirectory(applications)
   if(NEXUS_BUILD_TESTS)
       add_subdirectory(tests)
   endif()

NexusPlatform Module
~~~~~~~~~~~~~~~~~~~~

The ``cmake/modules/NexusPlatform.cmake`` module provides:

**Platform Detection**
   Automatically detects Windows, Linux, and macOS

**Compiler Detection**
   Identifies MSVC, GCC, Clang, and AppleClang

**Generator Detection**
   Recognizes Visual Studio, Ninja, and Make

**Compiler Configuration**
   Applies appropriate flags for each compiler:

   * MSVC: ``/W4 /WX /permissive- /MP``
   * GCC/Clang: ``-Wall -Wextra -Wpedantic -Werror``

**Output Directory Management**
   Organizes build artifacts consistently:

   * Libraries: ``${CMAKE_BINARY_DIR}/lib``
   * Executables: ``${CMAKE_BINARY_DIR}/bin``
   * Archives: ``${CMAKE_BINARY_DIR}/lib``

Usage:

.. code-block:: cmake

   # Include the module
   include(cmake/modules/NexusPlatform.cmake)

   # Platform detection is automatic
   # Access detected values:
   message("Platform: ${NEXUS_DETECTED_PLATFORM}")
   message("Compiler: ${NEXUS_DETECTED_COMPILER}")
   message("Generator: ${NEXUS_DETECTED_GENERATOR}")

   # Configure compiler flags
   nexus_configure_compiler_flags()

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

Using CMake Presets (Recommended)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The modern, recommended approach using CMake Presets:

Configure
^^^^^^^^^

.. code-block:: bash

   # List available presets
   cmake --list-presets

   # Configure with a preset
   cmake --preset <preset-name>

Examples:

.. code-block:: bash

   # Windows with MSVC
   cmake --preset windows-msvc-debug

   # Linux with GCC
   cmake --preset linux-gcc-debug

   # macOS with Clang
   cmake --preset macos-clang-debug

   # Cross-compilation for ARM
   cmake --preset cross-arm-debug

Build
^^^^^

.. code-block:: bash

   # Build with preset
   cmake --build --preset <preset-name>

   # Build with parallel jobs
   cmake --build --preset <preset-name> -j8

Examples:

.. code-block:: bash

   # Debug build
   cmake --build --preset windows-msvc-debug

   # Release build with 8 jobs
   cmake --build --preset linux-gcc-release -j8

   # Cross-compilation
   cmake --build --preset cross-arm-release

Test
^^^^

.. code-block:: bash

   # Run tests with preset
   ctest --preset <preset-name>

   # Run tests in parallel
   ctest --preset <preset-name> -j8

   # Show output on failure
   ctest --preset <preset-name> --output-on-failure

Examples:

.. code-block:: bash

   # Run all tests
   ctest --preset windows-msvc-debug

   # Parallel testing
   ctest --preset linux-gcc-debug -j8 --output-on-failure

   # Quiet mode (summary only)
   ctest --preset macos-clang-release --quiet

Complete Workflow
^^^^^^^^^^^^^^^^^

.. code-block:: bash

   # Configure, build, and test
   cmake --preset windows-msvc-debug && \
   cmake --build --preset windows-msvc-debug -j8 && \
   ctest --preset windows-msvc-debug -j8 --output-on-failure

Using Build Script
~~~~~~~~~~~~~~~~~~

The Python build script provides a simpler interface:

.. code-block:: bash

   # Auto-detect platform and build
   python scripts/building/build.py

   # Build with specific preset
   python scripts/building/build.py --preset windows-msvc-debug

   # Build and test
   python scripts/building/build.py -p linux-gcc-debug --test

   # Clean build with 8 jobs
   python scripts/building/build.py -p windows-msvc-release --clean -j8

   # List available presets
   python scripts/building/build.py --list

Manual CMake (Legacy)
~~~~~~~~~~~~~~~~~~~~~

Traditional CMake workflow (not recommended for new projects):

.. code-block:: bash

   # Create build directory
   mkdir build
   cd build

   # Configure
   cmake -DCMAKE_BUILD_TYPE=Debug \
         -DNEXUS_BUILD_TESTS=ON \
         -DNEXUS_PLATFORM=native \
         ..

   # Build
   cmake --build . -j8

   # Test
   ctest --output-on-failure

.. note::

   Using CMake Presets is strongly recommended over manual configuration as it ensures consistent builds and simplifies the workflow.

Platform-Specific Builds
~~~~~~~~~~~~~~~~~~~~~~~~

Windows
^^^^^^^

**MSVC (Visual Studio 2022)**

.. code-block:: bash

   # Debug
   cmake --preset windows-msvc-debug
   cmake --build --preset windows-msvc-debug

   # Release
   cmake --preset windows-msvc-release
   cmake --build --preset windows-msvc-release

**MinGW GCC**

.. code-block:: bash

   # Ensure MinGW is in PATH
   cmake --preset windows-gcc-debug
   cmake --build --preset windows-gcc-debug -j8

**Clang**

.. code-block:: bash

   # Ensure Clang is in PATH
   cmake --preset windows-clang-debug
   cmake --build --preset windows-clang-debug -j8

Linux
^^^^^

**GCC**

.. code-block:: bash

   # Debug
   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug -j$(nproc)

   # Release
   cmake --preset linux-gcc-release
   cmake --build --preset linux-gcc-release -j$(nproc)

   # Coverage
   cmake --preset linux-gcc-coverage
   cmake --build --preset linux-gcc-coverage -j$(nproc)

**Clang**

.. code-block:: bash

   # Debug
   cmake --preset linux-clang-debug
   cmake --build --preset linux-clang-debug -j$(nproc)

   # Release
   cmake --preset linux-clang-release
   cmake --build --preset linux-clang-release -j$(nproc)

macOS
^^^^^

.. code-block:: bash

   # Debug
   cmake --preset macos-clang-debug
   cmake --build --preset macos-clang-debug -j$(sysctl -n hw.ncpu)

   # Release
   cmake --preset macos-clang-release
   cmake --build --preset macos-clang-release -j$(sysctl -n hw.ncpu)

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

Expected output:

.. code-block:: text

   arm-none-eabi-gcc (GNU Arm Embedded Toolchain 10.3-2021.10) 10.3.1 20210824
   Copyright (C) 2020 Free Software Foundation, Inc.

STM32F4 Build
^^^^^^^^^^^^^

Using presets (recommended):

.. code-block:: bash

   # Configure
   cmake --preset cross-arm-debug

   # Build
   cmake --build --preset cross-arm-debug

   # Or use build script
   python scripts/building/build.py --preset cross-arm-debug

Using manual configuration:

.. code-block:: bash

   mkdir build-stm32f4
   cd build-stm32f4

   cmake -DNEXUS_PLATFORM=stm32f4 \
         -DCMAKE_BUILD_TYPE=Release \
         -DNEXUS_BUILD_TESTS=OFF \
         -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/arm-none-eabi.cmake \
         ..

   cmake --build . -j8

STM32H7 Build
^^^^^^^^^^^^^

.. code-block:: bash

   mkdir build-stm32h7
   cd build-stm32h7

   cmake -DNEXUS_PLATFORM=stm32h7 \
         -DCMAKE_BUILD_TYPE=Release \
         -DNEXUS_BUILD_TESTS=OFF \
         -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/arm-none-eabi.cmake \
         ..

   cmake --build . -j8

Build Output
^^^^^^^^^^^^

For embedded targets, the build generates:

.. code-block:: text

   build/cross-arm-debug/
   ├── bin/
   │   ├── blinky.elf          # Executable with debug symbols
   │   ├── blinky.bin          # Raw binary for flashing
   │   ├── blinky.hex          # Intel HEX format
   │   └── blinky.map          # Memory map file
   └── lib/
       ├── libnexus_hal.a
       ├── libnexus_osal.a
       └── ...

Memory usage is displayed during linking:

.. code-block:: text

   Memory region         Used Size  Region Size  %age Used
              FLASH:       45632 B       512 KB      8.70%
                RAM:       12288 B       128 KB      9.38%

Toolchain Configuration
~~~~~~~~~~~~~~~~~~~~~~~

The ARM toolchain file (``cmake/toolchains/arm-none-eabi.cmake``) configures:

**Compiler Tools**

.. code-block:: cmake

   set(CMAKE_C_COMPILER arm-none-eabi-gcc)
   set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
   set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
   set(CMAKE_AR arm-none-eabi-ar)
   set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
   set(CMAKE_OBJDUMP arm-none-eabi-objdump)
   set(CMAKE_SIZE arm-none-eabi-size)

**Compiler Flags for Cortex-M4**

.. code-block:: cmake

   set(CMAKE_C_FLAGS_INIT
       "-mcpu=cortex-m4 \
        -mthumb \
        -mfloat-abi=hard \
        -mfpu=fpv4-sp-d16 \
        -ffunction-sections \
        -fdata-sections"
   )

**Linker Flags**

.. code-block:: cmake

   set(CMAKE_EXE_LINKER_FLAGS_INIT
       "-Wl,--gc-sections \
        -Wl,--print-memory-usage \
        -specs=nano.specs \
        -specs=nosys.specs"
   )

Custom Toolchain Path
^^^^^^^^^^^^^^^^^^^^^

If the toolchain is not in PATH:

.. code-block:: bash

   cmake --preset cross-arm-debug \
         -DTOOLCHAIN_PREFIX=/path/to/gcc-arm-none-eabi

Or set environment variable:

.. code-block:: bash

   export ARM_TOOLCHAIN_PATH=/path/to/gcc-arm-none-eabi
   cmake --preset cross-arm-debug

Flashing
^^^^^^^^

After building, flash the binary to your device:

**Using OpenOCD**

.. code-block:: bash

   openocd -f interface/stlink.cfg \
           -f target/stm32f4x.cfg \
           -c "program build/cross-arm-debug/bin/blinky.elf verify reset exit"

**Using ST-Link Utility**

.. code-block:: bash

   st-flash write build/cross-arm-debug/bin/blinky.bin 0x8000000

**Using J-Link**

.. code-block:: bash

   JLinkExe -device STM32F407VG -if SWD -speed 4000 \
            -CommanderScript flash.jlink

Where ``flash.jlink`` contains:

.. code-block:: text

   loadfile build/cross-arm-debug/bin/blinky.hex
   r
   g
   exit

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

Maximize build speed with parallel compilation:

.. code-block:: bash

   # Auto-detect CPU count (Linux/macOS)
   cmake --build --preset linux-gcc-release -j$(nproc)

   # Auto-detect CPU count (macOS)
   cmake --build --preset macos-clang-release -j$(sysctl -n hw.ncpu)

   # Specify job count
   cmake --build --preset windows-msvc-debug -j8

   # Use all available cores
   cmake --build --preset linux-gcc-debug -j

Build script automatically detects CPU count:

.. code-block:: bash

   # Auto-parallel build
   python scripts/building/build.py -p linux-gcc-release

   # Override with specific job count
   python scripts/building/build.py -p windows-msvc-debug -j4

Verbose Build
~~~~~~~~~~~~~

Show full compiler commands for debugging:

.. code-block:: bash

   # CMake verbose mode
   cmake --build --preset windows-msvc-debug --verbose

   # Or set environment variable
   export VERBOSE=1
   cmake --build --preset linux-gcc-debug

   # For Ninja generator
   cmake --build --preset linux-gcc-debug -- -v

   # For Make generator
   cmake --build --preset linux-gcc-debug -- VERBOSE=1

Clean Build
~~~~~~~~~~~

Remove build artifacts:

.. code-block:: bash

   # Clean specific preset build
   cmake --build --preset windows-msvc-debug --target clean

   # Or delete entire build directory
   rm -rf build/windows-msvc-debug

   # Clean all build directories
   rm -rf build/

   # Using build script with clean flag
   python scripts/building/build.py -p linux-gcc-debug --clean

Incremental Builds
~~~~~~~~~~~~~~~~~~

CMake automatically performs incremental builds:

.. code-block:: bash

   # First build (full)
   cmake --build --preset windows-msvc-debug

   # Modify source file
   # ...

   # Second build (incremental - only changed files)
   cmake --build --preset windows-msvc-debug

Force rebuild:

.. code-block:: bash

   # Rebuild everything
   cmake --build --preset windows-msvc-debug --clean-first

Install
~~~~~~~

Install built artifacts to a specific location:

.. code-block:: bash

   # Default install location (/usr/local on Unix)
   cmake --build --preset linux-gcc-release --target install

   # Custom install prefix
   cmake --preset linux-gcc-release \
         -DCMAKE_INSTALL_PREFIX=/opt/nexus
   cmake --build --preset linux-gcc-release --target install

Install layout:

.. code-block:: text

   /opt/nexus/
   ├── include/
   │   ├── nexus/
   │   │   ├── hal/
   │   │   ├── osal/
   │   │   └── framework/
   │   └── nexus_config.h
   ├── lib/
   │   ├── libnexus_hal.a
   │   ├── libnexus_osal.a
   │   └── ...
   └── share/
       └── nexus/
           └── cmake/

Compilation Database
~~~~~~~~~~~~~~~~~~~~

Generate ``compile_commands.json`` for IDE integration:

.. code-block:: bash

   # Automatically generated with presets
   cmake --preset windows-msvc-debug

   # Or manually enable
   cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

The compilation database is used by:

* **clangd**: Language server for VS Code, Vim, Emacs
* **clang-tidy**: Static analysis tool
* **cppcheck**: Static analysis tool
* **IDE tools**: Code navigation and completion

Custom Build Targets
~~~~~~~~~~~~~~~~~~~~

The build system provides several custom targets:

.. code-block:: bash

   # Build documentation
   cmake --build --preset linux-gcc-debug --target docs

   # Run clang-format
   cmake --build --preset linux-gcc-debug --target format

   # Run clang-tidy
   cmake --build --preset linux-gcc-debug --target tidy

   # Generate coverage report
   cmake --build --preset linux-gcc-coverage --target coverage

Build Directory Structure
~~~~~~~~~~~~~~~~~~~~~~~~~

Each preset creates its own build directory:

.. code-block:: text

   build/
   ├── windows-msvc-debug/         # MSVC Debug build
   │   ├── CMakeCache.txt
   │   ├── compile_commands.json
   │   ├── bin/                    # Executables
   │   │   ├── blinky.exe
   │   │   └── shell_demo.exe
   │   ├── lib/                    # Libraries
   │   │   ├── nexus_hal.lib
   │   │   └── nexus_osal.lib
   │   └── tests/                  # Test executables
   │       └── nexus_tests.exe
   ├── linux-gcc-release/          # GCC Release build
   │   ├── bin/
   │   ├── lib/
   │   └── tests/
   └── cross-arm-debug/            # ARM Debug build
       ├── bin/
       │   ├── blinky.elf
       │   ├── blinky.bin
       │   ├── blinky.hex
       │   └── blinky.map
       └── lib/

Output Artifacts
~~~~~~~~~~~~~~~~

**Native Platform**

* ``.exe`` (Windows) or no extension (Unix) - Executable
* ``.lib`` (MSVC) or ``.a`` (GCC/Clang) - Static library
* ``.dll`` (Windows) or ``.so`` (Unix) - Shared library (if enabled)

**Embedded Platform**

* ``.elf`` - Executable with debug symbols
* ``.bin`` - Raw binary for flashing
* ``.hex`` - Intel HEX format
* ``.map`` - Memory map file
* ``.lst`` - Assembly listing (if enabled)

Build Performance Tips
~~~~~~~~~~~~~~~~~~~~~~

**Optimize Build Speed**

1. **Use Ninja generator**: Faster than Make or Visual Studio

   .. code-block:: bash

      # Ninja is used by default in presets
      cmake --preset linux-gcc-debug

2. **Enable parallel builds**: Use all CPU cores

   .. code-block:: bash

      cmake --build --preset linux-gcc-debug -j$(nproc)

3. **Use ccache**: Cache compilation results

   .. code-block:: bash

      # Install ccache
      sudo apt-get install ccache  # Linux
      brew install ccache          # macOS

      # Configure CMake to use ccache
      cmake --preset linux-gcc-debug \
            -DCMAKE_C_COMPILER_LAUNCHER=ccache \
            -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

4. **Incremental builds**: Only rebuild changed files

   .. code-block:: bash

      # CMake automatically does incremental builds
      cmake --build --preset linux-gcc-debug

5. **Precompiled headers**: Reduce compilation time (future feature)

**Reduce Build Size**

1. **Use MinSizeRel build type**:

   .. code-block:: bash

      cmake -DCMAKE_BUILD_TYPE=MinSizeRel ..

2. **Enable LTO (Link Time Optimization)**:

   .. code-block:: bash

      cmake -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ..

3. **Strip debug symbols**:

   .. code-block:: bash

      arm-none-eabi-strip blinky.elf -o blinky_stripped.elf

Code Coverage
-------------

The Nexus build system includes comprehensive code coverage support for quality assurance.

Using Coverage Preset (Recommended)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The easiest way to generate coverage reports:

.. code-block:: bash

   # Configure with coverage preset
   cmake --preset linux-gcc-coverage

   # Build
   cmake --build --preset linux-gcc-coverage -j8

   # Run tests
   ctest --preset linux-gcc-coverage

   # Generate coverage report
   bash scripts/coverage/run_coverage_linux.sh

Or use the build script:

.. code-block:: bash

   # Build and test with coverage
   python scripts/building/build.py -p linux-gcc-coverage -t

Manual Coverage Setup
~~~~~~~~~~~~~~~~~~~~~

Linux/macOS with GCC
^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   # 1. Configure with coverage flags
   cmake -DNEXUS_ENABLE_COVERAGE=ON \
         -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_C_FLAGS="--coverage" \
         -DCMAKE_CXX_FLAGS="--coverage" \
         ..

   # 2. Build
   cmake --build . -j8

   # 3. Run tests
   ctest --output-on-failure

   # 4. Generate coverage data
   lcov --capture --directory . --output-file coverage.info

   # 5. Filter out system and test files
   lcov --remove coverage.info \
        '/usr/*' \
        '*/ext/*' \
        '*/tests/*' \
        '*/googletest/*' \
        --output-file coverage_filtered.info

   # 6. Generate HTML report
   genhtml coverage_filtered.info \
           --output-directory coverage_html \
           --title "Nexus Coverage Report" \
           --legend

   # 7. View report
   xdg-open coverage_html/index.html  # Linux
   open coverage_html/index.html      # macOS

Windows with MSVC
^^^^^^^^^^^^^^^^^

Use OpenCppCoverage:

.. code-block:: powershell

   # Install OpenCppCoverage
   choco install opencppcoverage

   # Run tests with coverage
   OpenCppCoverage.exe `
       --sources nexus\* `
       --excluded_sources nexus\ext\* `
       --excluded_sources nexus\tests\* `
       --export_type html:coverage_html `
       -- ctest --preset windows-msvc-debug -C Debug

   # View report
   start coverage_html\index.html

Coverage Scripts
~~~~~~~~~~~~~~~~

The project includes helper scripts for coverage:

**Linux**

.. code-block:: bash

   # Run coverage and generate report
   bash scripts/coverage/run_coverage_linux.sh

   # Clean coverage data
   bash scripts/coverage/clean_coverage.sh

**Windows**

.. code-block:: powershell

   # Run coverage with OpenCppCoverage
   .\scripts\coverage\run_coverage_windows.ps1

Coverage Metrics
~~~~~~~~~~~~~~~~

The coverage report includes:

* **Line Coverage**: Percentage of executed lines
* **Function Coverage**: Percentage of called functions
* **Branch Coverage**: Percentage of executed branches

Target coverage goals:

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Component
     - Target
     - Notes
   * - HAL Drivers
     - > 90%
     - Core functionality must be well-tested
   * - OSAL
     - > 85%
     - Platform-specific code may vary
   * - Framework
     - > 80%
     - Log, Shell, Config components
   * - Overall
     - > 80%
     - Project-wide coverage target

Viewing Coverage Results
~~~~~~~~~~~~~~~~~~~~~~~~

The HTML report provides:

* **Summary Page**: Overall coverage statistics
* **Directory View**: Coverage by directory
* **File View**: Line-by-line coverage
* **Function List**: Coverage per function

Example coverage summary:

.. code-block:: text

   Overall coverage rate:
     lines......: 87.3% (12456 of 14267 lines)
     functions..: 91.2% (1234 of 1353 functions)
     branches...: 78.5% (3456 of 4401 branches)

CI/CD Integration
~~~~~~~~~~~~~~~~~

Coverage in GitHub Actions:

.. code-block:: yaml

   - name: Run tests with coverage
     run: |
       cmake --preset linux-gcc-coverage
       cmake --build --preset linux-gcc-coverage
       ctest --preset linux-gcc-coverage

   - name: Generate coverage report
     run: |
       lcov --capture --directory . --output-file coverage.info
       lcov --remove coverage.info '/usr/*' '*/ext/*' '*/tests/*' \
            --output-file coverage.info

   - name: Upload to Codecov
     uses: codecov/codecov-action@v3
     with:
       files: ./coverage.info
       fail_ci_if_error: true

Continuous Integration
---------------------

GitHub Actions Workflow
~~~~~~~~~~~~~~~~~~~~~~~

The project includes a comprehensive CI/CD workflow in ``.github/workflows/build-and-test.yml``:

**Features:**

* ✅ Multi-platform testing (Windows, Linux, macOS)
* ✅ Multiple compiler support (MSVC, GCC, Clang)
* ✅ Parallel test execution
* ✅ Code coverage reporting
* ✅ Artifact uploading
* ✅ ARM cross-compilation

Workflow Structure
~~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   name: Build and Test

   on:
     push:
       branches: [ main, develop ]
     pull_request:
       branches: [ main, develop ]

   jobs:
     build-and-test:
       strategy:
         matrix:
           os: [ubuntu-latest, windows-latest, macos-latest]
           preset:
             - linux-gcc-debug
             - linux-gcc-release
             - windows-msvc-debug
             - windows-msvc-release
             - macos-clang-debug
             - macos-clang-release
         exclude:
           # Exclude invalid combinations
           - os: ubuntu-latest
             preset: windows-msvc-debug
           # ... more exclusions

       runs-on: ${{ matrix.os }}

       steps:
         - uses: actions/checkout@v3

         - name: Setup Python
           uses: actions/setup-python@v4
           with:
             python-version: '3.x'

         - name: Install dependencies
           run: |
             python -m pip install --upgrade pip
             pip install -r requirements.txt

         - name: Configure
           run: cmake --preset ${{ matrix.preset }}

         - name: Build
           run: cmake --build --preset ${{ matrix.preset }} -j4

         - name: Test
           run: ctest --preset ${{ matrix.preset }} -j4 --output-on-failure

         - name: Upload artifacts
           uses: actions/upload-artifact@v3
           with:
             name: build-${{ matrix.os }}-${{ matrix.preset }}
             path: build/${{ matrix.preset }}/bin/

Coverage Job
~~~~~~~~~~~~

Separate job for coverage reporting:

.. code-block:: yaml

   coverage:
     runs-on: ubuntu-latest

     steps:
       - uses: actions/checkout@v3

       - name: Install dependencies
         run: |
           sudo apt-get update
           sudo apt-get install -y lcov

       - name: Configure with coverage
         run: cmake --preset linux-gcc-coverage

       - name: Build
         run: cmake --build --preset linux-gcc-coverage -j4

       - name: Test
         run: ctest --preset linux-gcc-coverage -j4

       - name: Generate coverage report
         run: |
           lcov --capture --directory . --output-file coverage.info
           lcov --remove coverage.info '/usr/*' '*/ext/*' '*/tests/*' \
                --output-file coverage.info

       - name: Upload to Codecov
         uses: codecov/codecov-action@v3
         with:
           files: ./coverage.info
           fail_ci_if_error: true

Cross-Compilation Job
~~~~~~~~~~~~~~~~~~~~~

ARM cross-compilation testing:

.. code-block:: yaml

   cross-compile:
     runs-on: ubuntu-latest

     steps:
       - uses: actions/checkout@v3

       - name: Install ARM toolchain
         run: |
           sudo apt-get update
           sudo apt-get install -y gcc-arm-none-eabi

       - name: Configure
         run: cmake --preset cross-arm-release

       - name: Build
         run: cmake --build --preset cross-arm-release -j4

       - name: Check binary size
         run: |
           arm-none-eabi-size build/cross-arm-release/bin/*.elf

       - name: Upload firmware
         uses: actions/upload-artifact@v3
         with:
           name: firmware-arm
           path: |
             build/cross-arm-release/bin/*.elf
             build/cross-arm-release/bin/*.bin
             build/cross-arm-release/bin/*.hex

Local CI Testing
~~~~~~~~~~~~~~~~

Test CI workflow locally using act:

.. code-block:: bash

   # Install act
   # Linux: curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash
   # macOS: brew install act
   # Windows: choco install act-cli

   # Run all jobs
   act

   # Run specific job
   act -j build-and-test

   # Run with specific matrix
   act -j build-and-test --matrix os:ubuntu-latest --matrix preset:linux-gcc-debug

Best Practices
--------------

Development Workflow
~~~~~~~~~~~~~~~~~~~~

**Daily Development**

.. code-block:: bash

   # 1. Pull latest changes
   git pull

   # 2. Build with debug preset
   python scripts/building/build.py -p windows-msvc-debug -t

   # 3. Make changes
   # ... edit code ...

   # 4. Incremental build and test
   cmake --build --preset windows-msvc-debug -j8
   ctest --preset windows-msvc-debug --output-on-failure

   # 5. Commit changes
   git add .
   git commit -m "feat: add new feature"

**Before Pull Request**

.. code-block:: bash

   # 1. Clean build
   python scripts/building/build.py -p linux-gcc-release --clean

   # 2. Run all tests
   ctest --preset linux-gcc-release -j8

   # 3. Check coverage
   python scripts/building/build.py -p linux-gcc-coverage -t
   bash scripts/coverage/run_coverage_linux.sh

   # 4. Format code
   cmake --build --preset linux-gcc-debug --target format

   # 5. Run static analysis
   cmake --build --preset linux-gcc-debug --target tidy

Preset Selection Guidelines
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Choose the right preset for your task:**

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Task
     - Recommended Preset
   * - Daily development
     - ``*-debug`` (e.g., ``windows-msvc-debug``)
   * - Performance testing
     - ``*-release`` (e.g., ``linux-gcc-release``)
   * - Code coverage
     - ``linux-gcc-coverage``
   * - Production build
     - ``*-release`` with LTO enabled
   * - Embedded deployment
     - ``cross-arm-release``
   * - CI/CD testing
     - Multiple presets for matrix testing

Build Configuration Tips
~~~~~~~~~~~~~~~~~~~~~~~~

**1. Use Presets Consistently**

.. code-block:: bash

   # Good: Use presets
   cmake --preset windows-msvc-debug
   cmake --build --preset windows-msvc-debug
   ctest --preset windows-msvc-debug

   # Avoid: Manual configuration
   cmake -DCMAKE_BUILD_TYPE=Debug -G "Visual Studio 17 2022" ..

**2. Keep Build Directories Separate**

.. code-block:: bash

   # Good: Preset-based directories
   build/windows-msvc-debug/
   build/linux-gcc-release/

   # Avoid: Single build directory
   build/

**3. Use Build Script for Automation**

.. code-block:: bash

   # Good: Automated workflow
   python scripts/building/build.py -p linux-gcc-debug -t

   # Avoid: Manual steps
   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug
   ctest --preset linux-gcc-debug

**4. Enable Parallel Builds**

.. code-block:: bash

   # Good: Use all cores
   cmake --build --preset linux-gcc-release -j$(nproc)

   # Avoid: Single-threaded build
   cmake --build --preset linux-gcc-release

**5. Run Tests Regularly**

.. code-block:: bash

   # Good: Test after every build
   python scripts/building/build.py -p windows-msvc-debug -t

   # Avoid: Build without testing
   python scripts/building/build.py -p windows-msvc-debug

Common Workflows
~~~~~~~~~~~~~~~~

**Quick Test**

.. code-block:: bash

   # Build and test in one command
   python scripts/building/build.py -p windows-msvc-debug -t

**Release Build**

.. code-block:: bash

   # Clean release build
   python scripts/building/build.py -p linux-gcc-release --clean -j8

**Coverage Analysis**

.. code-block:: bash

   # Build with coverage and generate report
   python scripts/building/build.py -p linux-gcc-coverage -t
   bash scripts/coverage/run_coverage_linux.sh
   xdg-open coverage_html/index.html

**Cross-Compilation**

.. code-block:: bash

   # Build for ARM target
   cmake --preset cross-arm-release
   cmake --build --preset cross-arm-release -j8
   arm-none-eabi-size build/cross-arm-release/bin/*.elf

**Multi-Platform Testing**

.. code-block:: bash

   # Test on all available platforms
   for preset in windows-msvc-debug linux-gcc-debug macos-clang-debug; do
       if cmake --preset $preset 2>/dev/null; then
           cmake --build --preset $preset -j8
           ctest --preset $preset --output-on-failure
       fi
   done

IDE Integration
---------------

The build system integrates seamlessly with modern IDEs through CMake Presets.

Visual Studio Code
~~~~~~~~~~~~~~~~~~

**Setup:**

1. Install CMake Tools extension
2. Open project folder
3. Select preset from status bar (bottom)
4. Build and debug using CMake Tools interface

**Configuration** (``.vscode/settings.json``):

.. code-block:: json

   {
       "cmake.useCMakePresets": "always",
       "cmake.configureOnOpen": true,
       "cmake.buildDirectory": "${workspaceFolder}/build/${presetName}"
   }

**Keyboard Shortcuts:**

* ``Ctrl+Shift+P`` → "CMake: Select Configure Preset"
* ``F7`` → Build
* ``Shift+F5`` → Debug

Visual Studio 2022
~~~~~~~~~~~~~~~~~~~

**Setup:**

1. Open project folder (File → Open → Folder)
2. Visual Studio automatically detects ``CMakePresets.json``
3. Select preset from configuration dropdown
4. Build using standard Visual Studio interface

**Features:**

* Native CMake Presets support
* IntelliSense integration
* Integrated debugger
* Test Explorer integration

CLion
~~~~~

**Setup:**

1. Open project
2. CLion automatically detects ``CMakePresets.json``
3. Select preset from CMake profiles
4. Build and debug using standard CLion interface

**Features:**

* Full CMake Presets support
* Code navigation and refactoring
* Integrated debugger
* Test runner integration

See :doc:`ide_integration` for detailed IDE setup guides.

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**CMake version too old**

.. code-block:: text

   CMake Error: CMake 3.21 or higher is required.

**Solution:** Upgrade CMake to version 3.21 or later.

.. code-block:: bash

   # Linux
   sudo apt-get update
   sudo apt-get install cmake

   # macOS
   brew upgrade cmake

   # Windows
   # Download from https://cmake.org/download/

**Preset not found**

.. code-block:: text

   CMake Error: Could not read presets from CMakePresets.json

**Solution:** Ensure you're running CMake from the project root directory.

.. code-block:: bash

   # Check current directory
   pwd
   ls CMakePresets.json

   # If not in project root, navigate there
   cd /path/to/nexus

**Compiler not found**

.. code-block:: text

   CMake Error: Could not find compiler

**Solution:** Install the required compiler and ensure it's in your PATH.

.. code-block:: bash

   # Windows - Install Visual Studio or MinGW
   # Linux
   sudo apt-get install build-essential

   # macOS
   xcode-select --install

   # Verify compiler
   gcc --version
   clang --version

**Toolchain not found (ARM)**

.. code-block:: text

   CMake Error: Could not find ARM toolchain

**Solution:** Install ARM GCC toolchain and add to PATH.

.. code-block:: bash

   # Linux
   sudo apt-get install gcc-arm-none-eabi

   # macOS
   brew install gcc-arm-embedded

   # Windows - Download from ARM website
   # Add to PATH: C:\Program Files (x86)\GNU Arm Embedded Toolchain\bin

   # Verify
   arm-none-eabi-gcc --version

**Tests fail to discover**

.. code-block:: text

   No tests were found!!!

**Solution:** Ensure tests are enabled and platform is native.

.. code-block:: bash

   # Check CMake cache
   cmake --preset windows-msvc-debug
   grep NEXUS_BUILD_TESTS build/windows-msvc-debug/CMakeCache.txt

   # Should show: NEXUS_BUILD_TESTS:BOOL=ON

   # Rebuild if needed
   cmake --build --preset windows-msvc-debug --clean-first

**Configuration generation failed**

.. code-block:: text

   Failed to generate HAL config from .config file

**Solution:** Validate ``.config`` file or regenerate from defconfig.

.. code-block:: bash

   # Validate configuration
   python scripts/kconfig/validate_kconfig.py --config .config

   # Or use defconfig
   cp platforms/stm32/defconfig_stm32f4 .config
   python scripts/kconfig/generate_config.py

**Build fails on Windows**

.. code-block:: text

   'cmake' is not recognized as an internal or external command

**Solution:** Use Visual Studio Developer Command Prompt or install CMake.

.. code-block:: powershell

   # Option 1: Use VS Developer Command Prompt
   # Start → Visual Studio 2022 → Developer Command Prompt

   # Option 2: Install CMake and add to PATH
   # Download from https://cmake.org/download/

**Ninja not found**

.. code-block:: text

   CMake Error: Could not find Ninja

**Solution:** Install Ninja build system.

.. code-block:: bash

   # Linux
   sudo apt-get install ninja-build

   # macOS
   brew install ninja

   # Windows
   choco install ninja

   # Or use Visual Studio generator instead
   # Edit CMakePresets.json to use "Visual Studio 17 2022"

**Python not found**

.. code-block:: text

   Could NOT find Python3 (missing: Python3_EXECUTABLE)

**Solution:** Install Python 3.7 or higher.

.. code-block:: bash

   # Linux
   sudo apt-get install python3 python3-pip

   # macOS
   brew install python3

   # Windows
   # Download from https://www.python.org/downloads/

   # Verify
   python3 --version

**Out of memory during build**

.. code-block:: text

   c++: fatal error: Killed signal terminated program cc1plus

**Solution:** Reduce parallel jobs or increase system memory.

.. code-block:: bash

   # Reduce parallel jobs
   cmake --build --preset linux-gcc-debug -j2

   # Or use build script with limited jobs
   python scripts/building/build.py -p linux-gcc-debug -j2

Debugging Build Issues
~~~~~~~~~~~~~~~~~~~~~~

**Enable verbose output:**

.. code-block:: bash

   # Verbose CMake configuration
   cmake --preset windows-msvc-debug --debug-output

   # Verbose build
   cmake --build --preset windows-msvc-debug --verbose

   # Verbose tests
   ctest --preset windows-msvc-debug --verbose

**Check CMake cache:**

.. code-block:: bash

   # View all cache variables
   cmake -L build/windows-msvc-debug

   # View with help text
   cmake -LH build/windows-msvc-debug

   # Edit cache with GUI
   cmake-gui build/windows-msvc-debug

**Clean and rebuild:**

.. code-block:: bash

   # Clean build artifacts
   cmake --build --preset windows-msvc-debug --target clean

   # Or delete build directory
   rm -rf build/windows-msvc-debug

   # Reconfigure and rebuild
   cmake --preset windows-msvc-debug
   cmake --build --preset windows-msvc-debug

Getting Help
~~~~~~~~~~~~

If you encounter issues not covered here:

1. **Check documentation**: Review :doc:`../getting_started/environment_setup`
2. **Search issues**: Visit `GitHub Issues <https://github.com/X-Gen-Lab/nexus/issues>`_
3. **Ask community**: Join `GitHub Discussions <https://github.com/X-Gen-Lab/nexus/discussions>`_
4. **Report bug**: Open a new issue with:

   * CMake version: ``cmake --version``
   * Compiler version: ``gcc --version`` or ``cl``
   * Operating system and version
   * Full error message
   * Steps to reproduce

Summary
-------

The Nexus build system provides:

✅ **Modern CMake Presets** for consistent, reproducible builds
✅ **Multi-platform support** (Windows, Linux, macOS, ARM)
✅ **Comprehensive testing** with 1600+ unit tests
✅ **Code coverage** analysis for quality assurance
✅ **CI/CD integration** with GitHub Actions
✅ **IDE support** for VS Code, Visual Studio, CLion
✅ **Cross-compilation** for embedded targets
✅ **Build scripts** for simplified workflows

**Quick Reference:**

.. code-block:: bash

   # List presets
   cmake --list-presets

   # Build and test
   python scripts/building/build.py -p <preset> -t

   # Run tests
   ctest --preset <preset> -j8

   # Generate coverage
   python scripts/building/build.py -p linux-gcc-coverage -t

See Also
--------

* :doc:`kconfig` - Kconfig configuration system
* :doc:`../development/testing` - Testing guide
* :doc:`../getting_started/environment_setup` - Installation guide
* :doc:`ide_integration` - IDE integration guides
* :doc:`../platform_guides/index` - Platform-specific guides
* :doc:`../development/contributing` - Contributing guidelines



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
