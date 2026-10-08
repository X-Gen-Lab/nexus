# Maintained GitHub workflows

The workflow files define gates; they are not execution evidence. Retain actual
logs, effective configuration, compiler commands, test reports and artifact
identities. The current local outcomes and unavailable tools/hardware are recorded
under `docs/implementation/`.

| Workflow | Contract |
|---|---|
| `ci.yml` | Detect code/docs/workflow changes, call the required jobs and reject missing or failed required results. |
| `build-matrix.yml` | Linux GCC Debug/Release and Clang Release host tests; STM32F407 baremetal/FreeRTOS ARM GCC compilation; host coverage and sanitizer contracts. |
| `quality-checks.yml` | Run real clang-tidy and cppcheck against the actual compilation database; findings, missing tools, empty source selection and process failures fail the job. |
| `docs-build.yml` | Build documentation using its declared tools; build results remain distinct from firmware/HIL evidence. |
| `release.yml` | Validate a matching source tag, build three candidate members, verify their identities and hashes, then create a draft. |
| `security.yml` | Audit resolved Python dependencies, run CodeQL and scan secrets; tool execution is distinct from cryptographic or product qualification. |
| `performance.yml` | Retain measured host execution and memory evidence and ARM image sizes; host timings do not establish control-loop deadlines. |

## Build and test inputs

Presets select compiler, build mode and input fragment. Each build directory owns
`generated/effective.config`, `nexus_config.h` and `config.cmake`. Source-root
`.config` and headers are not build inputs. Unknown or conflicting configuration,
missing implementations and missing pinned dependencies stop configuration.

The maintained matrix initializes GoogleTest, FreeRTOS, CMSIS and the STM32F4
SDK submodules at the repository gitlinks. Native builds use OpenSSL 3. The setup
composite installs `kconfiglib==14.1.0`; configure does not download dependencies.
ccache caches compilation, while each job resolves a fresh build configuration.
Whole build directories are not shared across products or compilers.

Host CTest invocations reject zero tests and save actual JUnit reports. Windows
and macOS presets remain available for separate validation; they are not part of
the maintained Linux execution matrix. ARM jobs compile the concrete F407/MB997
profile and do not claim electrical, IRQ/DMA, timing or power-loss qualification.

Coverage retains lcov/HTML artifacts in GitHub. No Codecov service or token is
required. The sanitizer job builds 18 standalone contracts plus five finite
Native applications, and runs ASan/UBSan with leak checking on its normal Linux
runner. A local environment limitation must be recorded separately and cannot
be silently copied into the CI settings.

## Required quality and supply-chain checks

External Actions are pinned to full commit identities; local composite/reusable
workflow paths remain local. Before changing them, run:

```sh
python scripts/ci/check_action_pins.py
python -m unittest discover -s scripts/ci -p 'test_*.py'
```

The static-analysis runner reads the compilation database and executes analyzers
without a shell. Reports are retained on success and failure. Missing analyzers
are not successful checks. Formatting and complexity guidance do not establish
MISRA compliance or functional-safety certification.

`ci.yml` evaluates required job results according to the detected change/event
policy. A prerequisite failure cannot turn a required skipped job into a green
status. Repository branch protection must select these statuses explicitly;
this maintenance run does not change remote repository settings.

## Candidate release boundary

The release matrix is Linux GCC Native, STM32F407 baremetal and STM32F407
FreeRTOS. Package validation checks the effective bundle, actual compilation
arguments, ELF architecture, board/chip/OSAL identities, clean matching source
tag and pinned dependency commits. Native requires a nonzero successful JUnit
report; recorded skipped counts do not establish support for the skipped cases.
ARM members are explicitly `cross-compile-only` and `hardware_verified=false`.

Candidate archives retain the input fragment, resolved bundle, CMake cache,
compile commands, ELF/images/maps, dependency identity, execution reports and
SHA-256 checksums. Aggregation rechecks member hashes and source gitlinks before
the final job creates a draft. Product promotion requires separately reviewed
HIL, resource, manufacturing and security evidence. It reuses the validated
artifacts rather than rebuilding an undocumented image.

No production signing key belongs in the repository, fixtures, logs or artifacts.
A workflow definition or draft is not a qualified release. See
`docs/implementation/release-workflow.md` and
`docs/sphinx/development/release_process.rst` for the implemented candidate gates.

## Local entry points

```sh
python scripts/ci/ci_build.py --preset linux-gcc-debug --stage all --jobs 4
python scripts/building/build.py --preset linux-gcc-release --stage build --jobs 4
python scripts/test/test.py --preset linux-gcc-debug --jobs 4
python scripts/ci/ci_build.py --preset stm32-armgcc-release --stage build --jobs 4
```

All wrappers delegate to the same preset workflow and propagate failures. They
do not infer a toolchain from root `.config`, delete build directories or look
for the retired `nexus_tests` executable. `--stage test` runs an already built
host-test preset; embedded execution requires a real HIL station and report.
