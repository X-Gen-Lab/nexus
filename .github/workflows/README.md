# Maintained platform workflows

Nexus owns the common SDK, Board packages and platform contracts. Product
applications and demonstration repositories are independent consumers. Platform
CI neither checks out those repositories nor requires their application targets.
A workflow definition is a gate specification, not execution evidence.

| Workflow | Gate |
|---|---|
| `ci.yml` | Detect platform/docs/workflow changes and require the appropriate build, quality and documentation results. |
| `build-matrix.yml` | Compile GCC Debug, GCC Release and Clang Release; execute one full uninstrumented Native Release suite; compile and inspect eight ARM Board/OSAL contracts; run separate coverage and sanitizer configurations. |
| `quality-checks.yml` | Run clang-tidy and cppcheck on an actual compilation database, rejecting missing tools, empty selection, process errors and findings. |
| `release.yml` | Build nine source-bound platform candidates, revalidate their archived inputs and ELF/test evidence, and create a draft after the full matrix passes. |
| `enterprise-tools.yml` | Exercise delivery adapters and validate approved requirement/role mappings. |
| `security.yml` | Audit resolved Python dependencies, extract actual CodeQL build contexts and scan secrets. |
| `performance.yml` | Measure Valgrind execution and ARM linked sections separately. Native CTest timing records are already retained by the normal matrix. |
| `docs-build.yml` | Build documentation with its declared tools. |

## Platform build and execution

Each preset owns its generated configuration and build directory. Unknown or
contradictory configuration, missing dependencies or unimplemented requirements
stop configuration. The generated Board identity binds the manifest and each
Board input; ARM builds additionally bind the resolved Flash layout, generated
header and linker script. CPU/SDK/Board sources remain explicit build targets.

The maintained ARM matrix covers STM32F407VG Discovery, STM32F407ZG Qiming V3.1,
STM32F407VE Sky Youth and GD32F470ZG Liangshan with baremetal and FreeRTOS.
Each member runs the actual vector, strong IRQ, registry ABI, memory and
Board/layout hash checks. GCC ARM comes from the reviewed official download
and checksum lock through `setup-build`; distribution ARM packages do not
replace that compiler identity. Native Release also installs this compiler so
its relocated source SDK test executes the ARM consumer, as well as C/C++
Native consumers. The release Native checkout initializes all five maintained
dependencies required by the full contract suite and source SDK package.
Initialization is recursive, including the recorded FreeRTOS nested gitlinks.

GCC Debug and Clang Release are compilation checks. GCC Release executes the
complete normal Native CTest suite once. Coverage and ASan/UBSan execute their
own configurations because instrumentation changes the contract being checked.
The sanitizer build uses the actual CMake target graph, with no application
list or hand-maintained regular expression. Leak checks remain enabled.

Every Native CTest execution uses `scripts/ci/ci_build.py`: delete the previous
report, require the subprocess to succeed, reject zero tests and validate fresh
JUnit with the shared strict parser. Failed, malformed, contradictory or all-skip
reports cannot pass. Partial skips are recorded as skips. Matrix artifacts retain
configuration, compile commands, linked files, JUnit/static reports and logs.

Coverage captures only the instrumented Native ownership scope: Arch, Runtime,
HAL, OSAL, Framework, Services and Native platform code. Board/SoC hardware
coverage needs separate execution. Capture errors and empty source records fail;
coverage percentages are evidence, without an invented qualification threshold.

## Tooling and HIL readiness

The tooling job consumes Native Release and Qiming FreeRTOS artifacts from the
same successful build matrix. It executes the production configuration, Board,
CI/release and report helpers and independent SDK consumer tests. The suites are:

```sh
python3 -m unittest discover -s scripts/configure -p 'test_*.py' -v
python3 -m unittest discover -s scripts/kconfig -p 'test_*.py' -v
python3 -m unittest discover -s scripts/ci -p 'test_*.py' -v
python3 -m unittest discover -s tests/validation -p 'test_*.py' -v
python3 -m unittest discover -s tests/hil -p 'test_*.py' -v
```

Actual artifact environment variables enable the CI helpers' real-build cases.
Logs are retained per suite, with shell pipe failure propagation. Workflow tests
parse YAML, validate shell syntax and execute the declared Native commands on
finite, zero-test, all-skip and stale-report CMake projects. They also execute the
ARM checker against the retained linked artifact. PyYAML is pinned for this
workflow parser; platform production helpers remain stdlib based.

HIL readiness uses explicit nonphysical adapters to check admission, identity,
readback, cleanup and failure boundaries. It never flashes an untrusted runner
or claims physical qualification. Real boards require a reviewed lab station,
probe identity and separately retained measurements.

## Candidate release boundary

A platform candidate contains Native plus all eight ARM members. Packaging
requires a clean matching source tag, reviewed toolchain/configuration,
production compile commands, pinned submodules or verified vendor import,
actual platform contract ELF and Native nonzero successful JUnit.

Archives retain the effective three-file configuration bundle, input fragment,
Board manifest and each input, generated Board/layout files, source linker
sections, optional explicit layout input, compiler commands, files and hashes.
Verification regenerates configuration-derived outputs, repeats actual ELF
checks and compares Board/layout sources with committed Git objects. A coherent
rewrite of archive checksums does not replace source identity.

Candidates are drafts. Checksums and provenance are unsigned; this pipeline
neither establishes reproducible builds nor signs or qualifies firmware.
Electrical, timing, DMA/ISR, persistence, manufacturing and product promotion
require their own evidence. No signing credentials belong in source or logs.
Branch protection selects the aggregate statuses explicitly; these workflows
never change repository settings.

## Local entry points

```sh
python3 scripts/ci/ci_build.py --preset linux-gcc-release --stage all --jobs 4
python3 scripts/ci/ci_build.py --preset stm32-armgcc-release --stage build --jobs 4
python3 scripts/ci/validate_firmware_elf.py --build-dir build/stm32-armgcc-release --report build/stm32-armgcc-release/firmware-static-contract.json
python3 scripts/ci/check_action_pins.py
```

Legacy wrappers forward to the same explicit preset runner and preserve failures.
Embedded compilation and static inspection remain separate from physical HIL.
