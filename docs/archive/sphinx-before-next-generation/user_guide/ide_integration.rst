IDE Integration
===============

Import checked-in ``CMakePresets.json`` and select ``linux-gcc-debug`` for the
maintained Native reference. The IDE and terminal must use the same preset.
Do not override its compiler, mode, fragment or build directory in an unrelated
editor settings file.

Visual Studio Code
------------------

Install CMake Tools and a C/C++ language/debugging extension. A workspace can use
CMake target information directly:

.. code-block:: json

   {
     "cmake.useCMakePresets": "always",
     "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
   }

Run ``CMake: Select Configure Preset``, choose ``linux-gcc-debug``, configure,
and select the matching build preset. CMake supplies generated include paths
and definitions. If using clangd, point it at
``build/linux-gcc-debug/compile_commands.json`` and change the directory when
switching presets. Installing clangd does not change the firmware compiler.

Checked-in workspace tasks use explicit presets: ``Build Native`` configures
and builds ``linux-gcc-debug``; ``Test Native`` then runs nonempty CTest.
``Build STM32 baremetal`` and ``Build STM32 FreeRTOS`` use their respective
experimental Debug presets. Configure-on-open is disabled so opening the editor
does not select or configure an unintended profile. C/C++ uses the CMake target
provider rather than a shared hard-coded compilation database.

Native GDB launch
~~~~~~~~~~~~~~~~~

This ``.vscode/launch.json`` launches the actual Config test binary after you
build it. Supply your installed GDB path when it is not on PATH:

.. code-block:: json

   {
     "version": "0.2.0",
     "configurations": [
       {
         "name": "Native config_tests",
         "type": "cppdbg",
         "request": "launch",
         "program": "${workspaceFolder}/build/linux-gcc-debug/bin/config_tests",
         "cwd": "${workspaceFolder}",
         "args": [],
         "MIMode": "gdb",
         "stopAtEntry": false
       }
     ]
   }

Other executable targets include ``hal_native_tests``, ``osal_tests``,
``blinky``, ``config_demo`` and ``nexus_industrial_controller``. Use a listed case
with ``--gtest_filter`` for a focused regression. Finite application launches
can pass ``["--cycles", "3"]`` to blinky or ``["--smoke"]`` to shell_demo.

CLion and Visual Studio
-----------------------

In CLion, enable presets and select matching configure/build presets, then an
actual executable target. In Visual Studio, open the repository as a CMake
project and choose an applicable Windows preset. Use target-derived compiler
and include settings. Windows/macOS require their own executed acceptance
before a product relies on them.

Tests and profile changes
-------------------------

.. code-block:: bash

   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error

Outputs use ``build/<preset>/bin``. There is no ``nexus_tests`` target or shared
``build/tests/Release`` layout. Each build owns one generated bundle. Do not edit
it, add a source-root ``.config``, or combine another preset's binary and header.
New product choices belong in explicit fragments and presets.

STM32 debugging
---------------

Choose ``stm32-armgcc-debug`` or ``stm32-armgcc-freertos-debug`` with complete
ARM GCC/newlib. Cortex-Debug or a vendor debugger must use the matching ELF in
that build's ``bin`` directory, a specific probe and actual STM32F407VG/MB997.
Probe, partition and reset settings are explicit product/lab choices.

ARM linking and physical HIL were not executed locally. Other STM32 families and
GD32 do not become supported by importing an IDE project. Read
:doc:`../getting_started/build_and_flash` before programming a board.

Checked-in MCU launches use OpenOCD's MB997 board configuration and the matching
``blinky.elf``. In a shared lab bind the intended probe before launch. No missing
SVD path or generic legacy build directory is supplied by the workspace.
