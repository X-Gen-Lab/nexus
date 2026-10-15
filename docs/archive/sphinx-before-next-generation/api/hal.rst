HAL API Reference
=================

Authoritative declarations and detailed context/lifetime contracts live in
``hal/include/hal``. This page maps maintained consumer surfaces; it does not
redeclare a guessed status enum or legacy factory API.

.. list-table:: Consumer surfaces
   :header-rows: 1
   :widths: 40 60

   * - Header / target
     - Contract
   * - ``hal/base/nx_device.h`` / ``Nexus::HALCore``
     - Opaque identity, class/capability and explicit owner/generation open/close/recover.
   * - ``Nexus::HALGPIO``
     - Typed GPIO read/write/toggle.
   * - ``Nexus::HALUART``
     - Ticket submit/poll/cancel/recover and received events with buffer settlement.
   * - ``Nexus::HALSPI`` / ``Nexus::HALI2C``
     - Parent/child references, transfer/submit/service/cancel/poll and bounded ownership.
   * - ``Nexus::HALFlash``
     - Physical geometry, block, permissioned region, read/program/erase/sync and borrow.
   * - ``hal/runtime/nx_deadline.h`` / ``Nexus::HALRuntime``
     - Explicit clock/wait port and one finite operation budget.
   * - ``hal/runtime/nx_completion.h`` / ``Nexus::HALCompletion``
     - Caller-owned finite terminal queue with explicit arm/post/dispatch.
   * - ``hal/nx_hal.h`` / ``Nexus::HALSupport``
     - HAL initialization/deinitialization and OFFLINE/PARTIAL/READY/cleanup status.

``hal/provider/nx_device_provider.h`` is an explicit implementation surface,
not a normal product consumer API. Registration/constructor binds identity/state
without hardware startup; lifecycle init occurs during typed open. Vendor SDK
headers/macros remain private. Legacy factory constructors do not establish a
second unowned public lifetime path.

Typed open/close/recover and provider dispatch require task/startup with incoming
interrupt masks unset. Rejection occurs before provider side effects and retains
the original mask. Read-only metadata and explicit ISR adapters follow their
own contract.

HAL cleanup uses exclusive admission fencing. Read-only rejection retains READY;
mutating failure retains PARTIAL for retry. Runtime releases OSAL first and does
not restore it over PARTIAL hardware. Running/suspended MCU FreeRTOS teardown
remains BUSY. Original owners may settle/close/recover retained resources.

Modern typed I2C is implemented on Native; MCU typed I2C and UART DMA are
unsupported. A declared interface or legacy host model does not imply MCU
ADC/CAN/Ethernet/USB support. See :doc:`../user_guide/hal` and the current support
matrix for actual provider scope and source-bound evidence.
