# Relocatable Nexus source SDK

Nexus 1.0 exports reusable platform sources. The consumer owns its assembly,
application sources, compiler, ABI, workers and resource budgets. Vendor headers
and concrete provider storage stay private. No precompiled MCU binary ABI or
application generator is exported.

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
set(NEXUS_ASSEMBLY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assembly.json")
find_package(Nexus 1.0.0 EXACT CONFIG REQUIRED)
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
start hardware with `nx_platform_start`; generated `nx_binding_<id>` and
`nx_device_<id>` symbols are fixed typed aliases. Backend and application startup
order remains explicit in consumer code.

Every MCU firmware produces ELF, BIN, map and a checked resource report. The ELF
contains the configuration digest and the real startup vector. Default MCU ABI
is Cortex-M4F, FPv4-SP-D16, hard float and short enums; consumers inherit those
flags through Nexus targets. C++ consumers should use their declared embedded
runtime policy, such as `-fno-exceptions -fno-rtti` for the tested source SDK path.

The maintained boundary test is `tests/contracts/test_source_sdk.py`. It packages
the real checkout, relocates into a path with spaces, verifies byte tamper and
identity rejection, executes independent Native C/C++ consumers and links both
STM32F407 and GD32F470 C/C++ consumers with actual startup/linker/resource gates.
Those software checks do not establish physical Board qualification.

Release automation verifies an existing publishable installed prefix with the
same consumer template, without the development opt-in:

```sh
python cmake/package/verify_consumers.py \
  --prefix /relocated/nexus-sdk --output build/installed-sdk-check
```

`verification.json` and per-command logs retain actual arguments and exit codes.
This command fails for development fixtures, missing tools, any compile/run/link
failure, leaked private includes or failed ELF resource/configuration binding.
An old owned verification directory is invalidated before a new attempt.

The package includes the repository formatter and comment-policy files. Git
commit hooks and whole-checkout lint workflows remain repository maintenance
tools; they are not installed into the consumer's Git repository by `find_package`.
