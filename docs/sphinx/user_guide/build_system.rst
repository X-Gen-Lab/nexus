Build System
============

Nexus uses CMake targets, explicit presets and one resolved configuration per
build directory. Run the commands below from the repository root. Linux Native
is the current software-validation environment; STM32F407 baremetal and FreeRTOS
profiles are embedded build candidates whose hardware evidence remains separate.

Tools and dependencies
----------------------

Use CMake 3.21 or newer, Ninja, Python 3.11 or newer, and the compiler selected by
your preset. Install the Python requirements, including ``kconfiglib``:

.. code-block:: bash

   python -m pip install -r requirements.txt

Native builds require OpenSSL 3 development headers and libraries. Host tests
use the pinned Google Test and FreeRTOS submodules:

.. code-block:: bash

   git submodule update --init ext/googletest ext/freertos

The STM32F407 profiles additionally require the pinned CMSIS and STM32F4 SDKs:

.. code-block:: bash

   git submodule update --init vendors/arm/CMSIS_5 vendors/st/cmsis_device_f4 vendors/st/stm32f4xx_hal_driver

Install the ARM GNU embedded compiler, binutils, and newlib/nano libraries for
these profiles. CMake checks the selected implementation and dependencies;
configuration does not download another dependency version.

Explicit preset workflow
------------------------

List presets available on your host, then select one explicitly:

.. code-block:: bash

   cmake --list-presets
   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --parallel 4 --output-on-failure --no-tests=error --output-junit "$PWD/build/linux-gcc-debug/ctest-results.xml"

Presets in ``CMakePresets.json`` select the compiler, build mode, platform and
input fragment. Each preset writes to ``build/<preset>/``. Debug and Release
use different directories, including on multi-config generators; one directory
owns one effective build mode.

Frequently used profiles are:

.. list-table:: Build profiles
   :header-rows: 1
   :widths: 42 58

   * - Preset
     - Purpose
   * - ``linux-gcc-debug`` / ``linux-gcc-release``
     - Native GCC applications and host tests.
   * - ``linux-clang-release``
     - Native Clang build and test variant; consult its execution evidence.
   * - ``linux-gcc-sanitizers``
     - Native ASan/UBSan contract validation; CI selects supported test targets.
   * - ``native-services-debug``
     - Native device-service example; host test suites are disabled in this profile.
   * - ``stm32-armgcc-release``
     - STM32F407, STM32F4DISCOVERY MB997, baremetal cross-compilation.
   * - ``stm32-armgcc-freertos-release``
     - The same reference board with FreeRTOS cross-compilation.

Windows/macOS presets are available for separate validation. Their existence
does not establish the same service coverage or release support as Linux.
GD32 and other platforms without an implemented BSP reject configuration.

Shared script entry point
-------------------------

``scripts/ci/ci_build.py`` invokes the same CMake/CTest presets. It requires an
explicit ``--preset`` and accepts ``--stage`` and a positive ``--jobs`` value:

.. code-block:: bash

   python scripts/ci/ci_build.py --preset linux-gcc-debug --stage all --jobs 4
   python scripts/ci/ci_build.py --preset linux-gcc-release --stage build --jobs 4
   python scripts/ci/ci_build.py --preset linux-gcc-release --stage test --jobs 4

``configure`` resolves the build. ``build`` configures and builds. ``test`` runs
an already built host preset. ``all`` configures, builds and runs tests when the
preset enables host tests. Test execution rejects zero registrations and writes
``build/<preset>/ctest-results.xml``. An embedded preset completes compilation
and reports that hardware execution needs HIL; asking it for the host ``test``
stage fails.

The convenience entry points forward to this CLI rather than implementing
another build model:

.. code-block:: bash

   sh scripts/build.sh --preset linux-gcc-debug --stage all --jobs 4
   python scripts/building/build.py --preset linux-gcc-debug --stage all --jobs 4

On Windows:

.. code-block:: bat

   scripts\build.bat --preset windows-msvc-debug --stage build --jobs 4

The shell/batch entry points default to the ``build`` stage; an explicit stage
overrides that default. The Python entry point uses the shared CLI defaults.
Use ``--help`` on the entry point for the current arguments.

Effective configuration ownership
---------------------------------

