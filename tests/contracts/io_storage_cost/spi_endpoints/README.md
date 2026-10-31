# SPI endpoint allocation fixture

This link-only firmware consumes the public factory, validates every selected
endpoint against its public controller face, and uses one polling transfer loop.
Select `COST_ENDPOINTS=0`, `1`, `2` or `4`. Zero has no selected SPI controller,
endpoint face, endpoint array or SPI factory ID reference. The nonzero consumers
share the same topology-validation and transfer structure. One and two endpoints
keep a two-pointer local array; four keeps four pointers. The extra unused slot
in the one-endpoint consumer preserves the original one/two comparison shape.

All configurations select STM32F407ZGT6, Cortex-M4F hard float, baremetal,
HSE8/PLL168, `Os`, a 2048-byte main stack and no libc heap. They use the same
Flash and static RAM budgets. The zero image measures the common platform
startup/stop baseline. Nonzero images add the complete selected SPI1 transport,
generated cold resources and the consumer; their deltas are not face size alone.

The original `software-board` and `endpoints-1.toml`/`endpoints-2.toml` remain
unchanged. They describe silicon pads only: SPI1 PA5/PA6/PA7 and active-low PE4,
then PE3. The separate `software-board-four` adds the existing exact ZGT6 PG9 and
PB2 GPIO routes. It deliberately uses **three chip-select GPIO banks**, E/G/B,
in addition to the shared SPI pins on GPIOA. No PCB, device connection, voltage,
pull resistor or physical qualification is asserted by these software facts.

Build the maintained fixture with one explicit verified source tree and ARM
toolchain; there is no dedicated root preset:

```sh
cmake -S tests/contracts/io_storage_cost/spi_endpoints -B build/spi-cost-4 \
  -G Ninja -DNEXUS_SOURCE_ROOT=/absolute/nexus/source \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/nexus/source/cmake/toolchains/arm-gcc.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOST_ENDPOINTS=4
cmake --build build/spi-cost-4 --target endpoint_cost --parallel 1
```

Retain the exact source/configuration hashes, actual compile commands and
compiler identity with every ELF/BIN/map. The firmware's normal resource gate
measures LOAD, RAM and MSP/heap reservations. Apply the existing
`tools/measurement/interfaces.py`, `static_storage.inspect_storage` and
`retention.py` observers to the same ELF and generated configuration. Nonzero
images must retain both the controller and endpoint faces/tables. For zero,
record full face retention as not applicable and verify actual absence of SPI
faces, tables and provider state; never turn an empty retention report into a
pass. Static storage subsets are already included in total RAM.

Report complete-image deltas separately from endpoint state, CS state, readonly
faces, shared tables, cold pin snapshots and application code. The two-to-four
delta also activates GPIOG/GPIOB gates and leases, so it cannot establish a pure
per-endpoint linear cost. The zero-to-one delta includes the shared transport.

The earlier consumer transferred only through endpoint faces; linker collection
of its unused public controller face was legitimate. Its full face-retention
observer therefore failed. The current consumer also validates public topology
and retains that controller face/table. Preserve the old one/two images as an
endpoint-only workload, and account for this consumer/coverage difference before
comparing them with the new matrix. It is not a provider defect or proof of
regression. These link results do not measure cycles, wire timing, dynamic stack
high-water or physical behavior; no compiler stack-usage flag is required here.
