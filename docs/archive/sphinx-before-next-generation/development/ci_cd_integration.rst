CI and Source-Pair Validation
=============================

CMake presets/CTest are the build authority; workflows invoke the same commands
and bind actual source/config/toolchain/dependencies/artifacts. Workflow files,
cache restoration or a passing archive are not execution evidence.

Platform Gates
--------------

Required jobs cover selected Native compiler/build variants, meaningful
sanitizers/analysis, strict helper/configuration checks and four Board ×
baremetal/FreeRTOS ARM software profiles. Exact selected tests, counts, source
and results are recorded by each run, rather than copied from another commit.

.. code-block:: bash

   python3 scripts/ci/ci_build.py --preset linux-gcc-debug --stage all --jobs 4
   python3 scripts/ci/ci_build.py --preset stm32-qiming-armgcc-freertos-release --stage build --jobs 4
   python3 scripts/ci/validate_firmware_elf.py \
     --build-dir build/stm32-qiming-armgcc-freertos-release \
     --report build/stm32-qiming-armgcc-freertos-release/firmware-static.json

Kconfig generation rejects unsupported/contradictory choices and stale outputs.
Actual ELF checks startup/vector/SP/reset, strong IRQ/registry retention,
segments, ABI, physical density and Board/layout hashes. Owned translation-unit
analysis and minimal consumers validate private SDK/public dependency boundaries.
Dependency/toolchain identities are fixed; configuration does not download
arbitrary upgrades.

Required Execution
------------------

Required runs accept only actual success. Failure, cancellation, timeout, missing
or empty evidence and required all-skipped/zero-test suites fail. A legitimate
unselected job retains its reason. Commands propagate nonzero exits; delete old
reports before execution and validate fresh JUnit/test enumeration/counts.
Do not add continue-on-error to hide failures or classify an uninstalled tool as
passed. Overlapping scoped suites cannot be summed into a full-platform total.

External Applications
---------------------

The examples repository owns runnable applications and fixes a formal Nexus
Gitlink plus matching dependency lock. Public API/config changes update platform
caller/provider/test/docs together, then external pin/callers and actual Native/
eight ARM matrix. Platform workflows do not copy private example sources.

Examples workflow_dispatch may explicitly select one full 40-character lowercase
candidate Nexus commit. Empty input uses formal pin. Candidate source must be
clean and match origin/actual checkout/dependency identities; the same example
matrix executes. It does not modify formal lock/Gitlink and does not claim that
every core PR automatically triggers cross-private-repository checks.

Delivery and Qualification
--------------------------

Current exact source-pair/pass values are authoritative in external examples
validation reports and lock. Historical reports retain their source scope.
This avoids rewriting the platform's own final SHA into its source. Same-artifact
software delivery binds ELF/BIN/HEX/map, config/Board/layout/toolchain/dependencies,
nonzero tests, licenses and known limits. Relocatable source SDK requires clean
preparation/verification and actual moved consumers, not just alias existence.

The user has deferred hardware. HIL fixture/admission/leases/challenge preparation
is separate from real probe/flashing/serial/waveforms/power loss/long-load.
Product trust/signing/manufacturing, named reviewer/backup, support window and LTS
remain external qualification, not automatic effects of green software jobs.
