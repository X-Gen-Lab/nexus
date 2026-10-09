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
     - Fixed GPIO/UART/SPI/I2C/Flash/watchdog/EXTI/PWM/ADC contracts.
   * - Optional OS
     - Explicit wait/wake and per-object static task/queue storage.
   * - Optional components
     - Narrow owner, sensor, protocol, storage and logging mechanisms.
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
