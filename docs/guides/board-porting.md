---
id: docs-guides-board-porting
type: how-to
audience: maintainer
source_language: en
---

(docs-guides-board-porting)=
# Add a reviewed Board package

Use this guide when the exact SoC already has a maintained provider. A Board adds
PCB facts and reviewed bindings; it does not copy a vendor SDK or authorize every
silicon route. Actual unobserved PCB revision, bridges and voltage stay unknown.

Prepare the [fixed source and recursive dependencies](source-checkout.md) before
using the resolver or build commands below.

## Gather the minimum facts

Record the full part, package, source document revision and retained provenance.
Record oscillator values, supply conditions, SWD reservations, pins, active levels,
jumper conditions and safe initial outputs. Distinguish document defaults from an
observation of your particular board. Start with one LED and one console route.

Choose a maintained fixed clock profile matching those oscillator facts. A new
Board oscillator value does not synthesize an arbitrary PLL, power, Flash
wait-state or peripheral kernel-clock plan. Add and verify an exact SoC profile
before authorizing a new combination; parsing success does not measure clocks
or establish electrical qualification.

The following is a complete maintained Board file, including `soc_family`:

```{literalinclude} ../../boards/stm32f407_qiming_v31/board.json
:language: json
:caption: Source-reviewed Qiming V3.1 facts; physical PCB revision remains unknown.
```

The [integration contract](../design/integration-contracts.md) owns
field and resource requirements. Keep your Board package in its external product
repository when it describes private PCB data. Public reusable packages live under
`boards/`; ordinary packages require only `board.json` and a README. Add bounded
neutral Board hooks only for electrical actions not expressible by maintained data.

## Validate through the actual resolver

From the Nexus repository root, inspect a known-good reference before authoring:

```sh
python tools/configure/configure.py list-bindings     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/configure/configure.py check     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/configure/configure.py explain     --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
```

For your package, select `board_package` in an external complete assembly and run
those same commands with its absolute path. Relative package paths resolve from
the authored assembly. A JSON syntax check alone cannot prove SoC identity, clock,
route or resource compatibility. Unknown fields and unreviewed routes must reject.

To create an assembly from a reviewed private package, explicitly select its
clock and binding. The following is a command template: replace package, clock
and binding with your package's reviewed facts before running it.

```sh
python tools/configure/configure.py init \
    --board-package /path/to/private-board --backend baremetal \
    --clock REVIEWED_CLOCK --profile gpio --binding REVIEWED_BINDING \
    --instance led --output /path/to/product/assembly.toml
python tools/configure/configure.py provider-check \
    --assembly /path/to/product/assembly.toml --json
```

Exactly one of `--board` and `--board-package` is required. The CLI package path
resolves from the working directory; the generated TOML stores it relative to its
own directory. The resolver rejects symlinked package paths. It allows ordinary
parent-directory references, so a private Board and product configuration can
move together without placing private facts in Nexus.

`provider-check` resolves the same typed IR and emits the real static bundle into
a temporary directory. Its report records family, part, Board, backend, selected
controllers, configuration-module paths, fact-input identities, and generated-file
hashes. Optional `--expect-family` and `--expect-part` reject an unexpected
selection. `resolved_typed_ir` and `static_bundle_emitted` are generation checks;
they do not establish compilation, instance behavior or physical qualification.
Add same-family instances through reviewed routes and the existing provider;
maintain a new family implementation only when the actual SoC requires it.

## Build and record the selected scope

Configure your external consumer using its assembly and the locked ARM toolchain.
Keep baremetal and FreeRTOS in separate output roots. Check resolved claims,
vector/startup, strong handlers, ELF/BIN/map and every physical RAM/Flash budget.
Factory lookup must be pure before start and instance state must remain isolated.
Use a software Board fixture for model/link testing; label its wiring accordingly.

Prepare [HIL plans](performance-hil.md) without equipment. Physical qualification
requires a separately reviewed station, observed Board/part/UID/probe/wiring,
exact image and real raw measurements. Source-reviewed pads never establish
connector continuity, immunity or safe electrical behavior.
