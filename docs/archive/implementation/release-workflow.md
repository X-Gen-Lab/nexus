# Release workflow implementation

Requirements: REL-001, BAS-002, BAS-003; ADR 006. The current release candidate
matrix has nine profiles: Linux GCC Native plus baremetal/FreeRTOS ARM GCC for
F407VG Discovery, F407ZG Qiming V3.1, F407VE Sky Youth and GD32F470ZG Liangshan.
Windows/macOS remain separate validation work. Current source-bound build and
static ELF evidence is recorded in [platform delivery](platform-delivery.md),
[STM32 runtime](stm32-runtime.md), and [GD32F470](../../platforms/gd32f470/README.md).
The first three-profile execution checkpoint below remains historical.

| Board | Baremetal Release | FreeRTOS Release |
|---|---|---|
| Discovery | `stm32-armgcc-release` | `stm32-armgcc-freertos-release` |
| Qiming V3.1 | `stm32-qiming-armgcc-baremetal-release` | `stm32-qiming-armgcc-freertos-release` |
| Sky Youth | `stm32-sky-armgcc-baremetal-release` | `stm32-sky-armgcc-freertos-release` |
| Liangshan | `gd32f470-armgcc-baremetal-release` | `gd32f470-armgcc-freertos-release` |

The host profile is `linux-gcc-release`. These are software candidates; actual
PCB revision, physical HIL, product budgets and enterprise promotion are separate.

## Artifact contract

The package tool consumes `generated/effective.config`, `nexus_config.h`, and
`config.cmake` and checks every resolved symbol. It does not trust retired
`NEXUS_CONFIG_HEADER` or CPU/FPU/ABI cache aliases. The source fragment is packaged
as input provenance, separately from effective configuration. Cache source
identity, Release mode, build options, board/chip/OSAL/toolchain identity,
production compiler arguments and reference-application ELF architecture must
agree. ARM's resolved toolchain name is `arm-none-eabi-gcc`.

The archive contains actual CMake cache and compile commands, the configuration
bundle, input fragment, source/dependency commits, target identity and checksums.
Native requires a nonzero JUnit report without failures/errors and records
executed/skipped counts separately. ARM validation is static cross-build/ELF evidence;
`hardware_verified` is false and the physical board revision is unknown.
Available CTest logs are retained. Required dependencies must be initialized at
their recorded commits; unrelated inactive SDKs are not release prerequisites.
Tracked and untracked source changes cannot silently enter clean provenance.
GD32 uses the reviewed original SDK 3.3.3 import lock, file hashes and retained
license notices, separately from Git submodules. ARM packaging checks the actual
ELF vector/SP/Reset/strong IRQ, compiled ABI probe and partition layout, without
turning those checks into execution or hardware evidence.

The aggregation step checks member/transfer hashes, rechecks target/configuration
and validation identity, binds dependency commits to the source Git gitlinks, and
requires all nine assets selected by the workflow. Only then does the job with
write permission create a draft. Candidate promotion must reuse reviewed assets.

## Historical first three-profile checkpoint

The following original counts, fixture scopes and temporary records are retained
for traceability. Later profiles and commits need their own execution identity;
these counts are not the current whole suite.

- `python -m unittest discover -s scripts/ci -p 'test_*.py' -q`: 74 tests passed,
  including 55 release behavior tests. Fixtures use local Git submodules and
  ELF/configuration bundles, not network downloads or firmware execution.
- Release behavior checks cover all three candidates from one source commit,
  configuration/header/CMake disagreement, wrong target and compiler arguments,
  tracked/untracked source and dependency drift, empty/failing test reports,
  traversal/symlinks, corrupted assets, and false validation identities even
  after checksums are recomputed.
- Workflow YAML was parsed; all three configure/build presets and the Native
  test preset exist. Dependency initialization precedes configuration, and draft
  creation depends on all build jobs with a separate write permission.
- The actual Native Release build bundle passed the same validators: 324
  effective symbols, matching cache/options and production compile commands,
  plus an x86_64 reference ELF. This was read-only validation of the build
  configuration and selected compiled outputs, not a complete candidate package.
- Real Kconfig generation for the ARM baremetal and FreeRTOS Release fragments
  produced coherent bundles with 389 and 398 effective symbols respectively.
  Chip, board, toolchain, CPU, FPU, float ABI and backend match the release
  profiles. This generated configuration only; no ARM compiler or hardware was
  executed by this release task.

At that checkpoint, no tag was created and no package bypassed the clean-source/matching-tag/Native
JUnit requirements. GitHub Actions, release creation, full candidate packaging
and HIL are not execution claims here. SHA-256/provenance are unsigned integrity
and traceability records; signing, reproducibility attestation and SBOM remain
separate product gates; map-linked archive SBOM and public-key promotion tools
now exist, but no physical HIL, production signing service or complete license
qualification is inferred. MCU has no default cryptographic provider.
