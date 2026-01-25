Build System
============

Comprehensive guide to the Nexus build system based on CMake, covering configuration, building, and customization.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

The Nexus build system uses:

* **CMake**: Cross-platform build system generator
* **Python Scripts**: Build automation and tooling
* **Kconfig**: Configuration management
* **Ninja/Make**: Build backends

Key features:

* Multi-platform support (native, STM32, NRF52, etc.)
* Multiple build types (debug, release, minsizerel)
* Modular architecture
* Dependency management
* Testing integration
* Code coverage support

Quick Start
-----------

Basic Build
~~~~~~~~~~~

.. code-block:: bash

   # Build for native platform (default)
   python scripts/building/build.py

   # Build for specific platform
   python scripts/building/build.py --platform stm32f4

   # Build with specific type
   python scripts/building/build.py --build-type release

   # Clean build
   python scripts/building/build.py --clean

Build Options
~~~~~~~~~~~~~

.. code-block:: bash

   # Enable verbose output
   python scripts/building/build.py --verbose

   # Parallel build (4 jobs)
   python scripts/building/build.py --jobs 4

   # Build specific target
   python scripts/building/build.py --target hal_gpio

   # Generate compile_commands.json
   python scripts/building/build.py --export-compile-commands

CMake Structure
---------------

Project Layout
~~~~~~~~~~~~~~

.. code-block:: text

   nexus/
   ├── CMakeLists.txt              # Root CMake file
   ├── cmake/
   │   ├── platforms/              # Platform configurations
   │   │   ├── native.cmake
   │   │   ├── stm32f4.cmake
   │   │   └── nrf52.cmake
   │   ├── toolchains/             # Toolchain files
   │   │   ├── gcc-arm-none-eabi.cmake
   │   │   └── clang.cmake
   │   ├── modules/                # CMake modules
   │   │   ├── Coverage.cmake
   │   │   ├── Testing.cmake
   │   │   └── Documentation.cmake
   │   └── utils.cmake             # Utility functions
   ├── hal/
   │   └── CMakeLists.txt          # HAL build configuration
   ├── osal/
   │   └── CMakeLists.txt          # OSAL build configuration
   ├── framework/
   │   └── CMakeLists.txt          # Framework build configuration
   └── applications/
       └── CMakeLists.txt          # Applications build configuration

Root CMakeLists.txt
~~~~~~~~~~~~~~~~~~~

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.20)

   project(Nexus
       VERSION 1.0.0
       LANGUAGES C CXX
       DESCRIPTION "Nexus Embedded Platform"
   )

   # Set C standard
   set(CMAKE_C_STANDARD 11)
   set(CMAKE_C_STANDARD_REQUIRED ON)
   set(CMAKE_C_EXTENSIONS OFF)

   # Include CMake modules
   list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
   include(utils)

   # Platform configuration
   if(NOT DEFINED PLATFORM)
       set(PLATFORM "native" CACHE STRING "Target platform")
   endif()

   include(platforms/${PLATFORM})

   # Build configuration
   if(NOT CMAKE_BUILD_TYPE)
       set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
   endif()

   # Add subdirectories
   add_subdirectory(hal)
   add_subdirectory(osal)
   add_subdirectory(framework)
   add_subdirectory(applications)

   # Testing
   if(BUILD_TESTING)
       enable_testing()
       add_subdirectory(tests)
   endif()

