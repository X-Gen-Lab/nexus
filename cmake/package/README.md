# Relocatable Nexus source SDK

Nexus 0.2.0 is the current development Source SDK version, not a published
release or a qualification result. The consumer owns its assembly,
application sources, compiler, ABI, workers and resource budgets. Vendor headers
and concrete provider storage stay private. No precompiled MCU binary ABI or
application generator is exported.

`dependencies/platform-compatibility.json` is the sole authored semantic-version
policy. CMake, generated headers and the exported package derive their version
from it. Source interface epoch `1` and authored assembly schema `2` are separate
identities; they do not change solely because the semantic version changes.

Prepare from an initialized, clean, committed checkout:

```sh
python cmake/package/package_source_sdk.py --source . --output /tmp/nexus-sdk
```

The package records the source commit/tree, every packaged file hash, required
recursive dependency commits/trees, the reviewed GD32 import and license notices.
The installed CMake package verifies those bytes before configuring a consumer.
Modified, missing, extra or symlinked files fail verification. Preparation and
verification do not fetch missing dependencies.

A dirty checkout can produce an explicitly nonpublishable development fixture
with `--development-fixture`. Its consumers must deliberately set
`NEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON`. This does not qualify a release.

Move the complete prefix, then consume it from an independent project:

```cmake
cmake_minimum_required(VERSION 3.31)
project(my_device LANGUAGES C ASM)
set(NEXUS_ASSEMBLY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assembly.toml")
find_package(Nexus 0.2.0 EXACT CONFIG REQUIRED)
nexus_add_firmware(my_device SOURCES main.c)
```

```sh
cmake -S . -B build -G Ninja \
  -DNexus_DIR=/relocated/nexus-sdk/lib/cmake/Nexus \
  -DCMAKE_TOOLCHAIN_FILE=/relocated/nexus-sdk/share/nexus/src/cmake/toolchains/arm-gcc.cmake
cmake --build build
```

An assembly chooses a reviewed Board package, exact part, clock, OS backend,
explicit controller modes, device endpoints and memory budgets. Relative Board
paths are relative to the assembly, including when it is outside the SDK.
`NEXUS_EXPECTED_SOURCE_REVISION` can enforce a consumer-approved exact revision.
Native models use a host compiler and omit the ARM toolchain argument.

`Nexus::Core`, `Nexus::Arch`, `Nexus::IO`, `Nexus::Platform` and optional component
or OS targets expose their own includes and dependencies. `nexus_add_firmware`
links explicit sources, selected components, startup, generated bindings and the
resolved linker input. It does not create `main`, tasks or a scheduler. Consumers
start hardware with `nx_platform_start`. Generated `nexus_factory.h` supplies
compact typed IDs and pure static getters such as
`nx_factory_gpio(NX_GPIO_ID_LED0)`. A getter neither initializes hardware nor
allocates memory. The returned readonly face dispatches through one shared
provider operations table to its own mutable context. Generated
`nx_binding_<id>` and `nx_device_<id>` symbols remain fixed typed aliases.
Backend and application startup
order remains explicit in consumer code.

Every MCU firmware produces ELF, BIN, map and a checked resource report. The ELF
contains the configuration digest and the real startup vector. The selected CPU
facts determine the instruction set, optional FPU, float ABI and enum ABI;
consumers inherit those flags through Nexus targets. C++ consumers use their declared embedded
runtime policy, such as `-fno-exceptions -fno-rtti` for the tested source SDK path.

The maintained boundary test is `tests/contracts/test_source_sdk.py`. It packages
the real checkout, relocates into a path with spaces, verifies byte tamper and
identity rejection, executes independent Native C/C++ consumers and links both
maintained MCU C/C++ consumers with actual startup/linker/resource gates.
Those software checks do not establish physical Board qualification.

Use the release asset inventory and actual execution receipts to identify the
SDK source, tool tuple, dependency/license closure and consumer results. The
declared minimum Python/CMake versions do not establish installation or
integration on every host. Linux is the maintained executed host scope; physical
MCU HIL remains `not_executed`. Historical software candidates keep their original
source identity. The [0.1.0 release scope](../../docs/status/release-0.1.0.md)
is historical and does not qualify this development version.

Release automation verifies an existing publishable installed prefix with the
same consumer template, without the development opt-in:

```sh
python cmake/package/verify_consumers.py \
  --prefix /relocated/nexus-sdk --output build/installed-sdk-check
```

`verification.json` and per-command logs retain actual arguments and exit codes.
This command fails for development fixtures, missing tools, any compile/run/link
failure, leaked private includes or failed ELF resource/configuration binding.
The SDK prefix and verification output must be canonical paths without symlink
aliases, and neither may contain the other. Admission failure leaves both SDK
bytes and any previous report untouched and writes its diagnostic to stderr.
Only an admitted owned verification directory is invalidated for a new attempt.

The package includes the repository formatter and comment-policy files. Git
commit hooks and whole-checkout lint workflows remain repository maintenance
tools; they are not installed into the consumer's Git repository by `find_package`.

The source SDK includes authored TOML readers, typed resolution and factory
generation, maintained default assemblies and dependency provenance locks.
The pinned `ext/googletest` Git submodule, its licenses and contract binaries
belong to host repository development and are excluded from the firmware SDK
payload.
Configuring a firmware consumer never downloads a test dependency. A consumer
that needs platform development tests uses the complete repository checkout;
the installed source SDK explicitly excludes those contract targets.
