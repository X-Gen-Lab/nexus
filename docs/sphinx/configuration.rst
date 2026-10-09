One resolved configuration
===========================

``soc/<family>/soc.json`` declares exact variants, memories, clock profiles
and maintained modes. ``routes.json`` defines reviewed package pins and
alternate functions. Board ``board.json`` selects facts and records source
provenance, safe initial levels and explicit unknown PCB information.

One external assembly selects Board, OS backend, clock profile, bounded
controller configurations, child endpoints and resource budgets. Product
Flash layout is an external input. CMake remains the sole software dependency
graph; the resolver does not generate business tasks or a second build graph.

The resolver rejects duplicate or unknown keys, unsupported modes, package or
route mismatches, pin/IRQ/timebase conflicts, illegal budgets and stale inputs.
It atomically emits ``resolved.json``, input identities, configuration and
binding headers, binding C, linker script and resource budget. A rejected
reconfiguration invalidates the earlier bundle.

Kconfig, legacy defconfig and ``NEXUS_PLATFORM``/``NEXUS_OSAL_BACKEND`` cache
selection are removed. Invalid input never silently falls back to another
platform. Shutdown releases selected interfaces in reverse construction order
and retains progress and remaining effects on failure.

Silicon routes in development-only software Board fixtures demonstrate
construction and linkage. They do not qualify a connector or external device
on the three physical reference Boards.
