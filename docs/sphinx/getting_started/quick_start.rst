Quick Start
===========

Start with Linux Native to exercise production initialization and software
contracts without hardware. The STM32F407VG/STM32F4DISCOVERY MB997 reference is
experimental; host models do not establish physical board readiness.

Prepare dependencies
--------------------

Follow :doc:`environment_setup` for compiler, CMake, Python and OpenSSL 3.
Then create a complete checkout and initialize the required pinned submodules:

.. code-block:: bash

   git clone https://github.com/X-Gen-Lab/nexus.git
   cd nexus
   python -m pip install kconfiglib==14.1.0
   git submodule update --init ext/googletest ext/freertos vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4 vendors/st/stm32f4xx_hal_driver

Build and test Native
---------------------

.. code-block:: bash

   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4

Each build owns its generated effective configuration under
``build/<preset>/generated``. Do not edit those files or create a source-root
``.config``. Compiler and mode belong to the preset; device/resource selections
belong to its explicit Kconfig fragment. Generation failures stop configuration.

Run finite examples
-------------------

.. code-block:: bash

   build/linux-gcc-debug/bin/blinky --cycles 3
   build/linux-gcc-debug/bin/config_demo
   build/linux-gcc-debug/bin/shell_demo --smoke
   build/linux-gcc-debug/bin/nexus_industrial_controller

Blinky uses the production factory and board LED. Config demo exercises volatile
RAM values and import/export; it does not establish Flash persistence. Shell
smoke uses the Native console. The industrial controller runs the host Modbus
and supervision model; real RS485 transport and measured product deadlines need
board adapters and evidence. Application validation details are maintained in
``docs/implementation/reference-applications.md`` and
``docs/implementation/industrial-reference.md``.

For a finite producer/consumer OSAL demonstration, use its resource profile:

.. code-block:: bash

   cmake --preset native-services-debug
   cmake --build --preset native-services-debug --parallel 4
   build/native-services-debug/bin/freertos_demo --run-ms 500

Inspect actual test binaries
----------------------------

.. code-block:: bash

   build/linux-gcc-debug/bin/hal_native_tests --gtest_list_tests
   build/linux-gcc-debug/bin/osal_tests --gtest_list_tests
   build/linux-gcc-debug/bin/config_tests --gtest_list_tests

Use CTest labels and names as described in :doc:`../development/testing`.
Record results for the matching source and configuration rather than assuming a
build success or zero-test run proves runtime correctness.

Explore the experimental MCU reference
--------------------------------------

With complete ARM GCC/newlib installed:

.. code-block:: bash

   cmake --preset stm32-armgcc-debug
   cmake --build --preset stm32-armgcc-debug --parallel 4
   cmake --preset stm32-armgcc-freertos-debug
   cmake --build --preset stm32-armgcc-freertos-debug --parallel 4

Outputs are under each build's ``bin`` directory. The board LED is MB997 LD4,
PD12. Baremetal uses a main loop; it does not support task scheduling, event
barriers or software timers. The FreeRTOS profile uses the pinned kernel and
explicit port configuration.

ARM linking and physical HIL were not executed locally. Before programming,
read :doc:`build_and_flash` for board/probe identity and reserved Flash sectors.
GD32 awaits a verified SDK/board port; unsupported backends fail configuration.
Retained Windows/macOS presets require separate executed acceptance.

Read :doc:`../development/development_environment` and
:doc:`../user_guide/ide_integration` for editor setup. Architecture, support
promotion and product acceptance are defined in ``docs/strategy``.
