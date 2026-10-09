Platform Differences
====================

Platform capability is an implemented operation, not a silicon feature list.
Query the actual provider/backend and keep board-specific wiring in the Board.
No measured cross-platform benchmark is supplied by the current delivery.

.. list-table:: Maintained differences
   :header-rows: 1
   :widths: 25 35 40

   * - Area
     - STM32F407VE/VG/ZG
     - GD32F470ZG
   * - Physical Flash
     - VE 512 KiB/8 sectors; VG/ZG 1 MiB/12 nonuniform sectors.
     - 1 MiB/256 independent 4 KiB pages.
   * - Main RAM
     - 128 KiB main SRAM; 64 KiB CCM not DMA-accessible.
     - 192 KiB main SRAM; extra SRAM/TCM not default initialization/DMA guarantee.
   * - UART
     - Reviewed interrupt UART with true wire TC/ticket settlement.
     - Independent USART0 IRQ/event/ticket and TIMER1 timestamp.
   * - SPI
     - Reviewed polling and selected Discovery DMA routes.
     - SPI4 polling with bounded reset/drain; no DMA.
   * - Startup/SDK
     - ST startup/vector and fixed CMSIS/ST HAL, private compile context.
     - Official GD32 3.3.3 startup/vector/independent register/clock implementation.

Native is a virtual host behavior model with real host synchronization, not a
physical SoC. Its legacy peripherals and typed I2C success do not establish MCU
providers, precision, interrupt/DMA timing or electrical waveforms. Baremetal and
FreeRTOS expose different resource/execution capabilities, not interchangeable
schedulers. Running/suspended MCU kernel restart remains unsupported.

Default firmware owns full physical Flash with no storage reservation. External
layout/product policy chooses partitions and maintenance windows. Typed MCU I2C,
UART DMA, ADC/PWM/CAN/Ethernet, MCU boot/crypto/vault and MPU/cache are outside
implemented support. Actual clock/safe levels/IRQ/preemption/power loss and
worst-load qualification require physical evidence; boards are deferred.
