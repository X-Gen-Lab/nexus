# Build and effective configuration implementation

Requirements: BAS-001–004. The maintained build graph uses normal CMake targets
and target usage requirements. The former optimization, dependency resolver,
plugin loader and toolchain inference modules have been removed. CPU flags never rewrite compiler selection, build mode or directory-global
flags. CMake presets select the compiler, build mode and source configuration
fragment; Kconfig selects platform resources and OSAL behavior.

## Configuration ownership

The default host fragment is `platforms/native/defconfig`. The maintained ARM
reference fragments are `configs/stm32f407_baremetal_defconfig` and
`configs/stm32f407_freertos_defconfig`. A source-root
`.config` and source-root generated header are never consumed. They are removed
from version control. New product profiles must use explicit fragments.

A single Kconfig resolution creates the bundle below in the build directory:

- `generated/effective.config`: resolved values, including defaults and disabled symbols.
- `generated/nexus_config.h`: compiler input, including static instance traversal macros.
- `generated/config.cmake`: CMake input from those same resolved values.

Unknown and duplicate symbols, malformed values, choice conflicts, violated
ranges/dependencies and failed generators stop configuration. Output replacement
is staged after successful validation. An existing bundle may remain for
inspection after failure, but CMake exits before generation or compilation.
Reconfiguration removes legacy cached `CONFIG_*` entries before loading the new
bundle. Generated string arguments are escaped for both C and CMake consumers.
Multi-config generators expose only the selected CMAKE_BUILD_TYPE in a given
build directory, so another mode cannot compile against the wrong header.
Generated headers omit wall-clock timestamps and unchanged files retain their
contents and timestamps.

The STM32F1/F4 GPIO schema originally defined the same choice symbols in unrelated
anonymous choices, producing 4,144 warnings. Giving those choices shared names
makes the schema coherent instead of hiding diagnostics. The pin generator uses
those names too.

FreeRTOS preemption and its timer daemon are mandatory backend contracts. The
schema rejects attempts to disable them. Timer daemon priority must be smaller
than the number of priority levels, checked before any bundle is written.
Native exposes its fixed millisecond tick; FreeRTOS heap and priority options
appear only for that backend. The unused main-stack and framework-debug knobs
were removed; the application linker settings own the MCU stack reservation.
SoC physical memory addresses/sizes are read-only identities that agree with the
fixed F407 linker layout, rather than ignored editable memory settings.

The single preset workflow is `scripts/ci/ci_build.py --preset ... --stage ...`.
Build/test shell, batch, Python and PowerShell entry points delegate to it. They
propagate process failures and do not infer root configuration, delete build
folders, or search for a nonexistent aggregate test binary. The old generic
preset generator was removed; configuration is verified through executable
generator/compiler/CMake consumer regressions.

`NEXUS_SOURCE_REVISION` records Git HEAD and any dirty status at configuration;
`NEXUS_CONFIG_SHA256` records the resolved configuration digest. These are
runtime diagnostics, not a digest of an uncommitted source tree. Clean matching
source tags, artifact checksums and dependency identities remain release gates.

## Dependency and platform boundaries

Google Test uses the repository-pinned `ext/googletest` submodule. Configuration
never fetches another version. A local CMake policy compatibility floor applies
only while configuring that known legacy dependency, permitting CMake 4 without
weakening project policies.

Native platform drivers are an object library so static registrations reach the
final executable. Project options and generated-header includes propagate through
`nexus_build_options`. Vendor SDKs and host-only OpenSSL are separate targets. Host builds require
OpenSSL 3 development headers/libraries; Linux/macOS file-flash simulation uses
POSIX filesystem operations and is excluded on Windows.
GD32 and other placeholder platforms fail configuration instead of creating a
successful empty library. The initial embedded reference is STM32F407 with ARM
GCC. This build identity is not hardware validation or a general STM32/GD32
support promise.

Services build as `nexus_storage`, `nexus_security`, `nexus_update`,
`nexus_modbus_rtu` and `nexus_industrial`. Native adds
`nexus_file_flash` and the maintained OpenSSL provider. MCU security has an explicit
provider contract and cannot acquire an accidental host OpenSSL dependency.

