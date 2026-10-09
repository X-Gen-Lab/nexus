Checked Workers and Bounded OSAL
================================

The external
`rtos_pipeline example <https://github.com/X-Gen-Lab/nexus-examples/tree/main/apps/rtos_pipeline>`_
uses public OSAL tasks, bounded queues and synchronization. Main bootstraps common
Runtime, creates resources/workers with checked results and explicitly starts the
scheduler. Components needing blocking initialization run in a scheduled worker.

Query backend/execution capabilities. Baremetal has no task scheduler; it owns
its loop/pump. Native uses host threads; FreeRTOS uses configured static object,
stack/payload and finite identity budgets. Do not treat these resource models as
interchangeable. Slot reuse does not reuse lifetime tokens.

Workers stop new work and settle waiters/callbacks before object deletion. BUSY
retains ownership; no forced deletion contract is assumed. OSAL lifecycle cannot
hot restart a running/suspended MCU kernel. Real FreeRTOS POSIX execution proves
software kernel behavior, not ARM interrupt/timing qualification.
