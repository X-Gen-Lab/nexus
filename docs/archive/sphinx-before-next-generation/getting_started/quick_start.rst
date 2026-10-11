Quick Start
===========

Build the platform contracts on Linux Native first. Nexus owns common platform
code; runnable application examples live in a separate pinned consumer repository.
Full Native profiles need OpenSSL 3 development headers/libraries. Use CMake
3.21+, Ninja, C11/C++17 and pinned Kconfiglib.

.. code-block:: bash

   git clone --recurse-submodules https://github.com/X-Gen-Lab/nexus.git
   cd nexus
   python3 -m pip install kconfiglib==14.1.0
   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4

Each build owns ``generated/effective.config``, ``nexus_config.h`` and
``config.cmake``. Source-root ``.config`` and stale header fallback are not used.
``native-minimal-debug`` builds without optional services/OpenSSL.

Run External Examples
---------------------

With repository access, clone
`nexus-examples <https://github.com/X-Gen-Lab/nexus-examples>`_ and initialize its
fixed Gitlink recursively. Run these commands in that repository:

.. code-block:: bash

   git submodule update --init --recursive
   python3 scripts/test_repository.py
   python3 scripts/build.py --preset native-debug --test

The examples own main, workers, component order, budgets and recovery policy.
Their lock/Gitlink and source-pair reports identify the exact Nexus tested.
There are no application binaries under the Nexus platform build tree.

MCU Software Integration
------------------------

Use the locked ARM GNU 14.3.rel1 complete GCC/newlib toolchain. Four Boards each
have baremetal/FreeRTOS Release profiles: Discovery F407VG, Qiming F407ZG, Sky
F407VE and Liangshan F470ZG. For example:

.. code-block:: bash

   cmake --preset gd32f470-armgcc-freertos-release
   cmake --build --preset gd32f470-armgcc-freertos-release --parallel 4
   python3 scripts/ci/validate_firmware_elf.py \
     --build-dir build/gd32f470-armgcc-freertos-release \
     --report build/gd32f470-armgcc-freertos-release/firmware-static.json

Top-level MCU output is independent ``nexus_contract_firmware.elf`` plus selected
map/bin/hex, not a business application. Source consumers default development
tests/contracts off. Default firmware owns all physical Flash with no storage
reservation; a consumer-owned layout chooses regions, offset zero only.

Boards are deferred by the user. ARM linkage and host models do not establish
physical startup, IRQ/DMA, electrical, power-loss or timing qualification.
Current evidence is in the root README and ``docs/strategy/support-matrix.yaml``.
See :doc:`build_and_flash`, :doc:`first_application` and :doc:`project_structure`.
