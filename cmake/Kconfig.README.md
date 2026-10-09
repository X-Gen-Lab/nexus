# Build configuration contract

`cmake/Kconfig` includes only `Kconfig.build` and `Kconfig.toolchain`.

CMake presets select the compiler, build mode, tests, examples, coverage and
sanitizers. `NexusConfig.cmake` provides these decisions as required values during
one strict Kconfig resolution; a conflicting source fragment is rejected.
Kconfig defines platform resources, OSAL limits and ARM CPU/FPU/ABI values.
Target usage requirements apply the resolved hardware flags. Compiler and build
mode are never replaced by configuration generation.

Every build directory owns `generated/effective.config`, `nexus_config.h` and
`config.cmake`, generated from the same resolved state. A source-root `.config`
is not an input and a source-root generated header is not included. Unknown
symbols, duplicate definitions, dependency/choice/range contradictions and
schema warnings fail configuration. No output is replaced after failed validation.

The unused compiler/linker optimization menus, generic vendor resolver and
unmaintained commercial toolchain implementations have been removed. Native
GCC/Clang/MSVC compiler identities are recognized; the maintained MCU build path
is ARM GCC for STM32F407. Windows/macOS execution and MCU hardware evidence are
separate from Linux Native validation.

```sh
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --parallel 4
ctest --preset linux-gcc-debug --output-on-failure --no-tests=error
python scripts/nexus_config.py validate --config configs/stm32f407_baremetal_defconfig
```

See `docs/implementation/build-config.md` for executed validation and limitations.
