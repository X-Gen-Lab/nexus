Project Structure
=================

Nexus owns reusable platform code. Product applications live in the separate
`nexus-examples repository <https://github.com/X-Gen-Lab/nexus-examples>`_ or
private product repositories.

.. code-block:: text

   nexus/
   ├── arch/                 # Native/Cortex-M4 CPU primitives
   ├── hal/                  # Opaque device core, typed facades, provider contracts
   ├── osal/                 # Native, baremetal, pinned FreeRTOS backends
   ├── soc/
   │   ├── native/           # Host virtual controllers/resources, not physical silicon
   │   ├── stm32f407/        # VE/VG/ZG controllers, clock/IRQ/system, private SDK, Flash
   │   └── gd32f470/         # F470ZG controllers, clock/IRQ, private SDK, Flash
   ├── boards/               # Reviewed wiring/HSE/safe levels and resource manifests
   ├── platforms/            # Startup, platform lifecycle, final object assembly
   ├── runtime/              # Serial HAL/OSAL ownership bootstrap/shutdown
   ├── framework/            # Reusable component cores and explicit adapters
   ├── services/             # Storage/security/protocol/update ports and cores
   ├── cmake/                # Configuration, Board/layout, object/firmware/SDK helpers
   ├── configs/              # Maintained per-Board/backend configuration fragments
   ├── vendors/              # Fixed vendor dependencies and official imports
   ├── dependencies/         # Dependency/toolchain identities
   ├── tests/                # Host models, fault regressions and firmware link contracts
   ├── scripts/              # Build/evidence/HIL tooling
   └── docs/                 # Contracts, implementation history and current support

Layer Ownership
---------------

Arch owns CPU-local masks, exception queries and barriers. SoC owns controller
implementation, clock/IRQ/time, identity, physical Flash and private SDK context.
Board owns reviewed PCB resources. Platforms assemble the selected SoC/Board
objects and startup, without duplicating controllers. Runtime owns HAL/OSAL
infrastructure only. External applications own main, scheduler/workers,
component order, partitions, private protocols and product recovery.

The shared ``soc/stm32f407`` directory does not merge VE/VG/ZG identities or
Flash densities. Each build uses one Board/backend and one effective
configuration. ``soc/native`` is a host model and does not establish MCU timing.

See :doc:`../user_guide/architecture`, :doc:`../user_guide/build_system` and the
`maintained architecture contracts <https://github.com/X-Gen-Lab/nexus/blob/codex/industrial-platform-modernization/docs/strategy/target-architecture.md>`_.
