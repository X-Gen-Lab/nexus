# Mapped-storage link fixture

This fixture links `Nexus::MappedStorage` through the production F407 and GD470
platform target graphs. Its callbacks are test-local doubles. It does not
implement a serial-memory controller, authorize a PCB route, run a device, or
establish physical XIP/cache/DMA timing. `Nexus::NativeMappedStorage` is excluded
from both firmware images.

The face, shared table, exact caller state and two map slots remain measurable in
the ELF. Configure with the ordinary ARM toolchain and an explicit existing
`sky-baremetal.toml` or `liangshan-baremetal.toml` assembly. No product Flash
reservation is made. Host behavior is covered by `nexus_mapped_storage_test`.
