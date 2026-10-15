Configuration Entry Points
==========================

The authoritative symbols are the current Kconfig source tree and the selected
build's ``generated/effective.config``. This index maps ownership instead of
copying a stale all-chip/peripheral option list.

.. list-table:: Current configuration ownership
   :header-rows: 1
   :widths: 35 65

   * - Source
     - Responsibility
   * - ``platforms/Kconfig``
     - Maintained Native / STM32F407 / GD32F470 selection and thin SoC routing.
   * - ``soc/stm32f407/Kconfig``
     - Exact VE/VG/ZG density, clock/IRQ/system and implemented controllers.
   * - ``soc/gd32f470/Kconfig``
     - F470ZG clock/resources and implemented controllers.
   * - ``soc/native/Kconfig``
     - Host model resources and virtual peripheral configuration.
   * - ``boards/Kconfig``
     - One maintained Board or external manifest package.
   * - ``osal/Kconfig``
     - Native/baremetal/FreeRTOS, actual context capabilities and object budgets.
   * - ``cmake/Kconfig.firmware``
     - Generic firmware main-stack/libc-heap and artifact options.
   * - ``framework`` / ``services``
     - Explicit common component/core/adapter selection and resources.

One explicit fragment is resolved per build root. Unknown/duplicate symbols,
unsupported choices/ranges/dependencies and contradictory Board/config inputs
fail. Removed Product/application/unmaintained hardware symbols have no fallback.
No current configuration implies typed MCU I2C/UART DMA/ADC/PWM/EXTI or broader
vendor silicon support. Read :doc:`../user_guide/kconfig` and the maintained
Board/backend fragments for actual usage.
