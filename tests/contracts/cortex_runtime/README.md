# CPU software runtime link fixture

This fixture consumes `Nexus::Runtime` through the production CMake entry point
and one authored runtime TOML. It retains the Core, Arch and selected OS APIs,
actual kernel exception handlers and enabled floating point or MVE probes.

Its ROM/RAM addresses and clock are synthetic software references. It has no
SoC startup, reset vector, clocks, board wiring or physical qualification and
must not be used as a flashable board firmware. Compiled context instructions
and linked API bodies establish code generation and dependency closure only.

Run the complete maintained compiler/link matrix with:

```sh
python scripts/ci/cortex_runtime.py \
  --compiler /path/to/arm-none-eabi-gcc --output /tmp/cortex-runtime-evidence
```

The script writes each CPU facts JSON and runtime TOML, configures this fixture
with the normal toolchain, and preserves command logs, configuration identities,
ELF/object hashes, undefined symbols and instruction audit results.
