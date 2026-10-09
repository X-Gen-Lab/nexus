First External Application
==========================

Create applications in an external repository. Nexus owns common platform
contracts and provides no business main, task creation, mandatory application
choice or product partition. The maintained
`nexus-examples <https://github.com/X-Gen-Lab/nexus-examples>`_ repository contains
actual public-API consumers with fixed Gitlink/lock and source-bound execution.

Consumer CMake
--------------

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.21)
   project(my_firmware LANGUAGES C CXX ASM)
   set(NEXUS_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/platform.conf" CACHE FILEPATH "")
   add_subdirectory(external/nexus nexus)
   nexus_add_application(TARGET firmware SOURCES main.c)

Copy a matching maintained configuration fragment into your application and
choose toolchain before ``project``. Each build root owns one Board/backend.
For ARM pass ``CMAKE_TOOLCHAIN_FILE`` and matching ``NEXUS_PLATFORM`` on initial
configure. Optional components are explicit target dependencies. Vendor SDK
headers/macros stay private; raw SDK bring-up is an explicit opt-in.

Lifecycle and Devices
---------------------

External main calls ``nx_runtime_bootstrap`` and checks its report. READY means
HAL/OSAL ownership, not product health. Use typed ``nx_device_open`` with explicit
owner, query capabilities and operate through Nexus references. Real Board LED
name/polarity comes from the selected Board. The ``boot_blinky`` example provides
complete Native/baremetal/FreeRTOS branches using that public contract.

Baremetal owns its loop/pump. FreeRTOS main creates a startup worker, checks the
result and calls ``osal_start``; blocking component work runs in the scheduled
worker. The ``rtos_pipeline`` example demonstrates bounded queues and synchronized
workers. Application teardown first settles devices/components/OSAL objects;
BUSY or cleanup error preserves ownership for retry. Running MCU kernel restart
is unsupported.

Additional Components
---------------------

``shell_console``, ``config_roundtrip``, ``log_uart`` and ``spi_transaction`` show
explicit adapters and budgets. Config persistence requires an external layout,
real typed Flash region and StorageHAL binding; defaults do not reserve storage.
Native models or RAM roundtrips are not Flash power-loss evidence. Modern typed
MCU I2C, ADC/CAN/Ethernet and safety/security bootchain are not available by
adding tutorial configuration.

A relocated source SDK can replace ``add_subdirectory`` with exact-version
``find_package(Nexus 0.1.0 EXACT CONFIG REQUIRED)``. Read ``cmake/package/README.md``
for preparation, expected revision, relocation and verified consumption.
