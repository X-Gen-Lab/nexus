Static platform architecture
============================

.. list-table:: Ownership boundaries
   :header-rows: 1
   :widths: 20 80

   * - Layer
     - Responsibility
   * - Core
     - Neutral results, absolute time and caller-owned request states.
   * - Arch
     - Incoming interrupt state, barriers, exception context and cycle snapshots.
   * - SoC
     - Clock, memory facts, startup, controllers and the private SDK.
   * - Board
     - Reviewed package routes, clocks, safe levels, sources and unknowns.
   * - Typed I/O
     - Typed peripheral faces, borrowed requests and optional CAN/USB/Ethernet/timer mechanisms.
   * - Optional OS
     - Explicit wait/wake and per-object static task/queue storage.
   * - Optional components
     - Narrow owners, sensors, protocols, storage, logging, PHY and kernel-invoked GPIO admission.
   * - External consumer
     - Product composition, workers, budgets, policy and recovery.

Generated typed aliases address exactly selected provider contexts. Public device
headers expose no vendor SDK types or provider storage. The default path has no
registry, name lookup, factory discovery, forwarding-only runtime layer,
mandatory OS lock, hidden worker or heap pool.

UART admission borrows caller request and payload until acquire-observed
``SETTLED``. Rejection retains no references. The provider detaches all
references before final release publication. Cancel and timeout do not permit
premature reuse: failed drain retains ``QUARANTINED`` and recovery responsibility.

One execution owner drives service. Multi-producer sharing is an explicitly
selected bounded owner adapter, with fixed slots and epochs. Client waiting
does not take over hardware execution. Notification is a hint; the request
predicate remains authoritative. Shutdown keeps the executor running until
borrowed requests and future wake publishers have drained.

FreeRTOS and ThreadX are explicit independent backend policies. ThreadX binds the
real pinned Linux or Cortex-M4 GNU kernel port; it does not inherit every
FreeRTOS CPU, MPU or split-security capability. The product enters a scheduler
explicitly. Bootstrap reclamation is valid only in its proven prestart scope;
an attempted scheduler return retains effects and fails stop. A running kernel
continues to own its clocks and Tick after peripheral shutdown.

USB owns exact endpoints, FIFO/event storage and a 48 MHz clock consumer.
Ethernet owns exact descriptor arrays and RX loans; capture and complementary
PWM own immutable timer/IRQ/break facts. Resource conflicts are rejected before
effects. Chip routes, Board authorization, complete software composition and
physical qualification remain separate.

The optional user I/O broker handles bounded scalar GPIO calls from an
integrating privileged kernel. Its trusted memory and identity ports do not
create an implemented SVC, MPU or TrustZone userspace transport. Protocol stacks,
product recovery, safe outputs and electrical policy remain external.
