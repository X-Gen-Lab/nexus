Support and qualification
=========================

First-release silicon targets are STM32F407VET6/ZGT6 and GD32F470ZGT6.
Reference packages are Sky Youth, Qiming V3.1 and Liangshan. Native is an
executed host behavior model. Baremetal, pinned FreeRTOS and actual pinned
ThreadX are explicit adapters. ThreadX currently maintains Native Linux and
Cortex-M4/M4F on these three Boards; other ThreadX CPU ports reject. The wider
Arch/FreeRTOS CPU matrix does not enlarge that backend scope.

.. list-table:: First-release execution modes
   :header-rows: 1

   * - Contract
     - Maintained scope
   * - GPIO
     - Authorized masks, one-register set/reset, serialized toggle.
   * - UART
     - IRQ and finite TX DMA, IRQ/finite/continuous RX blocks, explicit duplex/flow control; actual TC and visible loss.
   * - SPI
     - Short polling and finite full-duplex DMA, exact endpoint CS interval, fixed-address dummy/discard options.
   * - I2C
     - 100 kHz bounded polling/IRQ/DMA messages, repeated START and final ACK/STOP drain; explicit GPIO bus-clear lease.
   * - Flash
     - Full physical geometry, program/erase validation, restricted regions.
   * - Watchdog
     - Independent enable/feed, irreversible effects, latched reset cause.
   * - EXTI
     - Selected static lines, shared-vector queue, visible coalescing/loss.
   * - PWM
     - Fixed timer/channel/base, zero/full duty and safe inactive stop.
   * - ADC
     - Single/low-rate scan and finite/continuous triggered DMA blocks, explicit sample time and loan/loss contract.
   * - Classic CAN
     - Three hardware mailboxes, shared filter domain and explicit controller recovery; no CAN FD or implicit replay.
   * - USB FS device
     - Internal-PHY PIO control/bulk packet loans, independent 48 MHz owner and full stop; no Host/HS/DMA/class stack.

Raw RMII Ethernet/MDIO/DP83848, advanced capture/complementary PWM, the
kernel-invoked GPIO broker and the initial G474 clock/GPIO/LPUART vertical slice
have separate integration and qualification exits. A scalar broker does not
implement a userspace SVC/MPU/TrustZone transport. USB/Ethernet software Board
fixtures do not establish connector, VBUS, PHY/reset or electrical routes.
FreeRTOS's reviewed optional shallow timebase is not dynamic reclocking, deep
Sleep or STOP support. Secure update and all undeclared modes remain outside
this scope. Chip peripheral availability does not imply a maintained mode or
reviewed Board route.

Production-source host fault models and ARM controller link fixtures execute
software checks. Resource tools measure ELF segments, memory domains and stack
reservation. These results do not establish physical cycles, clock precision,
waves, ADC error, reset behavior or power loss.

No hardware station is connected. Physical reference-Board qualification is
unexecuted. HIL fixture/admission tools reject an incomplete station. A software
candidate additionally needs clean source, exact inputs, sealed tools, fresh
tests and independent network-disabled rebuilds; it cannot be inferred from a
workflow definition or artifact digest.
