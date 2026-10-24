# Explicit I/O storage allocation model

This C11 link-only fixture retains the real public `nx_bus_owner_t`,
`nx_owner_slot_t`, UART request and optional owner observer types. Select
`COST_SLOTS=1` or `4` and `COST_OBSERVER=0` or `1`. Each selected slot also retains
one request and an explicit 32-byte caller payload, reported separately. The
constant five-word address roots keep actual symbols linked without guessing
their sizes or executing an operation. They are measurement overhead, not a
platform requirement.

Use the maintained ARM GNU toolchain with the selected CPU/FPU/float and enum
ABI, `-Os -ffunction-sections -fdata-sections`, the Core/I/O/OS/BusOwner/
OwnerObserver include directories, `-nostdlib -Wl,--gc-sections`, and the existing
[`os_wait_cost/linker.ld`](../os_wait_cost/linker.ld). Read sizes and addresses
from each resulting ELF symbol/section table, retaining exact commands, compiler
identity, input hashes, actual exit codes and all four original ELF files.
Compare slot-array, request-array, payload, observer, owner and anchor sizes
individually; section padding is separate from these object subsets.

This fixture does not supply MCU startup, Board routing, operational callbacks,
IRQ handling, a scheduler or a runnable product. It establishes linked allocation
cost for an explicitly selected ABI only. It cannot prove throughput, latency,
dynamic stack high-water, resource ownership, physical HIL or complete transport
cost. Instance/controller/mode costs use the actual generated firmware matrix;
an eight-byte interface face is never a substitute for that complete image.

For a public observation layout change, compile an additional size-only object
against the exact archived old header and the same flags, preserving both source
identities. Rebuild consumers even when added fields happen to fit old padding;
equal size does not establish ABI compatibility or zero cost on another ABI.

`spi_endpoints/` separately links one and two endpoints on the same SPI1
controller, using the public factory and identical polling transfer loop. Its
Board facts preserve the external shared-SPI example's explicit software-only
PA5/PA6/PA7 and PE4/PE3 pad contract; they do not establish first-release PCB
connections. Both configurations use the same baremetal backend, clock, CPU,
optimization, stack and heap. Only the second endpoint selection and consumer
count differ. Build the CMake fixture with `NEXUS_SOURCE_ROOT`, the production
ARM toolchain and `COST_ENDPOINTS=1` or `2`, then apply the existing resource,
static-storage, interface and retention observers to each actual ELF. Report
the complete image delta separately from endpoint state, face and shared table
subsets; any consumer instruction delta remains part of the complete image.
