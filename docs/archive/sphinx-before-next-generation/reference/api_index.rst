Public API Entry Points
=======================

Public headers/targets are authoritative. This index maps current contracts
rather than inventing legacy factory release functions or copied declarations.

.. list-table:: Current surfaces
   :header-rows: 1
   :widths: 40 60

   * - Header / target
     - Responsibility
   * - ``runtime/nx_runtime.h`` / ``Nexus::Runtime``
     - Serial infrastructure bootstrap/shutdown and ownership/error states.
   * - ``runtime/nx_platform_info.h``
     - Side-effect-free selected identity/config/memory and Board/layout metadata.
   * - ``hal/base/nx_device.h`` / typed HAL facades
     - Opaque identity, explicit owner/generation and device/child/region/operation lifetimes.
   * - ``hal/nx_hal.h`` / ``Nexus::HALSupport``
     - HAL OFFLINE/PARTIAL/READY and real platform cleanup status.
   * - ``hal/runtime/nx_deadline.h`` / ``Nexus::HALRuntime``
     - Explicit clock/wait and original finite operation budget.
   * - ``hal/runtime/nx_completion.h`` / ``Nexus::HALCompletion``
     - Caller-owned terminal queue and explicit dispatch pump.
   * - ``osal/osal.h`` / ``Nexus::OSAL``
     - Actual backend/execution capabilities, task/sync/time/resource contracts.
   * - Log / Shell / Config public headers
     - Common cores and explicit adapters, without product tasks or commands.
   * - Storage / Security / Modbus / Update public headers
     - Narrow persistence/crypto/protocol/state ports and caller-owned policies.

``Nexus::Firmware`` explicitly assembles selected startup/SoC/Board objects,
Runtime and typed facades. Ordinary consumer interfaces keep vendor SDK and
mutable provider state private. Raw SDK opt-in and provider implementation
headers have separate responsibilities and do not replace typed ownership.

See :doc:`../api/hal`, :doc:`../api/osal`, :doc:`../api/log`,
:doc:`../api/shell`, :doc:`../api/config` and :doc:`../api/init`. The support matrix
records actual implementation/evidence scope; declaration alone is not hardware
capability or qualification.
