Support and qualification
=========================

First-release silicon targets are STM32F407VET6/ZGT6 and GD32F470ZGT6.
Reference packages are Sky Youth, Qiming V3.1 and Liangshan. Native is an
executed host behavior model. Baremetal and pinned FreeRTOS are explicit
adapters.

.. list-table:: First-release execution modes
   :header-rows: 1

   * - Contract
     - Maintained scope
   * - GPIO
     - Authorized masks, one-register set/reset, serialized toggle.
   * - UART
     - IRQ 8N1, borrowed TX, actual TC, byte/event RX and visible loss.
   * - SPI
     - Finite-deadline short polling, up to 256 bytes, one CS interval.
   * - I2C
     - 100 kHz, bounded messages, repeated START, final one/two-byte read.
   * - Flash
     - Full physical geometry, program/erase validation, restricted regions.
   * - Watchdog
     - Independent enable/feed, irreversible effects, latched reset cause.
   * - EXTI
     - Selected static lines, shared-vector queue, visible coalescing/loss.
   * - PWM
     - Fixed timer/channel/base, zero/full duty and safe inactive stop.
   * - ADC
     - Selected single/low-rate scan, explicit sampling and valid prefix.

DMA streaming, advanced capture/break/trigger, arbitrary I2C reads, CAN, USB,
Ethernet, low-power management and secure update are deferred. Chip peripheral
availability does not imply a maintained mode or reviewed Board route.

Production-source host fault models and ARM controller link fixtures execute
software checks. Resource tools measure ELF segments, memory domains and stack
reservation. These results do not establish physical cycles, clock precision,
waves, ADC error, reset behavior or power loss.

No hardware station is connected. Physical reference-Board qualification is
unexecuted. HIL fixture/admission tools reject an incomplete station. A software
candidate additionally needs clean source, exact inputs, sealed tools, fresh
tests and independent network-disabled rebuilds; it cannot be inferred from a
workflow definition or artifact digest.
