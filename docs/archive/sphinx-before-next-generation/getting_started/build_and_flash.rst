Build and Hardware Preparation
==============================

Nexus maintains Native and four MCU Board/backend software profiles. Build root,
compiler/mode, explicit config, Board and layout stay together through named
presets. Configuration never downloads a different dependency version.

.. code-block:: bash

   git submodule update --init --recursive
   python3 -m pip install kconfiglib==14.1.0
   cmake --list-presets
   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4

Full Native needs OpenSSL 3 development packages; minimal profile does not.
Runnable applications are built in the external examples repository. Top-level
MCU profiles instead produce an independent contract ELF and configured map,
BIN/HEX under their own ``bin`` directory.

ARM Profiles
------------

Install the fixed ARM GNU 14.3.rel1 complete toolchain using
``scripts/ci/install_arm_toolchain.py`` and ``dependencies/toolchains.lock.json``.
Example software check:

.. code-block:: bash

   cmake --preset stm32-sky-armgcc-baremetal-release
   cmake --build --preset stm32-sky-armgcc-baremetal-release --parallel 4
   python3 scripts/ci/validate_firmware_elf.py \
     --build-dir build/stm32-sky-armgcc-baremetal-release \
     --report build/stm32-sky-armgcc-baremetal-release/firmware-static.json

Discovery F407VG, Qiming F407ZG, Sky F407VE and Liangshan F470ZG each have a
baremetal and FreeRTOS Release preset. Check actual ELF vector/SP/reset, segment
permissions, strong IRQ/registry, density and Board/layout identity. The default
image owns the complete physical Flash with no storage reservation. External
layout selects product regions; image offset must be zero.

Deferred Hardware
-----------------

The user has deferred physical boards. Software and HIL tooling preparation
execute no probe, flashing, serial, waveform or power control. Four Board fixture
templates, admission/lease tools and UART challenge runner exist, with no bound
station identities or measured budgets. Missing hardware/configuration cannot
produce a passing physical report.

A later hardware stage must bind observed PCB/chip/probe/port/power identities,
reviewed bounded identify/flash/readback/reset/serial/cleanup commands and exact
source/config/Board/layout/toolchain/ELF-BIN evidence. IRQ/DMA, electrical,
Flash power loss, worst-load and long-load qualifications remain unexecuted;
complete workload runners for some of those tests are still pending. Read
``scripts/hil/README.md`` and ``docs/implementation/hil-readiness.md`` before
that separate stage. Host models and static admission always retain
``hardware_verified=false``.
