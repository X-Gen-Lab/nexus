Configuration
=============

Use a maintained CMake preset or pass one explicit ``NEXUS_CONFIG_FILE`` to an
external consumer. Each build directory owns one platform/Board/backend and one
configuration. Presets select compiler/build mode; Kconfig resolves actual
software/resource choices. Unknown/duplicate symbols, illegal choices/ranges,
dependencies and Board/density contradictions stop configuration.

.. code-block:: bash

   cmake --preset stm32-qiming-armgcc-freertos-release
   cmake --build --preset stm32-qiming-armgcc-freertos-release --parallel 4

Resolved outputs are ``generated/effective.config``, ``nexus_config.h`` and
``config.cmake``. Do not edit them or create a source-root ``.config``. Failure
must not leave an old generated bundle usable. Different Board/backend variants
use independent build roots.

Only Native, F407VE/VG/ZG and F470ZG are selectable maintained platforms. OSAL
backends are Native, baremetal and pinned FreeRTOS. One ``NEXUS_BOARD_DIR``
provides an external package with declared hashed sources/reviewed routes;
one ``NEXUS_FLASH_LAYOUT_FILE`` selects product image/regions. Default firmware
owns the whole physical Flash; image offset is zero only.

Hardware capability follows actual providers and reviewed Board bindings.
Unsupported typed MCU I2C/UART DMA/ADC/PWM/EXTI and other platforms cannot be
added by a configuration snippet. Read :doc:`../user_guide/kconfig_platforms`,
:doc:`../user_guide/kconfig_peripherals` and :doc:`../user_guide/kconfig_osal`.