A successful CMake configuration resolves Kconfig once and writes:

.. code-block:: text

   build/<preset>/
   ├── generated/
   │   ├── effective.config     # Resolved values and defaults
   │   ├── nexus_config.h       # C/C++ compiler input
   │   └── config.cmake         # CMake values from the same resolution
   ├── CMakeCache.txt
   ├── compile_commands.json
   ├── bin/                    # Applications and executable tests
   └── lib/                    # Built libraries

``NEXUS_CONFIG_FILE`` selects an input fragment, not the final configuration.
The default Native fragment is ``platforms/native/defconfig``. The STM32 profiles
use ``configs/stm32f407_baremetal_defconfig`` or
``configs/stm32f407_freertos_defconfig``. The source-root ``.config`` and
source-root generated header are not consumed.

The preset owns the compiler and build mode. Resource and OSAL choices come from
the effective Kconfig values. ARM CPU/FPU/float ABI settings flow into target
compile/link requirements. Unknown or duplicate symbols, malformed values,
choice/range/dependency conflicts and generation failures stop configuration.
A previous bundle may remain for inspection after failure; CMake exits before
building against it. Fix the input and reconfigure the same preset.

Inspect or generate configuration explicitly:

.. code-block:: bash

   python scripts/nexus_config.py info --build-dir build/linux-gcc-debug
   python scripts/nexus_config.py validate --config configs/stm32f407_baremetal_defconfig
   python scripts/nexus_config.py generate --build-dir build/profile-inspection --config platforms/native/defconfig --set BUILD_TYPE_RELEASE=y --set TOOLCHAIN_GCC=y
   python scripts/nexus_config.py diff build/linux-gcc-debug/generated/effective.config build/linux-gcc-release/generated/effective.config

``info`` checks and prints the existing bundle paths. ``validate`` resolves a
fragment in a temporary directory. ``generate`` requires an explicit input fragment and a dedicated output
directory, and accepts repeated ``--set SYMBOL=value`` settings. ``diff`` compares
two explicitly named files. CMake regenerates the authoritative bundle during
configuration; inspection output is not a substitute for a configured build.

Product fragments and dedicated directories
------------------------------------------

Keep a product fragment under ``configs/`` and use a separate output directory
for each product/board/compiler/mode combination. A Native fragment can start
with:

.. code-block:: kconfig

   CONFIG_PLATFORM_NATIVE=y
   CONFIG_OSAL_NATIVE=y

After saving it as ``configs/product_native_defconfig``, configure explicitly:

.. code-block:: bash

   cmake -S . -B build/product-native-debug -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Debug -DNEXUS_PLATFORM=native -DNEXUS_CONFIG_FILE="$PWD/configs/product_native_defconfig"
   cmake --build build/product-native-debug --parallel 4
   ctest --test-dir build/product-native-debug --parallel 4 --output-on-failure --no-tests=error --output-junit "$PWD/build/product-native-debug/ctest-results.xml"

For shared team/CI use, add an explicit preset that inherits the appropriate
maintained compiler/profile and selects this fragment. Its ``binaryDir`` must
remain separate from another profile. Add matching build and host-test presets
when applicable so the shared script can address the product by name.

Target structure and evidence
-----------------------------

The maintained root build uses ``cmake/modules/NexusConfig.cmake`` for resolution
and ``NexusApplications.cmake`` for application startup/linker/artifact inputs.
Libraries use ordinary CMake targets. Public include directories, dependencies
and compile requirements flow through their targets and ``nexus_build_options``.
Common HAL/OSAL headers do not acquire vendor SDK or board wiring dependencies.

Native software tests exercise contracts and simulated devices. The FreeRTOS
POSIX runner uses the real pinned kernel, but it does not validate the ARM port,
interrupt timing or electrical behavior. STM32 profiles select real F407 startup,
SDK and linker inputs; cross-compiling does not establish boot, DMA ownership,
flash power-cut recovery or control deadlines on a board. Read executed results
and current blockers in ``docs/implementation/build-config.md`` and related
implementation reports rather than inferring support from a preset name.

Retain effective configuration, compile commands, test reports, target identity
and artifact hashes with an iteration or candidate. See
:doc:`../development/release_process` for the draft-candidate gates. Hardware
qualification and product promotion require their own evidence.
