Development Environment
=======================

Linux Native with GCC is the maintained host reference. Use a complete checkout;
a source snapshot or missing submodule is not a buildable repository.
Windows/macOS presets were not executed in this maintenance run.

Required tools
--------------

* Git, CMake 3.21+, Ninja and Python 3.11+.
* A C11/C++17 compiler and OpenSSL 3 development headers/libraries for Native.
* ``kconfiglib==14.1.0`` for effective configuration.
* Complete ARM GCC/newlib for STM32F407 presets.

On Debian/Ubuntu, install the corresponding packages, then prepare dependencies:

.. code-block:: bash

   sudo apt-get install git cmake ninja-build gcc g++ python3 python3-venv libssl-dev
   python3 -m venv .venv
   . .venv/bin/activate
   python -m pip install kconfiglib==14.1.0
   git submodule update --init ext/googletest ext/freertos vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4 vendors/st/stm32f4xx_hal_driver

Use approved toolchain packages for ARM GCC. Verify the compiler and libraries;
a compiler executable alone does not establish complete linker/newlib support.

First build
-----------

.. code-block:: bash

   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4

The directory is ``build/linux-gcc-debug``. Generated header, CMake values and
resolved Kconfig belong to this directory. Do not copy headers between presets
or reintroduce a source-root ``.config``. Compiler/mode come from presets;
explicit fragments select devices and bounded resource capacities.

Editor and debugger
-------------------

Import ``CMakePresets.json`` with VS Code CMake Tools or CLion preset support.
Use matching configure/build/test presets. Point analysis at that build's
``compile_commands.json`` and obtain generated include/define settings from
CMake, not a manual list. See :doc:`../user_guide/ide_integration`.

Native executables are in ``build/<preset>/bin``. Debug actual targets such as
``osal_tests`` or ``config_tests``. For STM32, use the exact verified ELF, intended
probe, board and silicon. Debugger configuration does not replace qualification
or release identity checks.

Engineering tools
-----------------

.. code-block:: bash

   python -m unittest discover -s scripts/ci -p 'test_*.py'

Analyzer versions/commands are in the quality workflow and
``scripts/ci/quality_tools.py``. Use a real compilation database and inspect both
findings and tool failures. Formatting, static analysis, sanitizers and physical
HIL provide different evidence; none alone establishes MISRA compliance,
production security or measured control deadlines.

Documentation additionally needs Doxygen, Sphinx and dependencies under
``docs/sphinx``. Inspect warnings and outdated claims rather than treating
output-file generation as correctness evidence.

Troubleshooting
---------------

Inspect the selected preset, source fragment and generator error on configure
failure. Correct unknown/conflicting values before compiling. Use a separate
preset directory when compiler or mode changes. Initialize a missing pinned
submodule instead of fetching an arbitrary current release.

GD32F470ZG Liangshan has an independent official SDK/startup/controller port; unmaintained
backends fail explicitly. STM32F407/MB997 remains experimental, with ARM linking
and physical HIL separate from host validation.
