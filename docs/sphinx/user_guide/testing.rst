Testing Platform Contracts
==========================

Use production paths with explicit test-only models and observable invariants.
A passing target/archive, workflow definition, empty JUnit or stale report is
not execution evidence. Do not invent an unsupported factory/release API for tests.

Run Host Suites
---------------

.. code-block:: bash

   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4
   python3 -m unittest discover -s scripts/ci -p 'test_*.py'

Actual selection/counts and exit codes belong to source-bound reports. Required
execution rejects zero registrations, empty/stale/all-skipped reports, count
mismatches and nonzero exits. Scoped suites may overlap; do not sum them into a
full-platform total. Configuration changes need fresh generated bundles/artifacts.

Useful Faults
-------------

Test stale owner/generation and child/region leases, finite pool/token exhaustion,
busy/error retry, full queues, original deadline/wrap, cancellation races,
zero-ticket recovery and final buffer return. Native provider tests verify real
model capture/file behavior. MCU lifecycle tests use actual production control
paths plus register/vendor models to verify preflight, partial cleanup quarantine,
clock/resource ownership and retry; those models do not measure hardware.

Minimal consumer links check unwanted OSAL/vendor/optional dependencies and parent
output isolation. Actual ARM ELF checks startup/vector/SP/reset, strong IRQ and
registration retention, ABI/segments, physical density and Board/layout identity.
External examples run their own fixed-source application matrix rather than being
copied into the platform test tree.

Qualification Boundary
----------------------

Real FreeRTOS POSIX execution is distinct from ARM interrupt/scheduler timing.
Host vendor/register faults are not electrical/IRQ/DMA/power-loss qualification.
HIL station identities and measured budgets cannot be fabricated from templates.
The user currently defers physical boards; tooling reports remain
``hardware_verified=false``. See :doc:`../development/testing` and the current
support matrix for actual execution scopes.
