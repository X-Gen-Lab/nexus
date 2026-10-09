Environment Setup
=================

Linux Native with GCC is the maintained host reference. The embedded reference
is STM32F407VG on STM32F4DISCOVERY MB997 with baremetal or FreeRTOS. STM32F407 is
experimental, GD32 lacks a verified SDK/board backend, and Windows/macOS presets
have not been executed in this maintenance run.

Tool prerequisites
------------------

.. list-table:: Build tools
   :header-rows: 1
   :widths: 25 25 50

   * - Tool
     - Required baseline
     - Purpose
   * - CMake
     - 3.21+
     - Preset configuration and target graph
   * - Python
     - 3.11+
     - Configuration and engineering tools
   * - kconfiglib
     - 14.1.0
     - Validated effective configuration
   * - GCC or another selected compiler
     - C11 and C++17
     - Native source and test compilation
   * - OpenSSL development package
     - 3.x
     - Maintained Native security provider
   * - Git and Ninja
     - Installed on PATH
     - Pinned checkout and build execution
   * - ARM GCC with newlib
     - Complete installation
     - Experimental STM32 reference only

Linux setup
-----------

A Debian/Ubuntu setup uses distribution packages and a dedicated Python
environment. Use your organization's approved package sources/toolchains.

.. code-block:: bash

   sudo apt-get install git cmake ninja-build gcc g++ python3 python3-venv libssl-dev
   python3 -m venv .venv
   . .venv/bin/activate
   python -m pip install kconfiglib==14.1.0

Verify that installed versions meet the baseline:

.. code-block:: bash

   git --version
   cmake --version
   ninja --version
   python --version
   gcc --version
   g++ --version
   openssl version

The OpenSSL command's presence does not replace development headers/libraries.
CMake verifies the actual provider dependency when configuring Native.

Checkout and dependencies
-------------------------

.. code-block:: bash

   git clone https://github.com/X-Gen-Lab/nexus.git
   cd nexus
   git submodule update --init ext/googletest ext/freertos vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4 vendors/st/stm32f4xx_hal_driver
   git submodule status ext/googletest ext/freertos vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4 vendors/st/stm32f4xx_hal_driver
   cmake --list-presets

A source snapshot with missing files is not a complete checkout. Configuration
uses pinned local submodules and never downloads another Google Test or vendor
version automatically. Network access is needed to obtain packages/submodules,
not to re-resolve a different dependency during CMake configuration.

Verify the Native baseline
--------------------------

.. code-block:: bash

   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4

The preset owns ``build/linux-gcc-debug`` and its generated configuration bundle.
Do not consume a source-root ``.config`` or copy another target's generated
header. See :doc:`quick_start` for finite application commands and
:doc:`build_and_flash` for configuration and output paths.

ARM toolchain and probes
------------------------

Install a complete approved ARM GCC/newlib distribution and verify
``arm-none-eabi-gcc`` is on PATH. A compiler without matching assembler/linker,
C libraries and multilib support is insufficient. Configure/build the named
STM32F407 preset to establish actual toolchain compatibility; generating an ARM
configuration or compiling against SDK headers alone is not a linked image.

OpenOCD or the intended ST-Link/J-Link tools are optional for physical debug,
but the board profile, probe identity, supply and Flash partition must match.
ARM linking and physical HIL were not executed locally. Installation of a probe
tool does not constitute HIL, production signing or manufacturing qualification.
GD32 cannot yet be selected as a working board backend.

Other hosts and editors
-----------------------

Use ``cmake --list-presets`` for applicable Windows/macOS choices and select a
matching complete compiler/provider environment. Native file-flash is a POSIX
model and is excluded on Windows. Test results and tool support must be recorded
for each host before it is promoted for product use.

VS Code/CLion should import ``CMakePresets.json`` and obtain definitions/include
paths from targets. See :doc:`../user_guide/ide_integration`. Documentation needs
additional Sphinx/Doxygen tools under ``docs/sphinx``; they are separate from
firmware runtime dependencies.
