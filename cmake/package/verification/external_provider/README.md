# Canonical external provider starter

Copy this directory from a verified Source SDK to create an external C11 provider
workspace. It is the single source used by checkout contracts, SDK packaging and
installed/relocated consumer verification. The package is a Native GPIO software
model; it adds no MCU support, Board wiring or physical qualification.

| File | Purpose |
| --- | --- |
| `assembly.toml`, `board/board.json` | Explicit package selection and reviewed Native PA0/PA3 model routes |
| `provider/nexus-soc.json` | Strict identity and SHA-256 input manifest |
| `provider/configure/` | Typed configuration, independent static state and shared readonly faces |
| `provider/soc/` | Private C adapter, Native GPIO reuse and bounded fault injection |
| `provider/include/` | Public test observation port with no vendor or private state |
| `consumer.c` | C11 acceptance executable with no framework or heap |
| `lifecycle_test.cpp` | Optional host GoogleTest contracts using the same adapter |

The test owner calls the lifecycle serially. Factory lookup has no hardware
effects and returns stable static faces; the contexts are independent and share
one readonly operations table. Start prepares both instances. Injecting failure
at the second prepare runs the first instance's actual C stop path. Both faces
then reject I/O; clearing the fault permits repeated start/stop recovery.

From the copied workspace, using CMake, Ninja, a C compiler and ASM support:

```sh
cmake -S . -B build -G Ninja -DNexus_DIR=/path/to/sdk/lib/cmake/Nexus
cmake --build build --target external_lifecycle_consumer --parallel 1
./build/bin/external_lifecycle_consumer
```

A zero exit means the C model assertions passed. A nonzero exit identifies a
lookup, normal-cycle, rollback or recovery failure through `consumer.c`. Preserve
the build and logs before changing the package. The default project requires no
C++ compiler. Its SDK prefix must be explicit; there is no checkout fallback.

For checkout integration, explicitly pass
`-DNEXUS_REFERENCE_PLATFORM_SOURCE=/path/to/nexus`. For optional host contracts,
set `-DEXTERNAL_LIFECYCLE_HOST_TESTS=ON` and
`-DNEXUS_REFERENCE_GOOGLETEST_SOURCE=/path/to/prepared/pinned/googletest`, then build
`external_lifecycle_contract_test` and run it with a fresh GoogleTest XML report.
These tests require C++17 and never enter the production C11 executable.

Review changed source before updating the manifest. With the SDK's Python tool:

```sh
python /path/to/sdk/share/nexus/src/tools/configure/integration_package.py \
  refresh-inputs --package provider
python /path/to/sdk/share/nexus/src/tools/configure/integration_package.py \
  check --package provider --json
```

Refresh updates existing declared paths and preserves package version; explicitly
add new files before refreshing. Check validates the package without executing its
Python modules. SDK copying expands verified `inputs`, including nested private
headers and helpers, and does not collect unrelated files recursively. Source
identity checks do not replace trust review of selected CMake/Python code.

Use a fresh build after moving an installed SDK prefix, including prefixes with
spaces. Keep vendor and adapter private include directories off the public
consumer compile row. The Native host result proves only this software model.
Existing exact STM32/GD32 ARM consumers establish separate compile/link boundaries;
new hardware requires reviewed facts, routes, C lifecycle implementation and its
own physical acceptance. Product tasks, recovery policy and private application
examples remain in their external repositories.
