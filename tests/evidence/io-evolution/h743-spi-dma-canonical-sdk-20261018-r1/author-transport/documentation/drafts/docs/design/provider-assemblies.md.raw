# Selected provider assembly gate

The maintained gate links explicitly authored software fixtures through their
existing CMake projects. TOML and the sole configuration resolver select the
exact part, Board package, backend, routes, modes and budgets. The gate does not
rewrite a backend, create a provider registry, select source targets or infer
physical Board wiring.

```sh
python scripts/ci/provider_assemblies.py \
    --fixture tests/contracts/usb_assembly/stm32-zg.toml \
    --family stm32f407 \
    --compiler arm-none-eabi-gcc \
    --output build/provider-assemblies/usb-stm32
```

Repeat `--fixture` for a reviewed list of authored TOMLs. Without `--project`,
each TOML must remain in `tests/contracts` with its maintained sibling
`CMakeLists.txt`. An explicit `--project` instead binds the whole invocation to
one existing CMake project under the checkout's `tests/contracts`; its TOMLs
may be owned contract fixtures or canonical schema-2 assemblies directly in
`tools/configure/assemblies`. Each selection must resolve to an MCU configuration.
The gate does not discover a project from the family or silently change modes.

For the three public H743 SPI4 DMA assemblies, run the existing H743 project
from the repository root with the maintained ARM compiler on `PATH`:

```sh
python scripts/ci/provider_assemblies.py \
    --project tests/contracts/h743_assembly \
    --fixture tools/configure/assemblies/h743-spi-dma.toml \
    --fixture tools/configure/assemblies/h743-spi-dma-uart-tx.toml \
    --fixture tools/configure/assemblies/h743-spi-dma-uart-rx.toml \
    --family stm32h743 \
    --compiler arm-none-eabi-gcc \
    --output build/provider-assemblies/h743-spi-dma
```

The output must be fresh; success requires every selected firmware build,
resource report and retained provider method to pass. The command links ARM
software and preserves its report and raw output; it does not execute the MCU.
The complete H743 provider stage selects the existing 26 contract assemblies
plus these three canonical assemblies through the same project. A narrow
three-fixture invocation is not the complete 29-fixture stage.

The optional family assertion compares the actual resolver result. Different
backends require distinct maintained authored TOMLs; there is no independent
OS override. CMake builds the selected project's default target graph using
`NEXUS_SOURCE_ROOT` and `NEXUS_ASSEMBLY_FILE`. Native selections, outside or
symlinked projects, duplicate fixtures and repeated project selection are
refused. The selected source root must own the executed gate and its imported
tools; tools from another checkout are refused.

The output directory must be absent. Existing evidence, symlink destinations
and source directories are refused. Every actual command records its argv,
working directory, timestamps, return code and original stdout/stderr bytes.
Missing tools, nonzero exits, timeouts and background artifact writers fail
the matrix and preserve its failed report. An empty or duplicate matrix cannot
count as successful work.

After each real build, the gate requires exactly one firmware ELF and one
completed generated bundle. It compares the built canonical configuration
digest against the original resolved authored input, rechecks every declared
input, and recomputes memory allocation using the existing ELF resource tool.
The original post-link report must match the recomputation. Flash LOAD bytes,
Flash footprint, each physical RAM domain, MSP and heap retain their existing
budget semantics.

For selected I/O, the existing retention tool independently follows actual
face pointers, concrete operations tables and every non-NULL method in ELF
LOAD bytes. Its post-link report must match the recomputation. A deliberately
empty fixture instead proves zero authored controllers/devices, zero generated
face declarations and absence of concrete I/O provider tables and faces from
the actual ELF. This is an executed empty-selection GC check, not a skipped
retention check or a claim that all unrelated runtime code is absent.

The actual compiler database and Ninja dependency log bind compiled sources,
generated headers, selected vendor headers and compiler-provided headers to
file identities. Maintained graph/configuration sources and selected locked
SDK input bytes are snapshotted before execution and verified afterward. The
explicit project, its CMake inputs and the selected authored assemblies are
part of that snapshot. Changing the project's source or any declared assembly
invalidates the evidence. The matrix rejects source, Git revision, tool or
artifact changes. A dirty checkout
may provide explicitly identified development evidence; it cannot qualify a
clean release candidate through this gate alone.

`report.json` records `selected_provider_assembly_matrix`, actual fixture and
command counts, source identities, tool identities, per-fixture configuration
digests, ELF/BIN/map/bundle/report identities, RAM domains and retained methods.
All successful fixtures must pass; there is no skipped-pass result. CI owns its
explicit fixture list and uploads original commands and artifacts alongside
the report.

This gate establishes software graph, compile/link, budget and selected
provider retention evidence. It does not execute MCU startup, interrupts, DMA,
USB enumeration, Ethernet traffic, timer waveforms or electrical controls.
Physical status remains `not_executed`; cycles and latency remain
`not_measured`. Model tests and eventual HIL qualification are separate scopes.

Tool regression tests run with:

```sh
python -m unittest scripts.ci.test_provider_assemblies -v
```
