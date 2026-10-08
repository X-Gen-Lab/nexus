Build and Flash
===============

Use a named CMake preset so compiler, build mode, build directory and source
configuration stay together. Run commands from the repository root. The
maintained embedded reference is STM32F407VG on STM32F4DISCOVERY MB997; other
STM32 families and GD32 are not qualified by the presence of source files.

Prepare the checkout
--------------------

Native requires CMake 3.21+, Python 3.11+, Ninja, a C11/C++17 compiler and OpenSSL
3 development headers/libraries. STM32 additionally requires complete ARM
GCC/newlib. Configuration never downloads dependencies.

.. code-block:: bash

   python -m pip install kconfiglib==14.1.0
   git submodule update --init ext/googletest ext/freertos vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4 vendors/st/stm32f4xx_hal_driver
   cmake --list-presets

Build Linux Native
------------------

.. code-block:: bash

   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4

Executables are in ``build/linux-gcc-debug/bin``. Test suites include
``hal_native_tests``, ``osal_tests`` and ``config_tests``. CTest also registers
service and application contracts.

.. code-block:: bash

   build/linux-gcc-debug/bin/blinky --cycles 3
   build/linux-gcc-debug/bin/config_demo
   build/linux-gcc-debug/bin/shell_demo --smoke
   build/linux-gcc-debug/bin/nexus_industrial_controller

The producer/consumer demo uses a dedicated OSAL resource profile:

.. code-block:: bash

   cmake --preset native-services-debug
   cmake --build --preset native-services-debug --parallel 4
   build/native-services-debug/bin/freertos_demo --run-ms 500

Application contracts and final execution evidence are recorded in
``docs/implementation/reference-applications.md``. Native execution validates
host behavior, not MCU interrupt or electrical timing.

Build STM32F407
---------------

.. code-block:: bash

   cmake --preset stm32-armgcc-release
   cmake --build --preset stm32-armgcc-release --parallel 4
   cmake --preset stm32-armgcc-freertos-release
   cmake --build --preset stm32-armgcc-freertos-release --parallel 4

These presets select ``configs/stm32f407_baremetal_defconfig`` and
``configs/stm32f407_freertos_defconfig`` respectively. Baremetal is a main-loop
backend and does not emulate an RTOS scheduler. The reference LED is MB997 LD4,
PD12. ELF/map/bin/hex outputs use the selected build's ``bin`` directory.
Retained Windows/macOS presets are options, not execution evidence.

Configuration ownership
-----------------------

Each build owns ``generated/effective.config``, ``generated/nexus_config.h`` and
``generated/config.cmake`` from one validated Kconfig resolution. Do not edit
these files or use a source-root ``.config``. Product choices belong in an
explicit fragment and preset with a separate build directory. Unknown values,
contradictions, invalid ranges and generation errors stop configuration.

.. code-block:: bash

   python scripts/nexus_config.py validate --config configs/stm32f407_baremetal_defconfig
   python scripts/nexus_config.py info --build-dir build/linux-gcc-debug

Flash the selected board
------------------------

ARM linking and physical HIL were not executed in the current local environment.
Inspect ELF/map, effective configuration, probe identity, silicon, board revision
and supply before programming. The F407 linker reserves the final two 128 KiB
Flash sectors for configuration storage. A full-chip erase can destroy product
configuration and is not an ordinary firmware update.

For a lab MB997 board with ST-Link and an installed OpenOCD version supporting
it, this is a manual diagnostic example. Bind the intended probe in a shared
lab and substitute the exact verified ELF:

.. code-block:: bash

   openocd -f board/stm32f4discovery.cfg -c "program build/stm32-armgcc-release/bin/blinky.elf verify reset exit"

This is not a production/signing/HIL gate. Product workflows use the board/probe
lease, identity checks, readback and evidence contracts in ``scripts/hil`` and
``scripts/manufacturing``. GD32 lacks a verified SDK/board backend and fails
configuration explicitly.

See :doc:`../development/testing` and :doc:`../development/build_system`.
Detailed records are in ``docs/implementation/build-config.md`` and
``docs/implementation/platform-drivers.md``.
