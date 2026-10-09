Nexus Embedded Platform
=======================

Nexus owns reusable embedded mechanisms: Core, Arch, SoC, fixed typed I/O,
optional OS adapters and optional common components. Product workers, policy,
private PCB facts and Flash reservations belong to external consumers.

The current C11 implementation uses one strict SoC/Board/assembly resolution,
explicit CMake targets and caller-sized static storage. Source and compiled
software evidence are separate from physical hardware qualification.

.. toctree::
   :maxdepth: 2

   getting_started
   architecture
   configuration
   migration
   support
   development/index
   api

Chinese architecture, engineering contracts and the delivery ledger live in
``docs/design/`` and ``docs/delivery/``. Earlier HAL/OSAL/Kconfig tutorials are
retained under ``docs/archive/`` as history; they do not describe current APIs
or establish current qualification.
