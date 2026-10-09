Maintained Script Entry Points
==============================

Scripts orchestrate the same CMake/CTest effective configuration; they do not
maintain another build DSL or implicit business application selection.

.. code-block:: bash

   python3 scripts/ci/ci_build.py --preset linux-gcc-debug --stage all --jobs 4
   python3 scripts/ci/ci_build.py --preset gd32f470-armgcc-freertos-release --stage build --jobs 4
   python3 scripts/ci/validate_firmware_elf.py \
     --build-dir build/gd32f470-armgcc-freertos-release \
     --report build/gd32f470-armgcc-freertos-release/firmware-static.json
   python3 -m unittest discover -s scripts/ci -p 'test_*.py'

Use an explicit preset/stage. Wrapper exits preserve actual failures; required
host execution rejects zero tests, stale/empty/all-skipped JUnit and inconsistent
counts. Embedded build/check does not execute hardware.

The locked ARM installer is ``scripts/ci/install_arm_toolchain.py``. Board/layout
and package tools validate real declared sources/dependencies/config/artifacts.
``cmake/package/package_source_sdk.py`` defaults to complete clean publishable
source and exact export identities; development fixtures require explicit opt-in.

``scripts/hil`` contains fixture/admission/lease/challenge preparation. Hardware
is deferred; no station/probe/serial/power command is executed in this delivery.
``scripts/evidence`` checks candidate/promotion structure and trust policy,
without creating product signing/manufacturing qualification from models.

Runnable application scripts belong to the separate examples repository. Formal
SDK consumption uses matching Gitlink/lock; optional full-commit candidate CI
records its actual pair without modifying that formal pin. Read tool ``--help``
and linked source-bound evidence rather than old unsupported platform examples.
