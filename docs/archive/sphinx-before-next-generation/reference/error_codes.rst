Errors, Ownership and Retry
===========================

HAL error declarations are authoritative in ``hal/nx_status.h``; OSAL and common
components retain their own status types. Do not translate a failed required
operation to success or treat all errors as resource settlement.

.. list-table:: Selected HAL outcomes
   :header-rows: 1
   :widths: 40 60

   * - Status
     - Meaning and lifecycle implication
   * - ``NX_ERR_CONTEXT`` / ``NX_ERR_INVALID_STATE``
     - Forbidden ISR/mask/state admission; reject before provider side effects.
   * - ``NX_ERR_TYPE_MISMATCH`` / ``NX_ERR_PERMISSION``
     - Wrong device class or owner/generation/region authorization.
   * - ``NX_ERR_NOT_SUPPORTED``
     - Required capability is absent; no fake fallback or successful placeholder.
   * - ``NX_ERR_BUSY`` / ``NX_ERR_RESOURCE_BUSY``
     - Active owner/operation/object prevents change; preserve retryable state.
   * - ``NX_ERR_NO_RESOURCE`` / ``NX_ERR_FULL``
     - Bounded slot/token/capacity exhausted; completion FULL retains ticket/context.
   * - ``NX_ERR_TIMEOUT``
     - Original finite budget consumed; unsettled hardware storage remains borrowed.
   * - ``NX_ERR_IO`` / ``NX_ERR_HARDWARE``
     - Actual backend failure; cleanup/ownership is checked separately.
   * - ``NX_ERR_CANCELLED``
     - Terminal canceled result under the specific operation settlement contract.

Cancellation request acceptance is not universally terminal. Close/cleanup errors
preserve owners and leases. Failed hardware cleanup keeps HAL/Runtime PARTIAL
with admission fencing; retry must settle retained resources. Runtime reports
original, cleanup/rollback and restore outcomes separately. READY describes
infrastructure, not application health or product recovery.

Generated declarations
----------------------

.. doxygenfile:: nx_status.h
   :project: nexus
