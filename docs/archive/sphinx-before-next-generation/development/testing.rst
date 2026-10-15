Testing
=======

Run tests against a named build and retain source revision, resolved
configuration, dependencies, executed count and result. A configure success or
empty test run is not validation. Native models, STM32 host fakes and physical
boards establish different evidence.

Build and select tests
----------------------

Use the prerequisites in :doc:`../getting_started/build_and_flash`, including
pinned Google Test and OpenSSL 3 development libraries.

.. code-block:: bash

   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4
   ctest --preset linux-gcc-debug -N
   ctest --preset linux-gcc-debug -L osal --output-on-failure --no-tests=error
   ctest --preset linux-gcc-debug -L config --output-on-failure --no-tests=error
   ctest --preset linux-gcc-debug -L contract --output-on-failure --no-tests=error
   ctest --preset linux-gcc-debug -R '^native_.*_smoke$' --output-on-failure --no-tests=error

Labels come from each suite's CMake target. List cases in actual executables:

.. code-block:: bash

   build/linux-gcc-debug/bin/hal_native_tests --gtest_list_tests
   build/linux-gcc-debug/bin/osal_tests --gtest_list_tests
   build/linux-gcc-debug/bin/config_tests --gtest_list_tests

Use a listed name with ``--gtest_filter`` when narrowing a failure. There is no
combined ``nexus_tests`` binary or shared ``build/tests/Release`` layout.

Configuration and workflow tools
--------------------------------

These suites exercise real generator/subprocess behavior without Google Test:

.. code-block:: bash

   python scripts/kconfig/test_effective_config.py
   python -m unittest discover -s scripts/ci -p 'test_*.py'
   python -m unittest discover -s scripts/evidence -p 'test_enterprise_tools.py'

CTest also registers effective-configuration regressions. Enterprise fixtures
exercise adapters and evidence validation; they do not establish real-board,
production-signer or manufacturing-line qualification.

Sanitizers
----------

.. code-block:: bash

   cmake --preset linux-gcc-sanitizers
   cmake --build --preset linux-gcc-sanitizers --parallel 4
   ctest --preset linux-gcc-sanitizers -L contract --output-on-failure --no-tests=error

Record sanitizer runtime and environmental overrides. Disabling LeakSanitizer
because a container restricts tracing leaves leak detection unverified.
Sanitizers on host fakes do not establish real DMA/IRQ or deadline behavior.

Useful regressions
------------------

Place tests alongside their domain under ``tests/hal/native``, ``tests/osal``,
``tests/config``, ``tests/drivers``, ``tests/storage_security`` or
``tests/industrial``, and register the target in that directory's CMake file.
Assert public behavior and failures: stale handles, bounded waits, exhaustion,
callback ownership, partial I/O, cancellation and clean shutdown. Application
smokes must use production initialization; fixture-only device registration does
not establish application readiness.

Preserve reproducible seeds and failing inputs. Persistence tests restart and
verify a complete old or new snapshot, rather than only a write error. Physical
power removal, Flash stalls and electrical behavior require board experiments.

Evidence and gates
------------------

Save nonzero JUnit results for the matching build:

.. code-block:: bash

   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --output-junit build/linux-gcc-debug/test-results.xml

``scripts/evidence`` binds test, configuration and artifact digests to source
identity. Skips, unconfigured adapters and missing evidence retain their real
status. Required analyzers fail on tool errors, findings and an empty owned
translation-unit set. A workflow definition is not analyzer execution, and
there is no blanket MISRA certification claim. Product requirements define
physical HIL and production release acceptance.