## Validation

`python scripts/kconfig/test_effective_config.py` executes real generator and
CMake configurations. Its 15 tests verify separate Debug/Release bundles, actual
C preprocessing, deterministic output, stale-cache removal, invalid input,
string escaping, failing reconfiguration, immutable required backend features,
physical memory identity and a real compiler consumer of FreeRTOS heap/priority/
timer settings. CTest registers this suite as
`effective_configuration`. Full C/Google Test and ARM link outcomes are recorded
below when executed; software builds alone do not validate timing, electrical
behavior, DMA ownership on hardware or power-loss recovery.

The configuration CLI requires a dedicated output directory:

```sh
python scripts/nexus_config.py generate --build-dir build/profile --config platforms/native/defconfig
python scripts/nexus_config.py validate --config configs/stm32f407_baremetal_defconfig
python scripts/nexus_config.py info --build-dir build/linux-gcc-debug
```

CMake always regenerates the bundle using the selected preset, so a CLI-generated
bundle is suitable for inspection and is not an alternative authoritative build
state.

## Executed validation

The final Linux GCC Debug build compiled and linked all selected Native targets
without compiler warnings. Its actual CTest run passed 1,696 of 1,696 registered
tests, with zero failures, errors or skips, in 49.93 seconds. This includes the
15 effective-configuration regressions, migrated HAL suites, 271 OSAL-labelled
registrations, 137 log-labelled registrations, the real pinned FreeRTOS POSIX
kernel, services, industrial contracts, and four finite production applications.
The report was written directly to
`build/linux-gcc-debug/ctest-results.xml`; the execution log is
`/tmp/nexus-native-final-all.log`.

The finite four-worker `freertos_demo --run-ms 500` was then added to the default
Native fragment and registered as `native_freertos_demo_smoke`. Its supplemental
CTest run passed 1/1 in 0.54 seconds, writing
`build/evidence/native-freertos-demo-final.xml`. The resulting complete graph has
1,697 registrations. The supplemental report is not copied or presented as a
rerun of the full suite; a subsequent clean source/configuration-bound build must
execute that complete graph.

ASan/UBSan passed all 23 selected registrations: 18 standalone contracts and
five finite applications, in 11.56 seconds. This includes the new production
registry, absent-platform rejection, log ownership/deadline contracts and POSIX
event cancellation helper. The report is
`build/linux-gcc-sanitizers/ctest-results.xml`, with the execution log at
`/tmp/nexus-sanitizer-final-contracts.log`. The container blocks LeakSanitizer,
so these executions used `ASAN_OPTIONS=detect_leaks=0`; CI retains leak checking.
The real pinned FreeRTOS kernel passed the ordinary POSIX-port runner; the
sanitizer result covers the standalone local event helper and does not claim
instrumentation of that kernel or validation of the ARM interrupt port.

The discovery wrapper forwards suite labels as one CTest property. MSVC-only
registration tests are built and registered only under MSVC, rather than skipped
in the Linux acceptance report. MCU queue budgets resolve through Kconfig
(1,024-byte maximum items, 8 KiB per queue), while Native permits 64 KiB per
queue. Config-manager capacities and persistence scratch space come from the
same generated configuration, with a 32-key MCU profile and 16 KiB scratch.

Storage exercised 630 interrupted writes, configuration-key rotation exercised
763 interruption points, and update/storage exercised 3,659 metadata-transition
interruption points. These are host file-flash fault-injection results.

Fresh STM32 baremetal and FreeRTOS profiles generated successfully. The final
strong platform-startup bridge and shared SysTick implementation passed four
host-GCC syntax checks against the pinned official ST/CMSIS/FreeRTOS headers;
`/tmp/nexus-final-sdk-syntax.log` records those checks. They produced no ARM
machine code. ARM GCC/newlib is unavailable in this environment, so real
Cortex-M4 compilation/linking, final MCU memory-map budgets and HIL remain
unexecuted release gates.
