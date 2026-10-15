# Nexus embedded platform

Nexus is a statically assembled C11 platform for industrial MCU software. It
provides reusable CPU, controller, typed I/O, optional OS and component mechanisms.
Applications, workers, product roles, private PCB data, Flash reservations and
recovery policy live in [nexus-examples](https://github.com/X-Gen-Lab/nexus-examples)
or private product repositories. [中文入口](README_CN.md).

**This checkout uses Nexus 0.2.0 for development and software-candidate work.**
It is not a published 0.2.0 release; a version value does not establish software
or hardware qualification. Consumers recompile source under the exact selected
configuration and toolchain. Read the [changelog](CHANGELOG.md) before integration.
The first public Source SDK preview remains Nexus 0.1.0, dated 2026-10-15
(Asia/Shanghai); its [historical release guide](docs/status/release-0.1.0.md)
describes those immutable released bytes. Physical MCU HIL is `not_executed`.

One authored TOML assembly selects reviewed SoC/Board facts and exact instances.
The typed factory returns existing immutable polymorphic faces; it does not
allocate, initialize hardware, lock or discover devices. Ordinary I/O dispatches
through shared readonly ops to independent private instance contexts. CMake owns
the source dependency graph. Kconfig and cache macros are not parallel authorities.

| Layer | Responsibility |
| --- | --- |
| `core/` | Results, monotonic deadlines, borrowed requests and stable epochs |
| `arch/` | Compile-time CPU primitives, masks, barriers and optional Cache/MPU/SAU |
| `soc/` | Exact chip memory, clock, startup, IRQ/DMA and private controllers |
| `boards/` | Reviewed wiring, oscillator facts, initial levels and explicit unknowns |
| `io/` | Typed public operation contracts; provider/SDK types remain private |
| `os/` | Optional caller-owned baremetal, Native, FreeRTOS and maintained ThreadX adapters |
| `components/` | Bounded reusable algorithms and explicit owner/port adapters |
| `tools/` | Resolution, build, measurement, HIL preparation and evidence |
| `tests/` | Actual host, register-model, ARM-link and tool boundary executions |

There is no default heap, registry, driver worker or maximum instance pool.
Accepted asynchronous requests retain storage until acquire-observed SETTLED;
cancel, timeout and failed drain preserve that borrow. Finite and continuous
IRQ/DMA modes are explicit maintained selections with different buffer/loan and
progress contracts. No arbitrary load or IRQ delay is promised lossless.

## Start with a verified task

- [Build an independent relocated Source SDK consumer](docs/tutorials/source-sdk.md).
- [Execute rejection, cancellation and drain fixtures](docs/tutorials/async-io.md).
- [Inspect the complete assembly format](docs/reference/configuration/assembly.md).
- [Understand layers, factory and ownership](docs/explanation/platform-architecture.md).
- [Check exact platform support and qualification](docs/reference/platforms/support.md).
- [Develop with TDD and normal commit gates](docs/development/workflow.md).

The [document entry](docs/index.md) provides one task navigation; original
[design contracts](docs/design/README.md) and [delivery records](docs/delivery/README.md)
retain their authority and historical source identity.

## Build from the checkout

Run from the repository root with host Python, Git, C/C++ tools and the maintained
CMake/Ninja versions available:

```sh
git submodule update --init --recursive
python scripts/setup/install_dev_tools.py
.venv/bin/python -m pip install -r dependencies/environment-tools.txt
python tools/dev/dev.py doctor
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4
python tools/dev/dev.py test --preset native-debug
```

Use the exact maintained preset and locked ARM GNU distribution for MCU builds.
External products consume `Nexus::Platform` and `nexus_add_firmware()` from a
verified Source SDK. Platform start does not generate tasks or start a scheduler.
GoogleTest/GoogleMock is pinned and host-only; firmware remains C11.

The release SDK requires the complete pinned dependency and license closure.
Its manifest records the exact clean source commit/tree, recursive dependencies
and every packaged file. Use `find_package(Nexus 0.2.0 EXACT CONFIG REQUIRED)`;
the source interface epoch remains `1` and authored assembly schema remains `2`.
The compatibility JSON is the sole authored version authority. The release
receipts identify the tools actually executed; declared minimum versions and
old CI results do not establish a new tool tuple or release qualification.

## Support and evidence boundaries

F407ZG Qiming, F407VE Sky and F470ZG Liangshan maintain explicit first-platform
assemblies. G474, H743 and U575 have separate narrow reference scopes. CPU-only
Core/Arch/Runtime profiles cover M0/M0+/M3/M4/M7/M23/M33/M55/M85 without creating
SoC/Board/factory support. ThreadX maintains its reviewed Native/M4 subset and
does not inherit FreeRTOS CPU/isolation coverage.

The [capability matrix](docs/delivery/capabilities.md) separates modes, routes,
Board declarations, fixtures and implementation source. The
[qualification guide](docs/reference/platforms/qualification.md) explains exact
code/configuration/tool/artifact evidence. **Physical MCU HIL remains not executed.**
Document publication, ARM linking and software candidates do not establish
hardware timing, electrical safety, signed product release or LTS commitments.

Formatting and backslash Doxygen style follow the existing root policies.
Commit hooks and CI preserve real failures and never stage or silently fix files.
[Historical material](docs/archive/README.md) does not establish current support.
