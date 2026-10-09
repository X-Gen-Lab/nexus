Platform Guides
===============

The maintained software platforms are Native, STM32F407VE/VG/ZG and GD32F470ZG.
Exact PCB, clock/electrical, IRQ/DMA, power-loss and worst-load qualification
requires physical evidence; no boards have been connected for this delivery.

.. toctree::
   :maxdepth: 1

   native
   stm32f4
   gd32

.. list-table:: Maintained scope
   :header-rows: 1
   :widths: 25 35 40

   * - Platform
     - Software implementation
     - Qualification boundary
   * - Native
     - Host HAL/OSAL models and real FreeRTOS POSIX tests
     - Does not establish MCU electrical/real-time behavior.
   * - STM32F407VE/VG/ZG
     - Sky Youth / Discovery / Qiming V3.1, baremetal and FreeRTOS
     - Reviewed GPIO/UART/SPI and physical Flash; HIL unexecuted.
   * - GD32F470ZG
     - Liangshan Pi, baremetal and FreeRTOS, official SDK 3.3.3
     - Independent controller/clock/Flash implementation; HIL unexecuted.

Modern typed I2C is implemented on Native. MCU typed I2C and UART DMA are
unsupported. Silicon ADC/CAN/Ethernet/USB peripheral availability is not a Nexus
provider support claim. STM32H7/L4, GD32F303/F407/VF103, ESP32 and nRF52 are not
maintained selectable platforms. Adding a platform needs implementation and
source-bound consumer/contract evidence.

Current command and source identities are maintained in the root README,
``docs/strategy/support-matrix.yaml`` and external examples validation records.
