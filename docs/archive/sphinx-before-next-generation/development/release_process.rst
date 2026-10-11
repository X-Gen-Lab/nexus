Release Candidates
==================

Nexus stages a draft candidate after its maintained build matrix succeeds.
A candidate is an engineering artifact; hardware qualification, signing and
product approval require separate evidence. No support window or LTS term is
currently promised by this workflow.

Maintained matrix
-----------------

* ``linux-gcc-release``: Linux x86_64, Native OSAL, host contract tests.
* ``stm32-armgcc-release``: STM32F407, STM32F4DISCOVERY MB997, baremetal.
* ``stm32-armgcc-freertos-release``: the same reference board with FreeRTOS.

The two MCU profiles are cross-compile candidates. Board revision, electrical
behavior, interrupt priorities, DMA cancellation, power-cut recovery and control
latency still need HIL reports. Windows/macOS presets and GD32 are outside the
current release matrix. MCU builds have no default cryptographic provider;
products requiring encrypted configuration or firmware authentication need an
integrated, validated provider before product release.

Build and evidence
------------------

Use a clean checkout of the intended source commit and its pinned dependencies.
The tag must be an existing ``vMAJOR.MINOR.PATCH`` tag, optionally followed by a
prerelease identifier such as ``-rc.1``, and must identify that exact commit.

For example, the Native candidate build commands are:

.. code-block:: bash

   git submodule update --init ext/googletest ext/freertos
   cmake --preset linux-gcc-release
   cmake --build --preset linux-gcc-release --parallel 4
   ctest --preset linux-gcc-release --parallel 4 --output-on-failure --no-tests=error --output-junit "$PWD/build/linux-gcc-release/ctest-results.xml"

Use the ARM preset names above after initializing ``vendors/arm/CMSIS_5``,
``vendors/st/cmsis_device_f4`` and ``vendors/st/stm32f4xx_hal_driver``; FreeRTOS also
requires ``ext/freertos``. ARM execution and HIL are separate from cross-compiling.

Each build owns ``build/<preset>/generated/`` with ``effective.config``,
``nexus_config.h`` and ``config.cmake``. The input fragment is provenance only.
The package tool rejects disagreement among the resolved files, cache options,
Release mode, target identity and production compile commands. It checks ARM
CPU/FPU/float ABI flags and requires an ELF reference application with the
expected architecture. Libraries or raw firmware blobs alone are insufficient.

Native packaging requires ``ctest-results.xml`` with at least one successful
execution and no failure or error. Skipped tests are counted separately. The
archive includes available CTest logs, resolved configuration, input fragment,
CMake cache, compile commands, source and dependency commits, board/SoC/OSAL
identity, file sizes and SHA-256 hashes. ARM validation is recorded as
``cross-compile-only`` with ``hardware_verified: false``.

Candidate workflow
------------------

``.github/workflows/release.yml`` runs on version tags or an explicitly selected
existing tag. It performs the following sequence:

1. Validate the tag format and its commit.
2. Checkout the same commit for all three builds, initialize maintained
   dependencies, configure and build; run Native tests with zero tests rejected.
3. Package verified outputs and retain build/test evidence even if a job fails.
4. Require the complete candidate asset set. Verify archive and member hashes,
   resolved configuration, target identity, test counts and dependencies against
   the source Git gitlinks.
5. Generate candidate notes and a combined ``SHA256SUMS``, then create a draft
   with ``gh release create --draft --verify-tag``. Prerelease tags also use
   ``--prerelease --latest=false``.

The workflow uses read permissions except for the final draft-creation job.
It does not overwrite an existing candidate. Tag creation and workflow dispatch
are explicit repository actions; this guide does not perform them.

Package contract tests run without network access:

.. code-block:: bash

   python -m unittest discover -s scripts/ci -p test_package_release.py -v

These tests use local Git repositories/submodules and binary-format fixtures.
They test package behavior rather than claiming the firmware built or ran.
Executed build results belong in ``docs/implementation/`` and the retained CI
reports.

Product promotion
-----------------

Promote the reviewed candidate assets without rebuilding a different image.
The product owner must supply the exact board revision, resource and deadline
budgets, HIL/fault-recovery evidence, deployment/recovery policy and any required
signing identity. Security and manufacturing credentials must remain outside
source and artifacts.

SHA-256 and provenance are unsigned integrity and traceability records. They are
not authenticity signatures, reproducible-build attestations or firmware
signatures. SBOM, supply-chain locking, signing and LTS commitments remain
separate work until their implementation and execution evidence exist.

See also
--------

* :doc:`testing` - Test execution and evidence.
* :doc:`code_review_guidelines` - Review process.
* ``docs/strategy/first-iteration-release.md`` - Candidate implementation details.
