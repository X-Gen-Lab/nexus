System Architecture
===================

Nexus provides the common embedded platform. Applications and product policy
are composed in external repositories through public Nexus types and targets.
The maintained implementations are Native host models, STM32F407VE/VG/ZG and
GD32F470ZG. Other silicon families are not selectable maintained platforms.

Responsibilities
----------------

.. list-table:: Implemented ownership boundaries
   :header-rows: 1
   :widths: 20 80

   * - Layer
     - Responsibility
   * - Arch
     - CPU-local saved interrupt state, exception/mask queries and barriers.
   * - SoC
     - Controllers, clock/IRQ/time, chip identity, physical Flash, private SDK context.
   * - Board
     - Reviewed HSE, pin/AF, IRQ/DMA routes, CS and initial safe levels.
   * - HAL
     - Opaque device identity/class/capability, owner/generation and typed operation lifetimes.
   * - OSAL
     - Explicit Native/baremetal/FreeRTOS capabilities, synchronization/time and bounded MCU pools.
   * - Runtime
     - Serial HAL then OSAL initialization, ownership/error reporting and bounded release.
   * - Components
     - Reusable cores and explicit adapters through narrow ports.
   * - External application
     - Main, workers/scheduler, product partitions, budgets, health and recovery policy.

Directories and Targets
-----------------------

``soc/stm32f407/`` and ``soc/gd32f470/`` own MCU controllers and private SDK
assembly. ``soc/native/`` owns virtual host controllers/resources, without
claiming physical silicon. ``platforms/`` owns startup/lifecycle/assembly.
The common ``nexus_forward_component_objects()`` helper forwards selected
SoC/Board objects so registration, startup and strong IRQ definitions reach
the firmware. SDK include paths/macros stay private to implementation targets.

``Nexus::HALCore`` is independent of OSAL and vendor SDK. GPIO/UART/SPI/I2C/Flash
facades are separate targets. ``Nexus::Runtime`` owns infrastructure lifecycle;
``Nexus::Firmware`` explicitly assembles that runtime, typed facades and platform
objects. Optional components remain explicit dependencies.

Lifetime and Time
-----------------

Discovery/query does not initialize hardware. Open acquires explicit ownership;
close failure or BUSY preserves a retryable reference. Generation rejects stale
owners after successful close/reopen. Controller, child, region and operation
leases have separate lifetimes. Timeout/cancel does not return an unsettled
buffer. Finite operation budgets start before admission and include queueing
and waits. Caller-owned completion queues dispatch settled terminal callbacks
through an explicit application pump; no default worker is created.

Runtime READY means owned HAL/OSAL infrastructure, not product health. External
callers must settle devices/components/OSAL objects before shutdown. Idle
baremetal/pre-scheduler MCU cleanup is bounded. Read-only rejection retains
READY; mutating cleanup failure retains PARTIAL and an admission fence for
retry. Original owners may settle/recover, while new admission is rejected.
Runtime restores OSAL only while HAL remains READY. Running/suspended MCU
FreeRTOS kernel remains BUSY; full kernel restart is unsupported. Direct SDK
users and products first settle their own resources and safe outputs. Arch
locks protect short metadata only, not vendor calls or callbacks.

Configuration and Evidence
--------------------------

One per-build Kconfig parse creates effective configuration, C header and CMake
outputs. One Board package validates declared inputs and reviewed GPIO/UART/SPI
routes. One consumer-owned Flash layout drives linker and typed regions. Default
firmware owns the full physical Flash with no storage reservation. Nonzero
image offset is rejected.

Native typed I2C is implemented; MCU typed I2C and UART DMA are unsupported.
CAN/Ethernet, boot/trust chain, MCU crypto/entropy/vault, MPU/cache, full memory
domain/DMA maps and arbitrary Board route solving are not implemented or
qualified. Legacy host models or vendor peripherals do not change that scope.

Software execution and physical qualification have separate evidence. The user
has deferred boards: no probe, flash, serial, waveform, power-cut or long-load
qualification has been executed. Read the
`support matrix <https://github.com/X-Gen-Lab/nexus/blob/codex/industrial-platform-modernization/docs/strategy/support-matrix.yaml>`_
for exact source-bound checks, known limits and current delivery status.
